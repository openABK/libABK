#include "stdafx.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <regex>
#include <sstream>

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
  std::smatch m;
  static const std::regex re("\\\"SessionId\\\"\\s*:\\s*([0-9]+)", std::regex::icase);
  if(!std::regex_search(body, m, re) || m.size() < 2)
    return -1;
  return std::atoi(m[1].str().c_str());
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

    if(req->getMethod() == drogon::HttpMethod::Get && path == "/abk/variables/var_list")
    {
      auto resp = drogon::HttpResponse::newHttpResponse();
      resp->setStatusCode(drogon::k200OK);
      resp->setBody(replay.BuildVarListResponse());
      callback(resp);
      return;
    }

    if(req->getMethod() == drogon::HttpMethod::Put && (path == "/abk/variables/daq_list" || path == "/abk/variables/mailbox_daq_list"))
    {
      std::string body(req->body());
      std::string daqName;
      std::string dqErr;
      if(!replay.RegisterDaqList(body, daqName, dqErr))
      {
        std::cerr << "DAQ list rejected: " << dqErr << "\n";
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k400BadRequest);
        resp->setBody("{\"error\":\"" + dqErr + "\"}");
        callback(resp);
        return;
      }

      std::cerr << "[DAQ] registered list '" << daqName << "' using " << replay.GetVariableNames().size() << " replay variable names\n";
      auto resp = drogon::HttpResponse::newHttpResponse();
      resp->setStatusCode(drogon::k200OK);
      resp->setBody("{}");
      callback(resp);
      return;
    }

    if(req->getMethod() == drogon::HttpMethod::Post && path == "/abk/variables/var_value")
    {
      const std::string query = std::string(req->query());
      const int sessionId = ParseSessionIdFromQuery(query);
      auto resp = drogon::HttpResponse::newHttpResponse();
      resp->setStatusCode(drogon::k200OK);
      resp->setBody(replay.BuildVarValueResponse(sessionId, std::string(req->body())));
      callback(resp);
      return;
    }

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
