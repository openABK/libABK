#pragma once

#include <chrono>
#include <cstddef>
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
    bool hasAudioRecEvent;
  };

  CPcapReplayEngine();

  bool LoadFromPcapng(const std::string &pcapPath, std::string &err);
  void SetPlaybackSpeed(double speed);
  void SetSessionEstablished(int sessionId, bool established);
  bool IsSessionEstablished(int sessionId) const;
  void StartAfterSessionEstablished(int sessionId);

  std::string NextServerEventResponse(int sessionId, ReplayEventInfo *pInfo = nullptr);
  std::string BuildVarListResponse() const;
  std::string BuildVarValueResponse(int sessionId, const std::string &requestBody) const;
  bool RegisterDaqList(const std::string &requestBody, std::string &listNameOut, std::string &errOut);

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

  SessionReplayState &GetOrCreateSessionState(int sessionId);
  const SessionReplayState *FindSessionState(int sessionId) const;

  bool ExtractTraffic(std::string &err);
  void FinalizeVariableModel();

  static bool RunCommand(const std::string &cmd, std::string &output, std::string &err);
  static std::vector<std::string> SplitLines(const std::string &text);
  static std::vector<std::string> SplitFields(const std::string &line, char sep, std::size_t maxParts);
  static std::string DecodeHexPayload(const std::string &hexText);
  static std::string Trim(const std::string &s);
  static bool ParseVarListFromBody(const std::string &body, std::vector<std::string> &varsOut);
  static std::vector<double> ParseFirstDataListValuesLoose(const std::string &body);
  static int CountElementsInFirstDataListLoose(const std::string &body);
  static std::vector<std::string> ParseGetListNamesLoose(const std::string &body);
  static std::string EscapeJson(const std::string &s);
  static std::string BuildDataArrayJson(const std::vector<double> &values);

private:
  std::string m_pcapPath;
  mutable std::mutex m_mutex;
  std::vector<std::string> m_varNames;
  std::unordered_map<std::string, std::size_t> m_varIndex;
  std::unordered_map<std::string, std::vector<std::string>> m_createdDaqLists;
  std::vector<ReplayFrame> m_frames;
  std::unordered_map<int, SessionReplayState> m_sessionStates;

  int m_fallbackVarCount;
  double m_speed;
  double m_firstFrameTs;
};
