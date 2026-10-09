#include "stdafx.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "MyFakeLogger.h"
#include "MyLoggerInterface.h"
#include "ReplayEngine.h"
#include "drogon/drogon.h"

namespace
{

std::string ReplayEventSummary(const std::string &body)
{
  std::string summary;
  if(body.find("AudioRec") != std::string::npos)
    summary += "AudioRec event";
  if(body.find("Mailbox") != std::string::npos || body.find("DaqListMailbox") != std::string::npos)
  {
    if(!summary.empty())
      summary += "; ";
    summary += "mailbox data present";
  }
  if(body.find("AudioRecStart") != std::string::npos)
  {
    if(!summary.empty())
      summary += "; ";
    summary += "AudioRecStart detected";
  }
  if(body.find("AudioRecStop") != std::string::npos)
  {
    if(!summary.empty())
      summary += "; ";
    summary += "AudioRecStop detected";
  }
  if(summary.empty())
    summary = "generic event payload";
  return summary;
}

int ParseSessionIdFromQuery(const std::string &query)
{
  std::smatch m;
  static const std::regex re("(?:^|&)SessionId=([0-9]+)(?:&|$)", std::regex::icase);
  if(!std::regex_search(query, m, re) || m.size() < 2)
    return -1;
  return std::atoi(m[1].str().c_str());
}

int ParseSessionIdFromBody(const std::string &body)
{
  Json::Value root;
  Json::CharReaderBuilder builder;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  std::string errs;
  if(!reader->parse(body.data(), body.data() + body.size(), &root, &errs) || !root.isObject())
    return -1;
  const Json::Value &id = root["SessionId"];
  if(id.isIntegral())
    return id.asInt();
  if(id.isString())
    return std::atoi(id.asCString());
  return -1;
}

struct ReplayTraceState
{
  std::mutex mutex;
  int lastSessionId;
  std::string lastEventType;
  bool hasInline;

