#include "stdafx.h"

#include "ReplayEngine.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <memory>
#include <regex>
#include <sstream>
#include <thread>

#include <json/json.h>

#include "MyFakeLogger.h"

namespace
{

const char *const SERVER_EVENT_PATH = "/abk/events/server_event";
const char *const VAR_LIST_PATH = "/abk/variables/var_list";

std::string QuoteShellArg(const std::string &s)
{
  std::string out("'");
  for(char c : s)
  {
    if(c == '\'')
      out += "'\\''";
    else
      out.push_back(c);
  }
  out.push_back('\'');
  return out;
}

bool IsJsonNumberToken(const std::string &t)
{
  if(t.empty() || t == "null" || t == "nan" || t == "NaN")
    return false;
  char *pEnd = nullptr;
  errno = 0;
  std::strtod(t.c_str(), &pEnd);
  return (errno == 0 && pEnd && *pEnd == '\0');
}

bool ParseJson(const std::string &text, Json::Value &out)
{
  Json::CharReaderBuilder builder;
  builder["collectComments"] = false;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  std::string errs;
  return reader->parse(text.data(), text.data() + text.size(), &out, &errs);
}

double JsonToDouble(const Json::Value &v)
{
  if(v.isNumeric())
    return v.asDouble();
  if(v.isBool())
    return v.asBool() ? 1.0 : 0.0;
  return std::numeric_limits<double>::quiet_NaN();
}

void StripSessionId(Json::Value &v)
{
  if(v.isObject())
  {
    for(const std::string &name : v.getMemberNames())
    {
      if(name == "SessionId")
        v.removeMember(name);
      else
        StripSessionId(v[name]);
    }
  }
  else if(v.isArray())
  {
    for(Json::Value &e : v)
      StripSessionId(e);
  }
}

} // namespace

CPcapReplayEngine::CPcapReplayEngine()
  : m_fallbackVarCount(0),
    m_speed(1.0),
    m_firstFrameTs(0.0)
{
}

bool CPcapReplayEngine::LoadFromPcapng(const std::string &pcapPath, std::string &err)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_pcapPath = pcapPath;
  m_varNames.clear();
  m_frames.clear();
  m_exchanges.clear();
  m_sessionStates.clear();
  m_fallbackVarCount = 0;
  m_firstFrameTs = 0.0;

  if(!ExtractTraffic(err))
    return false;

  FinalizeVariableModel();
  if(!m_frames.empty())
    m_firstFrameTs = m_frames.front().timestampSec;

  return true;
}

void CPcapReplayEngine::SetPlaybackSpeed(double speed)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if(speed > 0.0)
    m_speed = speed;
}

CPcapReplayEngine::SessionReplayState &CPcapReplayEngine::GetOrCreateSessionState(int sessionId)
{
  return m_sessionStates[sessionId];
}

const CPcapReplayEngine::SessionReplayState *CPcapReplayEngine::FindSessionState(int sessionId) const
{
  const auto it = m_sessionStates.find(sessionId);
  if(it == m_sessionStates.end())
    return nullptr;
  return &it->second;
}

void CPcapReplayEngine::SetSessionEstablished(int sessionId, bool established)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  SessionReplayState &state = GetOrCreateSessionState(sessionId);
  state.sessionEstablished = established;
  if(!established)
  {
    state.started = false;
    state.frameIndex = 0;
    state.currentValues.clear();
  }
}

bool CPcapReplayEngine::IsSessionEstablished(int sessionId) const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  const SessionReplayState *pState = FindSessionState(sessionId);
  return pState && pState->sessionEstablished;
}

void CPcapReplayEngine::StartAfterSessionEstablished(int sessionId)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  SessionReplayState &state = GetOrCreateSessionState(sessionId);
  if(!state.sessionEstablished || state.started)
    return;
  state.started = true;
  state.frameIndex = 0;
  state.startWall = std::chrono::steady_clock::now();
}

