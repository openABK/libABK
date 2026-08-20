//------------------------------------------------------------------------------------------------
// Author: D. Burger, Friedberg, Germany, <www.openABK.org>, <www.embu-sys.de>, <info@openABK.org>
//
// You are not allowed to remove this heading from the source code
// You are free to use this library under the terms of the
// Code Project Open Library, see <http://www.codeproject.com/info/cpol10.aspx>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//  _____  __   __  _____  _    _        ____              
// |  ___||  \ /  ||  _  \| |  | |      / ___|  _   _  ___ 
// |  __| |   V   ||  -  /| |  | |  __  \___ \ | | | |/ __|
// | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
// |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
//                                              |___/      
// Filename:    AbkLoggerIf.h
// Created:     2012-05-07 (08:55)
// Author:      D. Burger
// Description: Interface between HTTP and loggers internal data
//------------------------------------------------------------------------------------------------

#pragma once

#include <iostream>
#include <string>
#include <map>
#include <list>
#include <sstream>

#include "JsonFormatter.h"
#include "VarRef.h"
#include "CrossPlatform.h"
#include "Daq.h"
#include "LogQueue.h"

class CJsonParser;

namespace Abk {


typedef CDaqValues*(*PFN_CREATE_DAQ)();

class CLoggerInterface; //forward decl.


// interface between logger and HTTP interface
class CLoggerInterface
  {
  friend class CSession;
 public:
  enum HTTP_METHOD
    {
    HTTP_METHOD_INVALID, // used to identify an invalid method
    HTTP_METHOD_GET,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_DELETE,
    };

  struct HTTP_REQUEST
    {
    const std::string *pUrl; // url without query string of the request
    const std::string *pQueryString; // unescaped query string of the request
    const std::string *pClientAddr; // address of the requesting client. This is for information purposes only, need not to be initialized by the http server
    const std::string *pPostData; // data that came in with the request
    int nHttpMethod; // method of the request, one of HTTP_METHOD_xxx enumeration
    };

  struct HTTP_RESPONSE
    {
    std::string *pResponseData; // response data
    std::string *pExtraHeaders; // extra headers specific to openABK handling. each item must be followed by exactly one \r\n
    int nStatusCode; // status code as result of the handler
    void Set (int nCode) {nStatusCode=nCode;}
    void Set (int nCode, const std::string &strResponse) {nStatusCode=nCode; *pResponseData=strResponse;}
    void Set (int nCode, const std::stringstream &strResponse) {nStatusCode=nCode; *pResponseData=strResponse.str();}
    void Set (CJsonFormatter &jfResponse) {Set(200,*jfResponse.GetStream());}
    };


  typedef void (CLoggerInterface::*PFN_HANDLEREQUEST)(const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse);
  
  typedef struct tagRESPONSETABLE
    {
    const char *pszUri; // uri to be handled
    HTTP_METHOD nMethod; // method to be handled
    PFN_HANDLEREQUEST pfnHandler; // function which handles the request
    } RESPONSETABLE;

  enum LOGSEVERITY  // inherit the log severities from the logging queue
    {
    LOGSEVERITY_INFO    =CLogQueue<char>::LOGSEVERITY_INFO    ,
    LOGSEVERITY_DEBUG   =CLogQueue<char>::LOGSEVERITY_DEBUG   ,
    LOGSEVERITY_TRACE   =CLogQueue<char>::LOGSEVERITY_TRACE   ,
    LOGSEVERITY_WARNING =CLogQueue<char>::LOGSEVERITY_WARNING ,
    LOGSEVERITY_ERROR   =CLogQueue<char>::LOGSEVERITY_ERROR
    };

  enum MSGBOX_BUTTON
    {
    MSGBOX_BUTTON_OK                =0x0001,
    MSGBOX_BUTTON_OKPERMANENT       =0x0002,
    MSGBOX_BUTTON_YES               =0x0004,
    MSGBOX_BUTTON_NO                =0x0008,
    MSGBOX_BUTTON_RETRY             =0x0010,
    MSGBOX_BUTTON_CANCEL            =0x0020,
    MSGBOX_BUTTON_IGNORE            =0x0040,
    MSGBOX_BUTTON_ABORT             =0x0080,
    MSGBOX_BUTTON_OKCANCEL          =MSGBOX_BUTTON_OK|MSGBOX_BUTTON_CANCEL,
    MSGBOX_BUTTON_YESNO             =MSGBOX_BUTTON_YES|MSGBOX_BUTTON_NO,
    };

