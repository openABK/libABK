#pragma once

#include <chrono>
#include <cstddef>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class CMyFakeLogger;

class CPcapReplayEngine
{
public:
  struct ReplayEventInfo
  {
    std::size_t packetIndex;
    std::size_t packetCount;
    double progress;
    std::string eventType;
  };

  struct ReplayFrame
  {
    double timestampSec;
    std::string body;
    std::vector<double> values;
    std::string eventType;
    bool truncated;
  };

  CPcapReplayEngine();

  bool LoadFromPcapng(const std::string &pcapPath, std::string &err);
  void SetPlaybackSpeed(double speed);
  void SetSessionEstablished(int sessionId, bool established);
  bool IsSessionEstablished(int sessionId) const;
  void StartAfterSessionEstablished(int sessionId);

  // time-sequenced replay of /abk/events/server_event
  std::string NextServerEventResponse(int sessionId, ReplayEventInfo *pInfo = nullptr);

  // generic request -> recorded response matching (all other endpoints)
  bool FindRecordedResponse(const std::string &method,
                            const std::string &path,
                            const std::string &query,
                            const std::string &requestBody,
                            std::string &responseOut,
                            int &statusOut);

  int GetEstimatedVarCount() const;
  const std::vector<std::string> &GetVariableNames() const;
  void ApplyCurrentValuesToLogger(int sessionId, CMyFakeLogger &logger) const;

private:
  struct SessionReplayState
  {
    bool sessionEstablished;
    bool started;
    std::size_t frameIndex;
    std::vector<double> currentValues;
    std::chrono::steady_clock::time_point startWall;

    SessionReplayState()
      : sessionEstablished(false),
        started(false),
        frameIndex(0)
    {
    }
  };

  struct RecordedExchange
  {
    double timestampSec;
    std::string canonicalBody;
    std::string responseBody;
    int statusCode;
  };

  struct ExchangeGroup
  {
    std::vector<RecordedExchange> exchanges;
    std::map<std::string, std::size_t> cursorByBody; // round-robin per canonical request body
    std::size_t cursorAny;

    ExchangeGroup() : cursorAny(0) {}
  };

  SessionReplayState &GetOrCreateSessionState(int sessionId);
  const SessionReplayState *FindSessionState(int sessionId) const;

  bool ExtractTraffic(std::string &err);
  void AddServerEventFrame(double ts, const std::string &body);
  void FinalizeVariableModel();

  static std::string MakeKey(const std::string &method, const std::string &path, const std::string &query);
  static std::string CanonicalQuery(const std::string &query);
  static std::string CanonicalBody(const std::string &body);
  static void SplitUri(const std::string &uri, std::string &path, std::string &query);

  static bool RunCommand(const std::string &cmd, std::string &output, std::string &err);
  static std::vector<std::string> SplitLines(const std::string &text);
  static std::vector<std::string> SplitFields(const std::string &line, char sep, std::size_t maxParts);
  static std::string DecodeHexPayload(const std::string &hexText);
  static std::string Trim(const std::string &s);
  static bool ParseVarListFromBody(const std::string &body, std::vector<std::string> &varsOut);
  static bool ParseServerEventJson(const std::string &body, std::vector<double> &valuesOut, std::string &eventTypeOut);
  static std::vector<double> ParseFirstDataListValuesLoose(const std::string &body);
  static std::string ParseEventTypeLoose(const std::string &body);
  static std::string BuildDataArrayJson(const std::vector<double> &values);

private:
  std::string m_pcapPath;
  mutable std::mutex m_mutex;
  std::vector<std::string> m_varNames;
  std::vector<ReplayFrame> m_frames;
  std::unordered_map<std::string, ExchangeGroup> m_exchanges;
  std::unordered_map<int, SessionReplayState> m_sessionStates;

  int m_fallbackVarCount;
  double m_speed;
  double m_firstFrameTs;
};
