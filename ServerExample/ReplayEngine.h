#pragma once

#include <chrono>
#include <cstddef>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <json/json.h>

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
    std::vector<Json::Value> values; // values of the DAQ list (numbers, strings, bools, null)
    std::string eventType;
    std::string listName; // name of the first DataList in the frame, may be empty for truncated frames
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

  enum ListKind
  {
    LIST_DAQ,     // /abk/variables/daq_list
    LIST_MAILBOX, // /abk/variables/mailbox_daq_list
    LIST_MAM,     // /abk/variables/mam_daq_list
    LIST_COUNT
  };

  // returns true if path is one of the list creating endpoints
  static bool ListKindFromPath(const std::string &path, ListKind &kindOut);

  // remember the list name the client created (PUT daq_list/mailbox_daq_list/mam_daq_list).
  // server_event responses then use this name instead of the one recorded in the capture.
  void NoteDaqListRequest(int sessionId, const std::string &path, const std::string &requestBody);

  // meta data requests (var_meta, mailbox_meta) are answered per variable, from the recorded
  // meta data where available. requestedOut receives the requested variables in request order,
  // recordedOut the recorded meta data entries of those variables found in the capture.
  static bool IsMetaRequestPath(const std::string &path);
  void LookupRecordedMeta(const std::string &path,
                          const std::string &requestBody,
                          std::vector<std::string> &requestedOut,
                          std::unordered_map<std::string, Json::Value> &recordedOut) const;

  int GetEstimatedVarCount() const;
  const std::vector<std::string> &GetVariableNames() const;
  void ApplyCurrentValuesToLogger(int sessionId, CMyFakeLogger &logger) const;

private:
  struct SessionReplayState
  {
    bool sessionEstablished;
    bool started;
    std::size_t frameIndex;
    std::vector<Json::Value> currentValues; // last known values of the recorded DAQ list
    std::string clientListName[LIST_COUNT]; // list names registered by the client, empty if not created yet
    std::vector<long> clientListMap[LIST_COUNT]; // per client list entry: index into the recorded list, -1 if unknown
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
  void ParseServerEventFrame(ReplayFrame &frame);
  bool KindOfRecordedList(const std::string &recordedName, ListKind &kindOut) const;
  Json::Value RewriteDataLists(const Json::Value &recordedLists, const SessionReplayState &state) const;
  std::vector<long> BuildListMap(ListKind kind, const std::vector<std::string> &clientVars) const;
  static Json::Value MapListValues(const Json::Value &recordedList, const std::vector<long> &map);
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
  static bool ParseServerEventJson(const std::string &body, const std::string &preferredList, std::vector<Json::Value> &valuesOut, std::string &eventTypeOut, std::string &listNameOut);
  static std::vector<Json::Value> ParseDataListValuesLoose(const std::string &body, const std::string &preferredList);
  static std::string ParseEventTypeLoose(const std::string &body);
  static std::string ParseListNameLoose(const std::string &body, const std::string &preferredList);
  static bool ParseDaqListRequest(const std::string &requestBody, std::string &nameOut, std::vector<std::string> &varsOut);

private:
  std::string m_pcapPath;
  mutable std::mutex m_mutex;
  std::vector<std::string> m_varNames;
  std::string m_recordedDaqListName; // name of the DAQ list created in the capture
  std::size_t m_recordedDaqListSize = 0; // number of values in the DAQ list (from the capture's daq_list request or complete frames)
  std::unordered_map<std::string, ListKind> m_recordedListKinds; // list names created in the capture -> kind
  std::vector<std::string> m_recordedListVars[LIST_COUNT]; // variables of the recorded lists, empty if unknown
  std::unordered_map<std::string, std::unordered_map<std::string, Json::Value>> m_recordedMeta; // path -> variable name -> recorded meta data entry
  std::vector<ReplayFrame> m_frames;
  std::unordered_map<std::string, ExchangeGroup> m_exchanges;
  std::unordered_map<int, SessionReplayState> m_sessionStates;

  int m_fallbackVarCount;
  double m_speed;
  double m_firstFrameTs;
};