  class CClientUid // identification of a client
    {
    public:
      int m_nSession;           // session id of the client, 0 if no session session
      std::string m_strAddress; // address of the client
      std::string m_strClass;  // class of the client, e.g. "display"
      std::string m_strType;   // type of the client e.g. "mytronics_superdisplay3000"
      std::string m_strSerial; // serial number of the client, may also be the mac address or other unique id
    public:
      CClientUid () {m_nSession=0;}
      void FormatStatistics (CJsonStreamObject &joDump) const; // formats characteristics into an json object
    };
  
  struct CClientFirmware
    {
    std::string m_strUrl; // url where the client can load the firmware image
    std::string m_strVersion; // version string of the firmware. if not available, it is an empty string
    std::string m_strMd5; // MD5 hash of the firmware image. if not available it is an empty string
    };

  struct CClientConfig
    {
    std::string m_strUrl; // url where the client can load the config file. It it is not available, it is an empty string
    std::string m_strMd5; // MD5 hash of the config file. if not available it is recommended to be an empty string
    };

  private:
    static RESPONSETABLE m_tblResponse[]; // function table holding handlers for url/method pairs

  // data members
  protected: // session management
    CAbkMutex m_mutexSessionList;       // mutex protecting the session id generator
    static int m_nNextSessionId;        // id of the next session
    std::map<int, CSession *> m_mapSessions; // map of sessions
  protected: // variable management
    CAbkMutex m_mutexVarList;           // mutex protecting the variable list/map
    std::list<CVarRef *> m_lstMeasVars; // list of available measurement data variables
    std::list<CVarRef *> m_lstMailboxVars; // list of available mailbox variables
    bool m_bFirstRequest;               // true as long no requests came in
    CAbkMutex m_mutexFirstRequest;      // mutex protecting m_bFirstRequest
  private: // error/warning logging
    CLogQueue<char> m_queueLog; // error logging, This queue is fed whenever log messages are available. OnLogAdded() is called and entities can be read at application level 

  // construction/destruction/lifetime
   public:
    CLoggerInterface ();
    virtual ~CLoggerInterface ();
    bool Shutdown (void); // shuts down the logger interface

  // misc implementation
  private:
    static void ClearVars (std::list<CVarRef *> *pList); // clears variables and empties the list

  // session management
  public:
    int GenerateSession (const CClientUid &uidClient); // generates a session
    bool DeleteSession (int nIdDelete); // deletes a session
    bool DeleteSession (CSession *pSession); // deletes a session
    int EnumSessions (std::list <CClientUid> *pReturn) const; // lists all connected clients
    bool IdentifyClient (const CClientUid &clientUid, const char *pszMessage=NULL); // a client shall identify itself
    virtual void OnClientConnected (const char *pszClass, const char *pszType, const char *pszSerial, const char *pszFwVersion, const char *pszHwVersion, int nSessionId)=0; // called to notify the server about client connection
  private:
    bool UnregisterSession (CSession *pSession); // unregisters a session in the list
    bool DeleteAllSessions (void); // deletes all sessions
    CSession *GetSession (int nSessionId) const; // retrurns pointer to a session
    CSession *GetSession (const std::string &strQuery) const; // retrurns pointer to a session
    CSession *GetSession (const HTTP_REQUEST &rRequest) const; // returns pointer to a session
    bool LockSessions (int nTimeout) const; // locks session map
    bool UnlockSessions (void) const; // unlocks the session map

  // variable management
  public:
    virtual bool OnGetVariableList (std::list<CVarRef *> *pList) const=0; // called to retrieve the available measurement variable catalogue
    virtual bool OnGetMailboxList (std::list<CVarRef *> *pList) const=0; // called to retrieve the available mailbox catalogue
    //typedef enum E_DAQTYPE { E_DAQTYPE_UNDEFINED = 0, E_DAQTYPE_LIST, E_DAQTYPE_TREND } daqtype_t;
    virtual void OnDAQCreate(int nSessionId, const char *pszDaqName, /*daqtype_t eDaqType,*/ bool &rbUseExternalTimer) = 0; //set rbUseExternalTimer to true if an external timer for a daq exist (instead of the internal daq timer). 
    virtual void OnDAQCycleUpdate(int nSessionId, const char *pszDaqName, /*daqtype_t eDaqType,*/ int &rnDAQCycleMs) = 0; // do not lock session list, it is already locked!
    virtual void OnDAQDispose(int nSessionId, const char *pszDaqName) = 0;
  protected:
    CVarRef *FindVariable (const std::list<CVarRef *> &rList, const std::string &rStrVarName) const; // searches a variable by its name
    CVarRef *FindVariable (const std::list<CVarRef *> &rList, const char *pszVarName) const; // searches a variable by its name