std::string CPcapReplayEngine::NextServerEventResponse(int sessionId, ReplayEventInfo *pInfo)
{
  std::size_t frameCount = 0;
  double frameTs = 0.0;
  double firstTs = 0.0;
  double speed = 1.0;
  std::chrono::steady_clock::time_point startWall;

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if(m_frames.empty())
      return "{\"DataLists\":{},\"Events\":[]}";

    SessionReplayState &state = GetOrCreateSessionState(sessionId);
    frameCount = m_frames.size();

    if(!state.started)
    {
      state.started = true;
      state.startWall = std::chrono::steady_clock::now();
      state.frameIndex = 0;
    }

    const std::size_t frameIndex = std::min(state.frameIndex, m_frames.size() - 1);
    frameTs = m_frames[frameIndex].timestampSec;
    firstTs = m_firstFrameTs;
    speed = m_speed;
    startWall = state.startWall;
  }

  {
    const double deltaSec = (frameTs - firstTs) / speed;
    const auto targetTime = startWall + std::chrono::milliseconds((long long)std::llround(deltaSec * 1000.0));
    const auto now = std::chrono::steady_clock::now();
    if(targetTime > now)
      std::this_thread::sleep_for(targetTime - now);
  }

  std::lock_guard<std::mutex> lock(m_mutex);
  SessionReplayState &state = GetOrCreateSessionState(sessionId);
  ReplayFrame frame;
  if(state.frameIndex < m_frames.size())
    frame = m_frames[state.frameIndex++];
  else
    frame = m_frames.back();

  if(!frame.values.empty())
    state.currentValues = frame.values;

  if(pInfo)
  {
    pInfo->packetCount = frameCount;
    pInfo->packetIndex = std::min<std::size_t>(state.frameIndex, frameCount);
    pInfo->progress = (frameCount > 0) ? ((double)pInfo->packetIndex / (double)frameCount) : 0.0;
    pInfo->eventType = frame.eventType;
  }

  // workaround for truncated packets: synthesize a valid response from the last known values
  if(frame.truncated || frame.body.empty())
    return std::string("{\"DataLists\":{\"ReplayList\":") + BuildDataArrayJson(state.currentValues) + "},\"Events\":[]}";

  return frame.body;
}

bool CPcapReplayEngine::FindRecordedResponse(const std::string &method,
                                             const std::string &path,
                                             const std::string &query,
                                             const std::string &requestBody,
                                             std::string &responseOut,
                                             int &statusOut)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  const auto it = m_exchanges.find(MakeKey(method, path, query));
  if(it == m_exchanges.end() || it->second.exchanges.empty())
    return false;

  ExchangeGroup &group = it->second;
  const std::string canonical = CanonicalBody(requestBody);

  // 1. exact match on the canonical request body (cycle through repeated identical requests)
  std::vector<std::size_t> candidates;
  for(std::size_t i = 0; i < group.exchanges.size(); ++i)
  {
    if(group.exchanges[i].canonicalBody == canonical)
      candidates.push_back(i);
  }

  const RecordedExchange *pMatch = nullptr;
  if(!candidates.empty())
  {
    std::size_t &cursor = group.cursorByBody[canonical];
    pMatch = &group.exchanges[candidates[cursor % candidates.size()]];
    ++cursor;
  }
  else
  {
    // 2. fall back to any response recorded for the same method/path/query
    pMatch = &group.exchanges[group.cursorAny % group.exchanges.size()];
    ++group.cursorAny;
  }

  responseOut = pMatch->responseBody;
  statusOut = pMatch->statusCode;
  return true;
}

int CPcapReplayEngine::GetEstimatedVarCount() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if(!m_varNames.empty())
    return (int)m_varNames.size();
  return m_fallbackVarCount;
}

const std::vector<std::string> &CPcapReplayEngine::GetVariableNames() const
{
  return m_varNames;
}

void CPcapReplayEngine::ApplyCurrentValuesToLogger(int sessionId, CMyFakeLogger &logger) const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  const SessionReplayState *pState = FindSessionState(sessionId);
  if(!pState)
    return;

  const int count = std::min((int)pState->currentValues.size(), logger.GetVarCount());
  for(int i = 0; i < count; ++i)
    logger.SetVariableValue(i, pState->currentValues[(std::size_t)i]);
}