  ReplayTraceState()
    : lastSessionId(-1),
      hasInline(false)
  {
  }
};

void PrintReplayProgress(ReplayTraceState &trace, int sessionId, const CPcapReplayEngine::ReplayEventInfo &info, const std::string &body)
{
  const std::string eventType = info.eventType.empty() ? ReplayEventSummary(body) : info.eventType;
  const double pct = std::max(0.0, std::min(100.0, info.progress * 100.0));

  std::ostringstream line;
  line << "[Replay][S" << sessionId << "] pkt " << info.packetIndex << "/" << info.packetCount
       << " (" << std::fixed << std::setprecision(1) << pct << "%)"
       << " event=" << eventType;

  std::lock_guard<std::mutex> lock(trace.mutex);
  const bool sameLine = trace.hasInline && trace.lastSessionId == sessionId && trace.lastEventType == eventType;
  if(sameLine)
  {
    std::cerr << '\r' << line.str() << std::flush;
  }
  else
  {
    if(trace.hasInline)
      std::cerr << '\n';
    std::cerr << line.str() << '\n';
    trace.lastSessionId = sessionId;
    trace.lastEventType = eventType;
    trace.hasInline = true;
  }
}

int DrogonToAbkMethod(const drogon::HttpMethod &method)
{
  switch(method)
  {
    case drogon::HttpMethod::Get: return CLoggerInterface::HTTP_METHOD_GET;
    case drogon::HttpMethod::Post: return CLoggerInterface::HTTP_METHOD_POST;
    case drogon::HttpMethod::Put: return CLoggerInterface::HTTP_METHOD_PUT;
    case drogon::HttpMethod::Delete: return CLoggerInterface::HTTP_METHOD_DELETE;
    default: return CLoggerInterface::HTTP_METHOD_INVALID;
  }
}

void ForwardToAbk(const drogon::HttpRequestPtr &req,
                  std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                  CMyLoggerInterface &ifLogger)
{
  std::string body(req->body());
  std::string ip = req->getPeerAddr().toIp();

  CLoggerInterface::HTTP_REQUEST reqIf;
  reqIf.nHttpMethod = DrogonToAbkMethod(req->getMethod());
  reqIf.pUrl = &req->path();
  reqIf.pQueryString = &req->query();
  reqIf.pPostData = &body;
  reqIf.pClientAddr = &ip;

  CLoggerInterface::HTTP_RESPONSE respIf;
  std::string responseData;
  respIf.pResponseData = &responseData;
  respIf.nStatusCode = 200;

  ifLogger.HandleHttpRequest(reqIf, respIf);

  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setStatusCode(static_cast<drogon::HttpStatusCode>(respIf.nStatusCode));
  resp->setBody(responseData);
  callback(resp);
}

// calls the fake logger interface directly and returns its status code and response
int CallAbk(CMyLoggerInterface &ifLogger, int nMethod, const std::string &path, const std::string &query, const std::string &body, std::string &responseOut)
{
  std::string postData(body);
  std::string ip("127.0.0.1");
  CLoggerInterface::HTTP_REQUEST reqIf;
  reqIf.nHttpMethod = nMethod;
  reqIf.pUrl = &path;
  reqIf.pQueryString = &query;
  reqIf.pPostData = &postData;
  reqIf.pClientAddr = &ip;

  CLoggerInterface::HTTP_RESPONSE respIf;
  responseOut.clear();
  respIf.pResponseData = &responseOut;
  respIf.nStatusCode = 200;
  ifLogger.HandleHttpRequest(reqIf, respIf);
  return respIf.nStatusCode;
}

// answers var_meta/mailbox_meta per variable: recorded meta data first, fake logger for the rest
void HandleMetaRequest(const drogon::HttpRequestPtr &req,
                       std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                       CPcapReplayEngine &replay,
                       CMyLoggerInterface &ifLogger)
{
  const std::string path = req->path();
  const std::string query(req->query());
  const std::string body(req->body());

  std::vector<std::string> requested;
  std::unordered_map<std::string, Json::Value> meta;
  replay.LookupRecordedMeta(path, body, requested, meta);
  if(requested.empty()) // nothing (valid) requested: let the fake logger answer as usual
  {
    ForwardToAbk(req, std::move(callback), ifLogger);
    return;
  }

  Json::Value missing(Json::arrayValue);
  for(const std::string &name : requested)
  {
    if(!meta.count(name))
      missing.append(name);
  }

  auto resp = drogon::HttpResponse::newHttpResponse();
  if(!missing.empty())
  {
    Json::Value fakeReq(Json::objectValue);
    fakeReq["VarList"] = missing;
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    std::string fakeBody;
    const int status = CallAbk(ifLogger, CLoggerInterface::HTTP_METHOD_POST, path, query, Json::writeString(writer, fakeReq), fakeBody);
    if(status != 200) // e.g. unknown variable: report the error of the logger
    {
      resp->setStatusCode(static_cast<drogon::HttpStatusCode>(status));
      resp->setBody(fakeBody);
      callback(resp);
      return;
    }

    Json::Value fakeRoot;
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    std::string errs;
    if(reader->parse(fakeBody.data(), fakeBody.data() + fakeBody.size(), &fakeRoot, &errs) && fakeRoot["MetaData"].isArray())
    {
      for(const Json::Value &entry : fakeRoot["MetaData"])
      {
        if(entry.isObject() && entry["Name"].isString())
          meta[entry["Name"].asString()] = entry;
      }
    }
  }

  Json::Value root(Json::objectValue);
  Json::Value &arr = root["MetaData"];
  arr = Json::Value(Json::arrayValue);
  for(const std::string &name : requested)
  {
    const auto it = meta.find(name);
    if(it != meta.end())
      arr.append(it->second);
  }

  Json::StreamWriterBuilder writer;
  writer["indentation"] = "";
  resp->setStatusCode(drogon::k200OK);
  resp->setBody(Json::writeString(writer, root));
  callback(resp);
}

void PrintUsage(const char *prog)
{
  std::cout << "Usage: " << prog << " [port] [pcapngPath] [speed] [fixedServerEventFile]\n";
  std::cout << "  port       : HTTP port (default 8080)\n";
  std::cout << "  pcapngPath : capture path (default ServerExample/PcapNoRec.pcapng)\n";
  std::cout << "  speed      : replay speed multiplier (default 1.0)\n";
  std::cout << "  fixedServerEventFile : optional file sent for every /abk/events/server_event response\n";
}

} // namespace