  // event management
  public:
    virtual bool OnClientEvent (int nSender, time_t tmSent, const std::string &strEventType, const std::string &strParam, double dParam1, double dParam2); // called when an event from a client is recieved
    void FireEventToAllClients (int nSender, time_t tmSend, const char *pszEventType, const std::string &strParam, double dParam1, double dParam2); // fires an event to all connected clients
    void FireEventToAllClients (const char *pszEventType, const std::string &strParam, double dParam1, double dParam2); // fires an event to all connected clients
    void FireEventToAllClients (const char *pszEventType); // fires an event to all connected clients, no params
    void FireEventToAllClientsMeasurementStarted (); // notifies clients that measurement has started
    void FireEventToAllClientsMeasurementStopped (); // notifies clients that measurement has stopped
    void FireEventToAllClientsFormClose (std::string formName);
    void FireEventToAllClientsVarlistChanged ();
    void Alert (const char *pszVarName, const char *pszEventClass, unsigned int nPriority, time_t tmSend, bool bNoClientSideSuppress, const CJsonFormatter *pJfAdditionalFields, const char *pszViolatingValue=NULL); // alerts a variable value limit violation
    void Alert (const char *pszVarName, const char *pszEventClass, unsigned int nPriority, time_t tmSend, bool bNoClientSideSuppress, const CJsonFormatter *pJfAdditionalFields, double dViolatingValue); // alerts a variable value limit violation
    void AlertVarLimit (const char *pszVarName, const char *pszEventClass, unsigned int nPriority, time_t tmSend, const CJsonFormatter *pJfAdditionalFields, const char *pszViolatingValue=NULL); // alerts a variable value limit violation
    void AlertVarLimit (const char *pszVarName, const char *pszEventClass, unsigned int nPriority, time_t tmSend, const CJsonFormatter *pJfAdditionalFields, double dViolatingValue); // alerts a variable value limit violation
    void NotifyAppChanged (void); // notifies all clients that an app/config file has changed
    void RequestOpenForm (const char *pszFormName); // fires event to all client: request to open a form
    void RequestCloseForm (const char *pszFormName); // fires event to all client: request to close a form with a certain name
    void MsgBox (const char *pszCaption, const char *pszText, unsigned int nPriority, int nID, MSGBOX_BUTTON nButtons); // invokes message box
    void RequestAudioRec (int nId, double dMaxRecTime); // fires event for audio recording request display, fire to all clients
    void StopAudioRec (int nId); // fires event to stop audio recording, fire to all clients

  // form helper functions
  public:
    static void FormWriteInput    (CJsonStreamArray &jaDest, const char *pszName, const char *pszCaption, const char *pszInitialValue, int nMaxLen=-1, bool bReadOnly=false, bool bPassword=false, bool bUpdateable=false); // writes an input form field
    static void FormWriteCheckbox (CJsonStreamArray &jaDest, const char *pszName, const char *pszCaption, bool bInitialValue, bool bUpdateable=false); // writes a  checkbox form field
    static void FormWriteList     (CJsonStreamArray &jaDest, const char *pszName, const char *pszCaption, int nInitialValue, std::list<std::string> &lstOptions, bool bUpdateable=false); // writes a list form field
    static void FormWriteButton   (CJsonStreamArray &jaDest, const char *pszName, const char *pszCaption, bool bSubmit=false, bool bCancel=false, bool bUpdateable=false); // writes a button form field

  // error log
  protected:
    void AddLog (LOGSEVERITY nSeverity, const char * pszMessage, ...); // writes one line to error log
    void AddLogV (LOGSEVERITY nSeverity, const char * pszMessage, va_list args); // writes one line to error log
    BOOL PopLog (LOGSEVERITY &nSeverityGet, std::string &strMessageGet); // pops one entity from the error log
    virtual void OnLogAdded (void); // notifies that the log queue has got new entities. may be called in any thread context

