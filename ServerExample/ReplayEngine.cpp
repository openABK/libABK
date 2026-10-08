#include "stdafx.h"

#include "ReplayEngine.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <regex>
#include <sstream>
#include <thread>

#include "MyFakeLogger.h"

namespace
{

static std::string QuoteShellArg(const std::string &s)
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

static std::string TrimCopy(const std::string &s)
{
  std::size_t b = 0;
  while(b < s.size() && std::isspace((unsigned char)s[b]))
    ++b;
  std::size_t e = s.size();
  while(e > b && std::isspace((unsigned char)s[e - 1]))
    --e;
  return s.substr(b, e - b);
}

static bool IsJsonNumberToken(const std::string &token)
{
  const std::string t = TrimCopy(token);
  if(t.empty())
    return false;
  if(t == "null" || t == "nan" || t == "NaN")
    return false;
  char *pEnd = nullptr;
  errno = 0;
  std::strtod(t.c_str(), &pEnd);
  return (errno == 0 && pEnd && *pEnd == '\0');
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
  m_varIndex.clear();
  m_frames.clear();
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
  if(!state.sessionEstablished)
    return;
  if(state.started)
    return;
  state.started = true;
  state.frameIndex = 0;
  state.startWall = std::chrono::steady_clock::now();
}

std::string CPcapReplayEngine::NextServerEventResponse(int sessionId, ReplayEventInfo *pInfo)
{
  std::size_t frameCount = 0;
  ReplayFrame frame;
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

    std::size_t frameIndex = state.frameIndex;
    if(frameIndex >= m_frames.size())
      frameIndex = m_frames.size() - 1;

    frame = m_frames[frameIndex];
    firstTs = m_firstFrameTs;
    speed = m_speed;
    startWall = state.startWall;
  }

  {
    const double deltaSec = (frame.timestampSec - firstTs) / speed;
    const auto targetTime = startWall + std::chrono::milliseconds((long long)std::llround(deltaSec * 1000.0));
    const auto now = std::chrono::steady_clock::now();
    if(targetTime > now)
      std::this_thread::sleep_for(targetTime - now);
  }

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    SessionReplayState &state = GetOrCreateSessionState(sessionId);
    if(state.frameIndex < m_frames.size())
    {
      frame = m_frames[state.frameIndex];
      state.frameIndex++;
    }
    else
    {
      frame = m_frames.back();
    }

    if(!frame.values.empty())
      state.currentValues = frame.values;

    if(pInfo)
    {
      pInfo->packetCount = frameCount;
      pInfo->packetIndex = std::min<std::size_t>(state.frameIndex, frameCount);
      pInfo->progress = (frameCount > 0) ? ((double)pInfo->packetIndex / (double)frameCount) : 0.0;
      pInfo->eventType.clear();
      std::smatch m;
      static const std::regex reEventType("\\\"EventType\\\"\\s*:\\s*\\\"((?:\\\\.|[^\\\"\\\\])*)\\\"");
      if(std::regex_search(frame.body, m, reEventType) && m.size() > 1)
        pInfo->eventType = m[1].str();
    }

    if(frame.body.empty())
      return std::string("{\"DataLists\":{\"ReplayList\":") + BuildDataArrayJson(state.currentValues) + "},\"Events\":[]}";

    const bool likelyTruncated = frame.body.find("DataLists") != std::string::npos && frame.body.find('}') == std::string::npos;
    if(likelyTruncated)
      return std::string("{\"DataLists\":{\"ReplayList\":") + BuildDataArrayJson(state.currentValues) + "},\"Events\":[]}";

    return frame.body;
  }
}

std::string CPcapReplayEngine::BuildVarListResponse() const
{
  std::lock_guard<std::mutex> lock(m_mutex);

  std::ostringstream out;
  out << "{\"VarList\":[";
  for(std::size_t i = 0; i < m_varNames.size(); ++i)
  {
    if(i)
      out << ',';
    out << '"' << EscapeJson(m_varNames[i]) << '"';
  }
  out << "]}";
  return out.str();
}