int main(int argc, char *argv[])
{
  int port = 8080;
  std::string pcapPath = "ServerExample/PcapNoRec.pcapng";
  double speed = 1.0;
  std::string fixedServerEventFile;
  std::string fixedServerEventBody;

  if(argc >= 2)
    port = std::atoi(argv[1]);
  if(argc >= 3)
    pcapPath = argv[2];
  if(argc >= 4)
    speed = std::atof(argv[3]);
  if(argc >= 5)
    fixedServerEventFile = argv[4];
  if(speed <= 0.0)
    speed = 1.0;

  if(!fixedServerEventFile.empty())
  {
    std::ifstream in(fixedServerEventFile.c_str(), std::ios::in | std::ios::binary);
    if(!in)
    {
      std::cerr << "Fixed server_event response file not found or unreadable: " << fixedServerEventFile << "\n";
      PrintUsage(argv[0]);
      return EXIT_FAILURE;
    }

    std::ostringstream content;
    content << in.rdbuf();
    fixedServerEventBody = content.str();
    if(fixedServerEventBody.empty())
    {
      std::cerr << "Fixed server_event response file is empty: " << fixedServerEventFile << "\n";
      return EXIT_FAILURE;
    }
  }

  if(!std::filesystem::exists(pcapPath))
  {
    std::cerr << "Replay pcapng not found: " << pcapPath << "\n";
    PrintUsage(argv[0]);
    return EXIT_FAILURE;
  }

  CPcapReplayEngine replay;
  std::string err;
  if(!replay.LoadFromPcapng(pcapPath, err))
  {
    std::cerr << "Failed to load replay capture: " << err << "\n";
    return EXIT_FAILURE;
  }
  replay.SetPlaybackSpeed(speed);

  int varCount = std::max(10, replay.GetEstimatedVarCount());
  if(varCount > FAKE_VARCOUNT_MAX)
    varCount = FAKE_VARCOUNT_MAX;

  CMyFakeLogger loggerFake(varCount, 1000);
  loggerFake.SetReplayMode(true);
  ReplayTraceState trace;

  sockaddr_in addrBindLocal;
  addrBindLocal.sin_port = htons(port);
  addrBindLocal.sin_addr.s_addr = INADDR_ANY;

  CMyLoggerInterface ifLogger(&loggerFake, addrBindLocal);
  loggerFake.Init(&ifLogger);

  const std::vector<std::string> &varNames = replay.GetVariableNames();
  const int renameCount = std::min((int)varNames.size(), loggerFake.GetVarCount());
  for(int i = 0; i < renameCount; ++i)
    loggerFake.SetVariableName(i, varNames[(std::size_t)i].c_str());

  drogon::app().registerHandlerViaRegex("/abk/.*", [&ifLogger, &replay, &loggerFake, &trace, &fixedServerEventBody](const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    const std::string path = req->path();

    if(req->getMethod() == drogon::HttpMethod::Post && path == "/abk/system_information/session_id")
    {
      ForwardToAbk(req,
                   [&replay, callback = std::move(callback)](const drogon::HttpResponsePtr &resp) mutable {
                     const int sessionId = ParseSessionIdFromBody(std::string(resp->body()));
                     if(sessionId > 0)
                     {
                       replay.SetSessionEstablished(sessionId, true);
                       std::cerr << "Session negotiation succeeded. Replay playback is now enabled for SessionId=" << sessionId << ".\n";
                     }
                     else
                     {
                       std::cerr << "Session negotiation response did not include a valid SessionId.\n";
                     }
                     callback(resp);
                   },
                   ifLogger);
      return;
    }

    if(req->getMethod() == drogon::HttpMethod::Get && path == "/abk/events/server_event")
    {
      const std::string query = std::string(req->query());
      const int sessionId = ParseSessionIdFromQuery(query);
      if(sessionId <= 0)
      {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k400BadRequest);
        resp->setBody("{\"error\":\"Missing or invalid SessionId\"}");
        callback(resp);
        return;
      }

      if(!replay.IsSessionEstablished(sessionId))
      {
        std::cerr << "No active ABK session yet for SessionId=" << sessionId << "; waiting for /abk/system_information/session_id before replaying the capture.\n";
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k200OK);
        resp->setBody("{\"DataLists\":{},\"Events\":[]}");
        callback(resp);
        return;
      }

      if(!fixedServerEventBody.empty())
      {
        CPcapReplayEngine::ReplayEventInfo info;
        info.packetIndex = 0;
        info.packetCount = 0;
        info.progress = 1.0;
        info.eventType = "fixed-response";
        PrintReplayProgress(trace, sessionId, info, fixedServerEventBody);

        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k200OK);
        resp->setBody(fixedServerEventBody);
        callback(resp);
        return;
      }

      replay.StartAfterSessionEstablished(sessionId);
      CPcapReplayEngine::ReplayEventInfo info;
      const std::string body = replay.NextServerEventResponse(sessionId, &info);
      PrintReplayProgress(trace, sessionId, info, body);
      replay.ApplyCurrentValuesToLogger(sessionId, loggerFake);

      auto resp = drogon::HttpResponse::newHttpResponse();
      resp->setStatusCode(drogon::k200OK);
      resp->setBody(body);
      callback(resp);
      return;
    }

    if(req->getMethod() == drogon::HttpMethod::Post && CPcapReplayEngine::IsMetaRequestPath(path))
    {
      HandleMetaRequest(req, std::move(callback), replay, ifLogger);
      return;
    }

    // remember the list names the client creates, so server_event responses use these names
    CPcapReplayEngine::ListKind listKind;
    if(req->getMethod() == drogon::HttpMethod::Put && CPcapReplayEngine::ListKindFromPath(path, listKind))
      replay.NoteDaqListRequest(ParseSessionIdFromQuery(std::string(req->query())), path, std::string(req->body()));

    // all other requests: answer with the matching response recorded in the capture
    {
      std::string recordedBody;
      int recordedStatus = 200;
      if(replay.FindRecordedResponse(req->methodString(), path, std::string(req->query()), std::string(req->body()), recordedBody, recordedStatus))
      {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(static_cast<drogon::HttpStatusCode>(recordedStatus));
        resp->setBody(recordedBody);
        callback(resp);
        return;
      }
    }

    // nothing recorded for this request: let the fake logger answer
    ForwardToAbk(req, std::move(callback), ifLogger);
  });

  drogon::app().setThreadNum(4);

  std::cerr << "Trying to bind ABK replay server to 0.0.0.0:" << port << "...\n";
  try
  {
    drogon::app().addListener("0.0.0.0", port);
  }
  catch(const std::exception &e)
  {
    std::cerr << "Failed to bind replay server to port " << port << ": " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  std::cerr << "ABK replay server listening on port " << port
            << " using pcap " << pcapPath
            << " at speed " << speed << "x\n";
  if(!fixedServerEventFile.empty())
    std::cerr << "Fixed server_event response mode enabled with file: " << fixedServerEventFile << "\n";

  drogon::app().run();

  loggerFake.Shutdown();
  ifLogger.Shutdown();

  return EXIT_SUCCESS;
}