bool CPcapReplayEngine::ExtractTraffic(std::string &err)
{
  std::string requestsOut;
  std::string responsesOut;
  std::string cmdErr;

  const std::string quotedPath = QuoteShellArg(m_pcapPath);

  const std::string reqCmd =
    "tshark -r " + quotedPath +
    " -Y 'http.request' -T fields -E separator='|'"
    " -e frame.number -e http.request.method -e http.request.uri -e http.file_data";

  if(!RunCommand(reqCmd, requestsOut, cmdErr))
  {
    err = "Failed to parse HTTP requests from pcap with tshark: " + cmdErr;
    return false;
  }

  struct RequestInfo
  {
    std::string method;
    std::string uri;
    std::string body;
  };
  std::unordered_map<std::string, RequestInfo> requestByFrame;
  for(const std::string &line : SplitLines(requestsOut))
  {
    if(line.empty())
      continue;
    const std::vector<std::string> f = SplitFields(line, '|', 4);
    if(f.size() < 3)
      continue;
    RequestInfo info;
    info.method = Trim(f[1]);
    info.uri = Trim(f[2]);
    if(f.size() >= 4)
      info.body = DecodeHexPayload(f[3]);
    requestByFrame[Trim(f[0])] = info;
  }

  const std::string rspCmd =
    "tshark -r " + quotedPath +
    " -Y 'http.response && http.request_in' -T fields -E separator='|'"
    " -e frame.time_epoch -e http.request_in -e http.response.code -e http.file_data";

  if(!RunCommand(rspCmd, responsesOut, cmdErr))
  {
    err = "Failed to parse HTTP responses from pcap with tshark: " + cmdErr;
    return false;
  }

  for(const std::string &line : SplitLines(responsesOut))
  {
    if(line.empty())
      continue;
    const std::vector<std::string> f = SplitFields(line, '|', 4);
    if(f.size() < 3)
      continue;

    const auto itReq = requestByFrame.find(Trim(f[1]));
    if(itReq == requestByFrame.end() || itReq->second.uri.empty())
      continue;
    const RequestInfo &req = itReq->second;

    const double ts = std::atof(Trim(f[0]).c_str());
    int status = std::atoi(Trim(f[2]).c_str());
    if(status <= 0)
      status = 200;
    const std::string body = (f.size() >= 4) ? DecodeHexPayload(f[3]) : std::string();

    std::string path;
    std::string query;
    SplitUri(req.uri, path, query);

    if(path == SERVER_EVENT_PATH)
    {
      AddServerEventFrame(ts, body);
      continue;
    }

    if(path == VAR_LIST_PATH)
    {
      std::vector<std::string> vars;
      if(ParseVarListFromBody(body, vars))
        m_varNames = vars;
    }

    RecordedExchange ex;
    ex.timestampSec = ts;
    ex.canonicalBody = CanonicalBody(req.body);
    ex.responseBody = body;
    ex.statusCode = status;
    m_exchanges[MakeKey(req.method, path, query)].exchanges.push_back(ex);
  }

  if(m_frames.empty())
  {
    err = "No /abk/events/server_event responses found in capture";
    return false;
  }

  std::sort(m_frames.begin(), m_frames.end(), [](const ReplayFrame &a, const ReplayFrame &b) {
    return a.timestampSec < b.timestampSec;
  });
  for(auto &itGroup : m_exchanges)
  {
    std::stable_sort(itGroup.second.exchanges.begin(), itGroup.second.exchanges.end(),
                     [](const RecordedExchange &a, const RecordedExchange &b) { return a.timestampSec < b.timestampSec; });
  }

  return true;
}

void CPcapReplayEngine::AddServerEventFrame(double ts, const std::string &body)
{
  ReplayFrame frame;
  frame.timestampSec = ts;
  frame.body = body;
  frame.truncated = false;

  if(!ParseServerEventJson(body, frame.values, frame.eventType))
  {
    // truncated packet: read the JSON manually as far as it goes
    frame.truncated = true;
    frame.values = ParseFirstDataListValuesLoose(body);
    frame.eventType = ParseEventTypeLoose(body);
  }

  if(frame.values.size() > (std::size_t)m_fallbackVarCount)
    m_fallbackVarCount = (int)frame.values.size();

  m_frames.push_back(frame);
}

void CPcapReplayEngine::FinalizeVariableModel()
{
  if(!m_varNames.empty())
    return;

  int count = m_fallbackVarCount;
  if(count <= 0)
    count = 10;

  m_varNames.reserve((std::size_t)count);
  for(int i = 0; i < count; ++i)
  {
    std::ostringstream n;
    n << "ReplayVar_" << std::setw(4) << std::setfill('0') << i;
    m_varNames.push_back(n.str());
  }
}

std::string CPcapReplayEngine::MakeKey(const std::string &method, const std::string &path, const std::string &query)
{
  std::string m = method;
  std::transform(m.begin(), m.end(), m.begin(), [](unsigned char c) { return (char)std::toupper(c); });
  return m + ' ' + path + '?' + CanonicalQuery(query);
}