bool CPcapReplayEngine::RegisterDaqList(const std::string &requestBody, std::string &listNameOut, std::string &errOut)
{
  std::smatch m;
  static const std::regex reName("\\\"Name\\\"\\s*:\\s*\\\"((?:\\\\.|[^\\\"\\\\])*)\\\"", std::regex::icase);
  if(!std::regex_search(requestBody, m, reName))
  {
    errOut = "DAQ list request missing Name field";
    return false;
  }

  listNameOut = m[1].str();

  std::vector<std::string> names;
  static const std::regex reArray("\\\"DaqList\\\"\\s*:\\s*\\[(.*?)\\]", std::regex::icase);
  if(std::regex_search(requestBody, m, reArray))
  {
    const std::string arr = m[1].str();
    static const std::regex reStr("\\\"((?:\\\\.|[^\\\"\\\\])*)\\\"");
    auto it = std::sregex_iterator(arr.begin(), arr.end(), reStr);
    auto end = std::sregex_iterator();
    for(; it != end; ++it)
    {
      std::string name = (*it)[1].str();
      if(name.empty())
        name = "ReplayVar_" + std::to_string(names.size());
      names.push_back(name);
    }
  }

  if(names.empty())
  {
    const std::size_t baseCount = std::max<std::size_t>(m_varNames.size(), 1U);
    names.reserve(baseCount);
    for(std::size_t i = 0; i < baseCount; ++i)
    {
      if(i < m_varNames.size() && !m_varNames[i].empty())
        names.push_back(m_varNames[i]);
      else
        names.push_back("ReplayVar_" + std::to_string(i));
    }
  }

  std::lock_guard<std::mutex> lock(m_mutex);
  m_varNames = names;
  m_varIndex.clear();
  for(std::size_t i = 0; i < m_varNames.size(); ++i)
    m_varIndex[m_varNames[i]] = i;
  m_createdDaqLists[listNameOut] = names;
  for(auto &itSession : m_sessionStates)
  {
    if(itSession.second.currentValues.size() < m_varNames.size())
      itSession.second.currentValues.resize(m_varNames.size(), std::numeric_limits<double>::quiet_NaN());
  }
  return true;
}

std::string CPcapReplayEngine::BuildVarValueResponse(int sessionId, const std::string &requestBody) const
{
  std::lock_guard<std::mutex> lock(m_mutex);

  const SessionReplayState *pState = FindSessionState(sessionId);
  const std::vector<double> *pValues = pState ? &pState->currentValues : nullptr;

  const std::vector<std::string> requested = ParseGetListNamesLoose(requestBody);
  std::ostringstream out;
  out << "{\"Data\":[";
  for(std::size_t i = 0; i < requested.size(); ++i)
  {
    if(i)
      out << ',';
    const auto it = m_varIndex.find(requested[i]);
    if(!pValues || it == m_varIndex.end() || it->second >= pValues->size() || std::isnan((*pValues)[it->second]))
      out << "null";
    else
      out << std::setprecision(15) << (*pValues)[it->second];
  }
  out << "]}";
  return out.str();
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
    " -Y 'http.request' -T fields -E separator='|' -e frame.number -e http.request.uri";

  if(!RunCommand(reqCmd, requestsOut, cmdErr))
  {
    err = "Failed to parse HTTP requests from pcap with tshark: " + cmdErr;
    return false;
  }

  std::unordered_map<std::string, std::string> reqUriByFrame;
  for(const std::string &line : SplitLines(requestsOut))
  {
    if(line.empty())
      continue;
    const std::vector<std::string> f = SplitFields(line, '|', 2);
    if(f.size() < 2)
      continue;
    reqUriByFrame[Trim(f[0])] = Trim(f[1]);
  }

  const std::string rspCmd =
    "tshark -r " + quotedPath +
    " -Y 'http.response && http.request_in && http.file_data' -T fields -E separator='|' -e frame.time_epoch -e http.request_in -e http.file_data";

  if(!RunCommand(rspCmd, responsesOut, cmdErr))
  {
    err = "Failed to parse HTTP responses from pcap with tshark: " + cmdErr;
    return false;
  }

  for(const std::string &line : SplitLines(responsesOut))
  {
    if(line.empty())
      continue;
    const std::vector<std::string> f = SplitFields(line, '|', 3);
    if(f.size() < 3)
      continue;

    const double ts = std::atof(Trim(f[0]).c_str());
    const std::string reqFrame = Trim(f[1]);
    const std::string uri = reqUriByFrame[reqFrame];
    const std::string body = DecodeHexPayload(f[2]);

    if(uri.empty())
      continue;

    if(uri.find("/abk/variables/var_list") != std::string::npos)
    {
      std::vector<std::string> vars;
      if(ParseVarListFromBody(body, vars) && !vars.empty())
        m_varNames = vars;
      continue;
    }

    if(uri.find("/abk/events/server_event") != std::string::npos)
    {
      ReplayFrame frame;
      frame.timestampSec = ts;
      frame.body = body;
      frame.values = ParseFirstDataListValuesLoose(body);
      if(frame.values.size() > (std::size_t)m_fallbackVarCount)
        m_fallbackVarCount = (int)frame.values.size();
      else if(frame.values.empty())
      {
        const int looseCount = CountElementsInFirstDataListLoose(body);
        if(looseCount > m_fallbackVarCount)
          m_fallbackVarCount = looseCount;
      }
      frame.hasAudioRecEvent = (body.find("AudioRec") != std::string::npos);
      m_frames.push_back(frame);
    }
  }

  if(m_frames.empty())
  {
    err = "No /abk/events/server_event responses found in capture";
    return false;
  }

  std::sort(m_frames.begin(), m_frames.end(), [](const ReplayFrame &a, const ReplayFrame &b) {
    return a.timestampSec < b.timestampSec;
  });

  return true;
}