  // misc virtual functions
  public:
    virtual bool OnSessionEventPollEstablished (CSession* pNewSession) = 0; // called to notify about new client session established
    virtual bool OnGetLoggerFwVersion (std::string &strReturn) const {return false;} // called to retrieve the logger firmware revision
    virtual bool OnGetLoggerHwVersion (std::string &strReturn) const {return false;} // called to retrieve the logger hardware revision
    virtual bool OnGetLoggerName (std::string &strReturn) const {return false;} // called to retrieve the name of the logger instance (e.g. PowerTrain-Logger)
    virtual bool OnGetLoggerType (std::string &strReturn) const {return false;} // called to retrieve the type of the logger, e.g. Logger2000
    virtual bool OnGetLoggerSerial (std::string &strReturn) const {return false;} // called to retrieve the serial number of the logger, e.g. 0001
    virtual bool OnGetLoggerDescriptionUrl (std::string &strReturn) const {return false;} // called to retrieve the description file URL type of the logger, e.g. /index.html
    virtual bool OnGetStorageInfo (unsigned long long &rTotal, unsigned long long &rFree) const=0; // called to retrieve the total and available storage space
    virtual std::string OnGetClientRole (int nSession, const char *pszClass, const char *pszType, const char *pszSerial); // returns the role of a client
    virtual bool OnGetConnectionPreference (const char *pszClass, const char *pszType, const char *pszSerial) const; // returns preference of a connection
    virtual bool OnGetClientFirmwareInfo (const char *pszClientClass, const char *pszClientType, std::list<CClientFirmware> &lstGet) const; // returns list of available firmware for a specific client
    virtual bool OnGetClientConfigInfo (const char *pszClientClass, const char *pszClientType, const char *pszClientSerial, CClientConfig &cfgGet) const; // returns client config file info
    virtual bool OnFormGet (const char *pszFormName, CJsonFormatter &jfForm, std::stringstream &strErr) const=0; // renders a form. return false if not rendered
    virtual bool OnFormPut (const char *pszFormName, CJsonParser &jpForm, std::stringstream &strErr)=0; // called when client returned a forms result. return false if error form

  // audio recording misc overrideables
  public:
    virtual bool OnAudioRecHeader (int nId, int nSamplerateHz, int nChannels, int nBitsPerSample) const; // gets called when client starts an audio recording
    virtual bool OnAudioRecData (int nId, const int *pSampleData, int nSamples) const; // gets called when client has audio recording data
    virtual bool OnAudioRecFooter (int nId) const; // gets called when client terminates audio recording


  // HTTP interface
  public:
    void HandleHttpRequest (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // handles all HTTP requests
    static HTTP_METHOD DecodeHttpMethod (const char *pszMethod); // decodes method string into enumerated value
  protected:
    static bool DecodeUriValue (const std::string &strQuery, const char *pszKey, std::string &rStrDest); // decode a variable value from the query string or post data

  private:
    void Handle_PostSession         (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // handles the session request
    void Handle_DeleteSession       (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // handles the session deletion request
    void Handle_GetVarListMeas      (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // responds with list of measurement variables
    void Handle_GetVarListMailbox   (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // responds with list of mailbox variables
    void Handle_GetVarList          (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse, const std::list<CVarRef *> &rListDump); // responds with list of variables (measurement/mailbox)
    void Handle_PostVarMeta         (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse, const std::list<CVarRef *> &rListSearch); // responds with meta data
    void Handle_PostVarMetaMeas     (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // responds with measurement variable meta data
    void Handle_PostVarMetaMailbox  (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // responds with mailbox variable meta data
    void Handle_PutDaqList          (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse, const std::list<CVarRef *> &rVarList, PFN_CREATE_DAQ pfnCreate); // puts a daq list to the session    
    void Handle_PutDaqListMeas      (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // puts a variable daq list to the session    
    void Handle_PutDaqListMAM       (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // puts a variable min-average-amx daq list to the session    
    void Handle_PutDaqListMailbox   (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // puts a mailbox daq list to the session    
    void Handle_PutDaqTrend         (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // puts a daq trend to the session    
    void Handle_DeleteDaqList       (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // deletes a daq list or daq trend from the session
    void Handle_EventPolling        (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // handles the main event polling
    void Handle_InterfaceStatistic  (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // handles the statistic query
    void Handle_PutClientEvent      (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // handles events coming from a client
    void Handle_GetClientAddress    (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // handles ip address request from the client
    void Handle_GetServerInfo       (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // handles server info get request
    void Handle_GetCurrentTime      (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // handles time get request
    void Handle_GetStorageInfo      (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // handles storage info get request
    void Handle_PostVarValue        (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse, const std::list<CVarRef *> &rVarList); // handles var/mailbox value get request
    void Handle_PostVarValueMeas    (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // handles variable value get request
    void Handle_PostVarValueMailbox (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // handles mailbos value get request
    void Handle_PutVarValue         (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse, std::list<CVarRef *> &rVarList); // handles var/mailbox value put request
    void Handle_PutVarValueMeas     (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // handles variable value put request
    void Handle_PutVarValueMailbox  (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // handles mailbox value put request
    void Handle_PostMamValues       (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // handles variable Min/Average/Max request
    void Handle_PostFirmwareInfo    (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // handles request about available firmware
    void Handle_PostClientConfigInfo(const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // handles request about client firmware
    void Handle_GetForm             (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse, const char *pszFormName); // handles the forms get method
    void Handle_PutForm             (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse, const char *pszFormName); // handles the forms put method
    void Handle_PutAudioRecHeader   (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // audio recording header
    void Handle_PutAudioRecData     (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // audio recording data
    void Handle_PutAudioRecFooter   (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse); // audio recording footer

  };


  
}