std::string CPcapReplayEngine::CanonicalQuery(const std::string &query)
{
  std::vector<std::string> params;
  std::size_t pos = 0;
  while(pos <= query.size())
  {
    std::size_t end = query.find('&', pos);
    if(end == std::string::npos)
      end = query.size();
    const std::string p = query.substr(pos, end - pos);
    if(!p.empty())
    {
      const std::string name = p.substr(0, p.find('='));
      std::string lower = name;
      std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return (char)std::tolower(c); });
      if(lower != "sessionid") // session ids differ between capture and replay
        params.push_back(p);
    }
    pos = end + 1;
  }
  std::sort(params.begin(), params.end());

  std::string out;
  for(std::size_t i = 0; i < params.size(); ++i)
  {
    if(i)
      out.push_back('&');
    out += params[i];
  }
  return out;
}

std::string CPcapReplayEngine::CanonicalBody(const std::string &body)
{
  const std::string trimmed = Trim(body);
  if(trimmed.empty())
    return std::string();

  Json::Value root;
  if(!ParseJson(trimmed, root))
    return trimmed;

  StripSessionId(root);
  Json::StreamWriterBuilder writer;
  writer["indentation"] = "";
  return Json::writeString(writer, root); // object keys are emitted sorted
}

void CPcapReplayEngine::SplitUri(const std::string &uri, std::string &path, std::string &query)
{
  std::string u = uri;
  // strip scheme/host if tshark reports an absolute URI
  const std::size_t pScheme = u.find("://");
  if(pScheme != std::string::npos)
  {
    const std::size_t pPath = u.find('/', pScheme + 3);
    u = (pPath == std::string::npos) ? std::string("/") : u.substr(pPath);
  }

  const std::size_t q = u.find('?');
  if(q == std::string::npos)
  {
    path = u;
    query.clear();
  }
  else
  {
    path = u.substr(0, q);
    query = u.substr(q + 1);
  }
}

bool CPcapReplayEngine::RunCommand(const std::string &cmd, std::string &output, std::string &err)
{
  output.clear();
  err.clear();

#if defined(_WIN32)
  FILE *pipe = _popen(cmd.c_str(), "r");
#else
  FILE *pipe = popen(cmd.c_str(), "r");
#endif
  if(!pipe)
  {
    err = "popen failed";
    return false;
  }

  char buffer[4096];
  while(std::fgets(buffer, (int)sizeof(buffer), pipe) != nullptr)
    output.append(buffer);

#if defined(_WIN32)
  const int rc = _pclose(pipe);
#else
  const int rc = pclose(pipe);
#endif

  if(rc != 0)
  {
    std::ostringstream s;
    s << "command failed (" << rc << "): " << cmd;
    err = s.str();
    return false;
  }

  return true;
}

std::vector<std::string> CPcapReplayEngine::SplitLines(const std::string &text)
{
  std::vector<std::string> out;
  std::size_t pos = 0;
  while(pos < text.size())
  {
    const std::size_t end = text.find('\n', pos);
    if(end == std::string::npos)
    {
      out.push_back(text.substr(pos));
      break;
    }
    out.push_back(text.substr(pos, end - pos));
    pos = end + 1;
  }
  return out;
}

std::vector<std::string> CPcapReplayEngine::SplitFields(const std::string &line, char sep, std::size_t maxParts)
{
  std::vector<std::string> out;
  std::size_t pos = 0;
  while(out.size() + 1 < maxParts)
  {
    const std::size_t end = line.find(sep, pos);
    if(end == std::string::npos)
      break;
    out.push_back(line.substr(pos, end - pos));
    pos = end + 1;
  }
  out.push_back(line.substr(pos));
  return out;
}

std::string CPcapReplayEngine::DecodeHexPayload(const std::string &hexText)
{
  std::string hex;
  hex.reserve(hexText.size());
  for(char c : hexText)
  {
    if(std::isxdigit((unsigned char)c))
      hex.push_back((char)std::tolower((unsigned char)c));
  }

  if((hex.size() & 1U) != 0)
    hex.pop_back();

  std::string out;
  out.reserve(hex.size() / 2U);
  for(std::size_t i = 0; i + 1 < hex.size(); i += 2)
  {
    const char byteStr[3] = {hex[i], hex[i + 1], '\0'};
    out.push_back((char)std::strtol(byteStr, nullptr, 16));
  }
  return out;
}