void CPcapReplayEngine::FinalizeVariableModel()
{
  if(m_varNames.empty())
  {
    int count = m_fallbackVarCount;
    if(count <= 0 && !m_frames.empty())
      count = (int)m_frames.front().values.size();
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

  m_varIndex.clear();
  for(std::size_t i = 0; i < m_varNames.size(); ++i)
    m_varIndex[m_varNames[i]] = i;
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
    const int v = std::strtol(byteStr, nullptr, 16);
    out.push_back((char)v);
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

  std::smatch m;
  static const std::regex reVarList("\\\"VarList\\\"\\s*:\\s*\\[(.*?)\\]", std::regex::icase);
  if(!std::regex_search(body, m, reVarList))
    return false;

  const std::string arr = m[1].str();
  static const std::regex reStr("\\\"((?:\\\\.|[^\\\"])*)\\\"");
  auto it = std::sregex_iterator(arr.begin(), arr.end(), reStr);
  auto end = std::sregex_iterator();
  for(; it != end; ++it)
    varsOut.push_back((*it)[1].str());

  return !varsOut.empty();
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

  for(std::size_t i = pArray + 1; i < body.size(); ++i)
  {
    const char c = body[i];
    if(inStr)
    {
      if(escape)
      {
        escape = false;
      }
      else if(c == '\\')
      {
        escape = true;
      }
      else if(c == '"')
      {
        inStr = false;
      }
      token.push_back(c);
      continue;
    }

    if(c == '"')
    {
      inStr = true;
      token.push_back(c);
      continue;
    }

    if(c == '[' || c == '{')
    {
      nested++;
      token.push_back(c);
      continue;
    }
    if(c == ']' || c == '}')
    {
      if(nested == 0)
      {
        const std::string t = Trim(token);
        if(IsJsonNumberToken(t))
          out.push_back(std::strtod(t.c_str(), nullptr));
        else if(!t.empty())
          out.push_back(std::numeric_limits<double>::quiet_NaN());
        break;
      }
      nested--;
      token.push_back(c);
      continue;
    }

    if(c == ',' && nested == 0)
    {
      const std::string t = Trim(token);
      if(IsJsonNumberToken(t))
        out.push_back(std::strtod(t.c_str(), nullptr));
      else if(!t.empty())
        out.push_back(std::numeric_limits<double>::quiet_NaN());
      token.clear();
      continue;
    }

    token.push_back(c);
  }

  return out;
}

int CPcapReplayEngine::CountElementsInFirstDataListLoose(const std::string &body)
{
  const std::size_t pDataLists = body.find("\"DataLists\"");
  if(pDataLists == std::string::npos)
    return 0;

  const std::size_t pArray = body.find('[', pDataLists);
  if(pArray == std::string::npos)
    return 0;

  bool inStr = false;
  bool escape = false;
  int nested = 0;
  int count = 0;
  bool haveToken = false;

  for(std::size_t i = pArray + 1; i < body.size(); ++i)
  {
    const char c = body[i];

    if(inStr)
    {
      haveToken = true;
      if(escape)
      {
        escape = false;
      }
      else if(c == '\\')
      {
        escape = true;
      }
      else if(c == '"')
      {
        inStr = false;
      }
      continue;
    }

    if(c == '"')
    {
      inStr = true;
      haveToken = true;
      continue;
    }

    if(c == '[' || c == '{')
    {
      nested++;
      haveToken = true;
      continue;
    }

    if(c == ']' || c == '}')
    {
      if(nested == 0)
      {
        if(haveToken)
          ++count;
        break;
      }
      nested--;
      haveToken = true;
      continue;
    }

    if(c == ',' && nested == 0)
    {
      ++count;
      haveToken = false;
      continue;
    }

    if(!std::isspace((unsigned char)c))
      haveToken = true;
  }

  return count;
}

std::vector<std::string> CPcapReplayEngine::ParseGetListNamesLoose(const std::string &body)
{
  std::vector<std::string> out;
  std::smatch m;
  static const std::regex reGetList("\\\"GetList\\\"\\s*:\\s*\\[(.*?)\\]", std::regex::icase);
  if(!std::regex_search(body, m, reGetList))
    return out;

  const std::string arr = m[1].str();
  static const std::regex reStr("\\\"((?:\\\\.|[^\\\"])*)\\\"");
  auto it = std::sregex_iterator(arr.begin(), arr.end(), reStr);
  auto end = std::sregex_iterator();
  for(; it != end; ++it)
    out.push_back((*it)[1].str());

  return out;
}

std::string CPcapReplayEngine::EscapeJson(const std::string &s)
{
  std::string out;
  out.reserve(s.size());
  for(char c : s)
  {
    switch(c)
    {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out.push_back(c); break;
    }
  }
  return out;
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