std::string CPcapReplayEngine::Trim(const std::string &s)
{
  std::size_t b = 0;
  while(b < s.size() && std::isspace((unsigned char)s[b]))
    ++b;
  std::size_t e = s.size();
  while(e > b && std::isspace((unsigned char)s[e - 1]))
    --e;
  return s.substr(b, e - b);
}

bool CPcapReplayEngine::ParseVarListFromBody(const std::string &body, std::vector<std::string> &varsOut)
{
  varsOut.clear();
  Json::Value root;
  if(!ParseJson(body, root) || !root.isObject())
    return false;

  const Json::Value &list = root["VarList"];
  if(!list.isArray())
    return false;

  for(const Json::Value &v : list)
  {
    if(v.isString())
      varsOut.push_back(v.asString());
    else if(v.isObject() && v["Name"].isString())
      varsOut.push_back(v["Name"].asString());
  }
  return !varsOut.empty();
}

bool CPcapReplayEngine::ParseServerEventJson(const std::string &body, std::vector<double> &valuesOut, std::string &eventTypeOut)
{
  valuesOut.clear();
  eventTypeOut.clear();

  Json::Value root;
  if(!ParseJson(body, root) || !root.isObject())
    return false;

  const Json::Value &dataLists = root["DataLists"];
  if(dataLists.isObject() && !dataLists.empty())
  {
    // first data list; if it holds several samples (array of arrays) use the latest one
    const Json::Value &list = dataLists[dataLists.getMemberNames().front()];
    const Json::Value *pSample = &list;
    if(list.isArray() && !list.empty() && list[list.size() - 1].isArray())
      pSample = &list[list.size() - 1];
    if(pSample->isArray())
    {
      for(const Json::Value &v : *pSample)
        valuesOut.push_back(JsonToDouble(v));
    }
  }

  const Json::Value &events = root["Events"];
  if(events.isArray())
  {
    for(const Json::Value &e : events)
    {
      if(e.isObject() && e["EventType"].isString())
      {
        eventTypeOut = e["EventType"].asString();
        break;
      }
    }
  }

  return true;
}

std::vector<double> CPcapReplayEngine::ParseFirstDataListValuesLoose(const std::string &body)
{
  std::vector<double> out;
  const std::size_t pDataLists = body.find("\"DataLists\"");
  if(pDataLists == std::string::npos)
    return out;

  const std::size_t pArray = body.find('[', pDataLists);
  if(pArray == std::string::npos)
    return out;

  bool inStr = false;
  bool escape = false;
  int nested = 0;
  std::string token;

  auto flushToken = [&out, &token]() {
    const std::string t = Trim(token);
    if(IsJsonNumberToken(t))
      out.push_back(std::strtod(t.c_str(), nullptr));
    else if(!t.empty())
      out.push_back(std::numeric_limits<double>::quiet_NaN());
    token.clear();
  };

  for(std::size_t i = pArray + 1; i < body.size(); ++i)
  {
    const char c = body[i];
    if(inStr)
    {
      if(escape)
        escape = false;
      else if(c == '\\')
        escape = true;
      else if(c == '"')
        inStr = false;
      token.push_back(c);
      continue;
    }

    if(c == '"')
    {
      inStr = true;
      token.push_back(c);
    }
    else if(c == '[' || c == '{')
    {
      nested++;
      token.push_back(c);
    }
    else if(c == ']' || c == '}')
    {
      if(nested == 0)
      {
        flushToken();
        break;
      }
      nested--;
      token.push_back(c);
    }
    else if(c == ',' && nested == 0)
    {
      flushToken();
    }
    else
    {
      token.push_back(c);
    }
  }

  // a token cut off at the end of a truncated packet is unreliable and therefore dropped
  return out;
}

std::string CPcapReplayEngine::ParseEventTypeLoose(const std::string &body)
{
  std::smatch m;
  static const std::regex reEventType("\\\"EventType\\\"\\s*:\\s*\\\"((?:\\\\.|[^\\\"\\\\])*)\\\"");
  if(std::regex_search(body, m, reEventType) && m.size() > 1)
    return m[1].str();
  return std::string();
}

std::string CPcapReplayEngine::BuildDataArrayJson(const std::vector<double> &values)
{
  std::ostringstream out;
  out << '[';
  for(std::size_t i = 0; i < values.size(); ++i)
  {
    if(i)
      out << ',';
    if(std::isnan(values[i]))
      out << "null";
    else
      out << std::setprecision(15) << values[i];
  }
  out << ']';
  return out.str();
}
