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
// |  __| |   ´   ||  -  /| |  | |  __  \___ \ | | | |/ __|
// | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
// |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
//                                              |___/      
// Filename:    AbkClient.h
// Created:     2012-08-02 (07:48)
// Author:      D. Burger
// Description: client provinding api to an ABK server
//------------------------------------------------------------------------------------------------



#pragma once

#include <atlhttp.h>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include "CrossPlatform.h"
#include "LogQueue.h"
#include <afxmt.h>
#include <comutil.h>


#define ABK_AUX_MAXRESPONSE_MS 2000 // response timeout for aux requests (30th June 2021: 500 ms turned out to be too short to query meta data)


enum
  {
  LOCK_HIER_ABK_STARTOF=0x10000,
  LOCK_HIER_ABK_CLIENTPTR,
  LOCK_HIER_ABK_NAVIGATE,
  };


class CJsonFormatter;
class CJsonParserAtl;


namespace Abk
  {

  class CAbkClientMeta;
  class CAbkClient;
  class CAbkClientDaq;
  class CAbkServerEvent;



  class CAbkClient
    {
    friend class CAbkClientDaq;
    private:

    // HTTP client class
    class CBaseAbstraction : private CAtlHttpClient
      {
      friend class CAbkClientDaq;
      friend class CAbkClient;
      // data members
      private:
        LPCTSTR m_pszServerAddress; // c-string of the server address
        int m_nPort; // port at the server. it is used to initialize the CAtlNavigateData
        DWORD m_dwTimeout; // timeout for read requests on http
        CAbkClient *m_pOwner; // pointer to owning container object
        // CAtlNavigateData m_nav; // navigation information
        CMyCriticalSection m_csNavigate; // prevent Navigate() from beeing called in different contexts
      // construction/destruction/setup
      private:
        CBaseAbstraction (CAbkClient *pOwner);
        virtual ~CBaseAbstraction ();
      // methods
      public:
      // implementation
      protected:
        void SetServerAddr (LPCTSTR pszServerAddress, int nPort); // re-assigns the server address and port
        int GetPort (void) const {return m_nPort;} // returns port
        void SetTimeout (DWORD dwNewTimeout); // sets the timeout for reads on http requests
        DWORD GetTimeout (void) const; // returns timeout for reads on http requests
        bool NavigateX (LPCTSTR pszServer, LPCTSTR pszPath, ATL_NAVIGATE_DATA *pNavData);
        const char *NavigateGet (LPCTSTR pszPath, int nSessionId); // sends GET request and waits for answer
        bool        NavigateGet (LPCTSTR pszPath, int nSessionId, LPCTSTR pszStorePath, PFNATLSTATUSCALLBACK pfnReadCallback=NULL, DWORD_PTR dwCookie=0); // sends a POST request and waits for answer, stores result as file in the local file system
        bool        NavigateGet (LPCTSTR pszPath, int nSessionId, CFile *pFileDest, PFNATLSTATUSCALLBACK pfnReadCallback=NULL, DWORD_PTR dwCookie=0); // sends a POST request and waits for answer, stores result to a file
        const char *NavigatePost (LPCTSTR pszPath, int nSessionId, CJsonFormatter *pPostData); // sends a POST request and waits for answer
        bool        NavigateDelete (LPCTSTR pszPath, int nSessionId, CJsonFormatter *pPostData); // sends a DELETE request
        const char *NavigatePut (LPCTSTR pszPath, int nSessionId, CJsonFormatter *pPutData); // sends a PUT request
        const char *NavigatePut (LPCTSTR pszPath, int nSessionId, const char *pPutData, int nLen, LPCTSTR pszMimeType); // sends a PUT request
        const char *GetBodySave (void); // returns last response data, empty string on no data
        int ObtainSessionId (LPCTSTR pszClientClass, LPCTSTR pszClientType, LPCTSTR pszClientSerial, LPCTSTR pszClientFwRev, LPCTSTR pszClientHwRev); // generates session at the server
        bool DeleteSession (int nSessionId); // deletes the actual session
        static bool DecodeDate (LPCTSTR pszDate, CTime *pResult); // decodes date into a CTime
        bool GetLastModified (LPCTSTR pszUrl, CTime *pGet); // returns the last modified date of a file by given url
        template <typename U> bool GetVarOrMailboxValue (LPCTSTR pszPath, const char *pszName, U *pGet); // queries value of variable or mailbox
        template <typename U> bool SetVarOrMailboxValue (LPCTSTR pszPath, const char *pszName, const U *pSet); // sets value of variable or mailbox
        bool GetVarOrMailboxMeta (const std::vector<LPCTSTR> &lstVarNames, std::vector<CAbkClientMeta> *pGet, bool bMailboxFlag); // requests meta data of a variables or mailboxes
        bool GetVarOrMailboxList (LPCTSTR pszPath, std::vector<CString> *pGet); // requests list of variables or mailboxes
      };

    public:
    class CFormElement // one element of a form
      {
      public:
        enum TYPE {TYPE_INVALID, TYPE_EDIT, TYPE_CHECKBOX, TYPE_COMBO, TYPE_BUTTON, NUMBEROFTYPES}; // form element types
        enum {READONLY=0x01, PASSWORD=0x02, NUMERIC=0x04, SUBMIT=0x08, CANCEL=0x10, UPDATEABLE=0x20}; // flag attributes
      // data members
      public:
        TYPE m_nType; // type of form element
        CString m_strName; // name of the entity
        CString m_strCaption; // caption of the entity
        std::vector<CString> m_vectOptions; // options for list/combo
        _variant_t m_varValue; // value of the element
        int m_nMaxLen; // maximum length for edit type. 0 if no limitation desired
        UINT m_nFlags; // attribute-flags
      // constructon/destruction
      public:
        CFormElement ();
        ~CFormElement ();
      // attributes and methods
      public:
        bool DecodeJson (CJsonParserAtl &jpElement); // docodes from JSON
      };

    public:
      struct CFirmwareInfo
        {
        CString m_strVersion; // version of firmware
        CString m_strUrl; // url where to download the firmware
        CString m_strMd5; // md5 of the firmware file
        };

    public:
      class CClientPtr
        {
        //class CClientPtrRef;
        //friend class CClientPtrRef;
        CBaseAbstraction *m_pClient;
        public:
        int m_nUsage; // usage counter
        CMyCriticalSection m_csUsage; // protecting the counter

        public:
        CClientPtr () :m_csUsage(LOCK_HIER_ABK_CLIENTPTR,_T("Abk::CAbkClient::CClientPtr")) {m_pClient=NULL; m_nUsage=0;}
        ~CClientPtr () {Delete();}
        CBaseAbstraction *GetPtr (void) {return m_pClient;}
        //BOOL IsValid (void) const {return m_pClient!=NULL;}
        CClientPtr & operator =(CBaseAbstraction *pSet) {ASSERT(m_pClient==NULL); m_pClient=pSet; return *this;}
        BOOL Delete (void);
        };

      class CClientPtrRef
        {
        CClientPtr &m_rRef;
        CBaseAbstraction *m_pClient;

        public:
        CClientPtrRef (CClientPtr &rClient) :m_rRef(rClient) {m_pClient=rClient.GetPtr(); CLockMyCriticalSection guard(m_rRef.m_csUsage,_T("CClientPtrRef()")); m_rRef.m_nUsage++;}
        ~CClientPtrRef () {CLockMyCriticalSection guard(m_rRef.m_csUsage,_T("~CClientPtrRef()")); m_rRef.m_nUsage--;}
        BOOL IsValid (void) const {return m_pClient!=NULL;}
        const CBaseAbstraction *operator -> () const {return m_pClient;}
              CBaseAbstraction *operator -> ()       {return m_pClient;}
        };

      class CClientPtrRefConst
        {
        CClientPtr &m_rRef;
        const CBaseAbstraction *m_pClient;

        public:
        CClientPtrRefConst (const CClientPtr &rClient) :m_rRef(const_cast<CClientPtr &>(rClient)) {m_pClient=m_rRef.GetPtr(); CLockMyCriticalSection guard(m_rRef.m_csUsage,_T("~CClientPtrRefConst()")); m_rRef.m_nUsage++;}
        ~CClientPtrRefConst () {CLockMyCriticalSection guard(m_rRef.m_csUsage,_T("~CClientPtrRef()")); m_rRef.m_nUsage--;}
        BOOL IsValid (void) const {return m_pClient!=NULL;}
        const CBaseAbstraction *operator -> () const {return m_pClient;}
        };

      enum LOGSEVERITY  // inherit the log severities from the logging queue
        {
        LOGSEVERITY_INFO    =CLogQueue<TCHAR>::LOGSEVERITY_INFO    ,
        LOGSEVERITY_DEBUG   =CLogQueue<TCHAR>::LOGSEVERITY_DEBUG   ,
        LOGSEVERITY_TRACE   =CLogQueue<TCHAR>::LOGSEVERITY_TRACE   ,
        LOGSEVERITY_WARNING =CLogQueue<TCHAR>::LOGSEVERITY_WARNING ,
        LOGSEVERITY_ERROR   =CLogQueue<TCHAR>::LOGSEVERITY_ERROR
        };

      /** temporary connection used to allocate ephemeral ports for the regular operation
      @note When disgracefully disconnecting the client from the server, the server starts to retry on the lost connection.
       Later, when the client restarts, it tries to establish connection(s) with the same ephemeral ports of the previous run.
       This leads to an error with an http time-out.
      @note In order to avoid using general retries, the temporary connection will be used to switch to the next ephemeral ports,
       hence allowing the connection of the normal operation to succeed immediately.
      */
      class CTempConnection
      {
        enum
        {
          REQUEST_COUNT = 3
        };
        HANDLE m_hThread;
        CAbkClient::CBaseAbstraction* m_pClient;
        static DWORD WINAPI RequestThreadS(void* vpThis);
      public:
        CTempConnection(CBaseAbstraction* pClient);
        ~CTempConnection();
      };

    // data members
    private:
      CLogQueue<TCHAR> m_queueLog; // error logging
      CString m_strServerAddress; // address of the server, e.g. "192.168.178.22".
      CString m_strClientClass; // class string of client
      CString m_strClientType; // type string of client
      CString m_strClientSerial; // serial number string of client
      CString m_strClientFwRev; // firmware revision string
      CString m_strClientHwRev; // hardware revision string
      int m_nPort; // current used port
      int m_nSessionId; // id of the client session, -1 if no session could be created
      CClientPtr m_pClientAux; // auxiliary client for blocking non-long-polling actions
      CClientPtr m_pClientEvent; // auxiliary client for long-polling data and event transfer actions
      std::map<std::string,CAbkClientDaq *> m_mapDaq; // DAQ lists currently used to transfer data
      HANDLE m_hLongPollThread; // handle of the running long-poll-thread
      ::CEvent m_evLongPollEnable; // if set, long polling is performed, if reset, long polling is stalled
      ::CEvent m_evLongPollDone; // event gets set when long poll thread terminates
      //bool m_bLongPollRunning; // true as long the long polling thread is running
      bool m_bTerminateLongPoll; // true if long-polling thread shall terminate
      CAbkMutex m_mutexDaq; // mutex to protect the daq items
      CAbkServerEvent *m_pNextEventData; // when event is received, it will be stored to this location. Will not be deleted on destruction!
      bool m_bSuppressLog; // true suppresses log file output
      bool m_bTextTranslationByServer; // true requests the server to translate values in text, if applicable. false instructs the server to send non-translated values

    // construction/destruction/setup
    public:
      CAbkClient (bool bSuppressLog = false, bool bTextTranslationByServer = true);
      virtual ~CAbkClient ();
      bool Create (LPCTSTR pszServerAddress, int nPort, CAbkServerEvent *pEventRxBuffer, LPCTSTR pszClientClass, LPCTSTR pszClientType, LPCTSTR pszClientSerial, LPCTSTR pszClientFwRev=NULL, LPCTSTR pszClientHwRev=NULL); // creates the client and initializes

    // state and control
    public:
      bool IsConnected (void) const; // returns true if connected to a server
      bool SuspendLongPolling (void); // pauses the long-poll thread
      bool ResumeLongPolling (void); // resumes long poll thread

    // blocking ABK methods
    public:
      void TidyUp (bool bLostConnection); // cleans object
      void SetServerAddr (LPCTSTR pszServerAddress, int nPort); // re-assigns the server address and port
      const CString &GetServerAddr (void) const {return m_strServerAddress;} // returns server address string
      int GetServerPort (void) const; // returns port of server connection
      int GetSessionId (void) {return m_nSessionId;} // returns the session id, -1 if no session created
      bool GetCurrentServerTime (time_t *pGet); // retrieves the current time of the server
      bool GetStorageInfo (unsigned long long &rTotal, unsigned long long &rFree); // requests storage info of the server
      bool GetLastModified (LPCTSTR pszUrl, CTime *pGet); // returns the last modified date of a file by given url
      bool SendEvent (const char *pszEventType, LPCTSTR pszStringParam, double dParam1, double dParam2, bool bPrivate); // sends a client event to the server
      bool SendEvent (const char *pszEventType, CJsonFormatter &jfString, double dParam1, double dParam2, bool bPrivate); // sends a client event to the server
      bool SendButtonEvent (LPCTSTR pszButtonName, bool bPressedState, int nTime, bool bPrivate); // sends a button press/release event to the server
      bool SendAlertConfirmEvent (LPCTSTR pszAlertClassName, int nSeverity, int nMerged, bool bPermanent, bool bSuppressed, bool bTimeout); // sends confirmation event to server: user has confirmed an event
      bool GetVarValue (LPCTSTR pszVarName, CString *pGet); // queries a variable value
      bool GetVarValue (LPCTSTR pszVarName, std::string *pGet); // queries a variable value
      bool GetVarValue (LPCTSTR pszVarName, double *pGet); // queries a variable value
      bool GetVarValue (LPCTSTR pszVarName, int *pGet); // queries a variable value
      bool GetVarValue (LPCTSTR pszVarName, bool *pGet); // queries a variable value
      bool GetVarValue (LPCTSTR pszVarName, CTime *pGet); // queries a variable value
      bool GetMailboxValue (LPCTSTR pszMailboxName, CString *pGet); // queries a mailbox value
      bool GetMailboxValue (LPCTSTR pszMailboxName, std::string *pGet); // queries a mailbox value
      bool GetMailboxValue (LPCTSTR pszMailboxName, double *pGet); // queries a mailbox value
      bool GetMailboxValue (LPCTSTR pszMailboxName, int *pGet); // queries a mailbox value
      bool GetMailboxValue (LPCTSTR pszMailboxName, bool *pGet); // queries a mailbox value
      bool GetMailboxValue (LPCTSTR pszMailboxName, CTime *pGet); // queries a mailbox value
      bool SetVarValue (LPCTSTR pszVarName, const CString &strSet); // sets a variable value
      bool SetVarValue (LPCTSTR pszVarName, const std::string &strSet); // sets a variable value
      bool SetVarValue (LPCTSTR pszVarName, double dSet); // sets a variable value
      bool SetVarValue (LPCTSTR pszVarName, int nSet); // sets a variable value
      bool SetVarValue (LPCTSTR pszVarName, bool bSet); // sets a variable value
      bool SetVarValue (LPCTSTR pszVarName, const CTime &tmSet); // sets a variable value
      bool SetMailboxValue (LPCTSTR pszMailboxName, const CString &strSet); // sets a mailbox value
      bool SetMailboxValue (LPCTSTR pszMailboxName, const std::string &strSet); // sets a mailbox value
      bool SetMailboxValue (LPCTSTR pszMailboxName, double dSet); // sets a mailbox value
      bool SetMailboxValue (LPCTSTR pszMailboxName, int nSet); // sets a mailbox value
      bool SetMailboxValue (LPCTSTR pszMailboxName, bool bSet); // sets a mailbox value
      bool SetMailboxValue (LPCTSTR pszMailboxName, const CTime &tmSet); // sets a mailbox value
      bool GetVarMeta (const std::vector<LPCTSTR> &lstVarNames, std::vector<CAbkClientMeta> *pGet); // requests meta data of one or more variables
      bool GetVarMeta (LPCTSTR pszVarName, CAbkClientMeta *pGet); // requests meta data of a variable
      bool GetMailboxMeta (const std::vector<LPCTSTR> &vectMailboxNames, std::vector<CAbkClientMeta> *pGet); // requests meta data of a mailbox
      bool GetMailboxMeta (LPCTSTR pszMailboxName, CAbkClientMeta *pGet); // requests meta data of a mailbox
      bool GetVarList (std::vector<CString> *pGet); // requests list of variables
      bool GetMailboxList (std::vector<CString> *pGet); // requests list of mailboxes
      bool GetForm (LPCTSTR pszFormName, std::vector<CFormElement> &lstGet, CString &strCaptionGet, int &nPersitenceMs); // requests a form
      bool SendForm (LPCTSTR pszFormName, const std::vector<CFormElement> &lstSend); // sends a form
      const char *GetClientState (LPCTSTR pszFileExtension); // reads client configuration from server
      bool SetClientState (const char *pConfigString, LPCTSTR pszFileExtension); // writes client configuration to server
      bool GetClientConfigInfo (CString &strUrl, CString &strMd5, LPCTSTR pszClientType=NULL); // queries the client configuration/app information
      bool GetClientFirmwareInfo (std::vector<CFirmwareInfo> &lstGet, LPCTSTR pszClientType=NULL); // queries the available client firmware information
      bool DownloadFile (LPCTSTR pszUrl, LPCTSTR pszStorePath, PFNATLSTATUSCALLBACK pfnReadCallback=NULL, DWORD_PTR dwCookie=0); // downloads a file
      bool DownloadFile (LPCTSTR pszUrl, CFile &fileStore, PFNATLSTATUSCALLBACK pfnReadCallback=NULL, DWORD_PTR dwCookie=0); // downloads a file
      const char *GetServerInfo (void); // returns server information as json formatted string
      bool GetServerInfo (CString &strProtocolVersion, CString &strInterfaceVersion, CString &strFwVersion, CString &strHwVersion, CString &strServerName, CString &strServerType, CString &strDescUrl); // retrieves information from server
      const char *GetInterfaceStatistics (void); // returns interface statistics of server as json formatted string
      bool SendAudioRecHeader (int nId, int nSampleRateHz, int nBitsPerSample, int nChannels); // sends an audio header
      bool SendAudioRecData (int nId, const void *pData, int nBitsPerSample, int nChannels, int nSamplesPerChannel); // sends audio data
      bool SendAudioRecFooter (int nId);
      bool SendAudioRecRejectEvent (int nId); // sends event that user rejected audio recording
      bool TextTranslationByServer (void) const; // returns whether the server will be instructed to translate values into text representation

    // error log
    public:
      void AddLog (LOGSEVERITY nSeverity, LPCTSTR pszMessage, ...); // writes one line to error log
      void AddLogV (LOGSEVERITY nSeverity, LPCTSTR pszMessage, va_list args); // writes one line to error log
      void AddLogHttp (LOGSEVERITY nSeverity, int nHttpStatusCode, LPCTSTR pszUrl, LPCTSTR pszMethod, const char *pcszResponse, const char *pcszoPostPutData=NULL); // writes one line to error log containing HTTP info
      BOOL PopLog (LOGSEVERITY &nSeverityGet, CString &strMessageGet); // pops one entity from the error log
      virtual void OnLogAdded (void); // notifies that the log queue has got new entities. may be called in any thread context!

    // overridables
    protected:
      virtual void OnServerDataBegin (void); // called before data of a DAQ will be dispatched through the virtual functions of CAbkClientDaq
      virtual void OnServerDataEnd (void); // called after data of a DAQs were dispatched. Counterpart to OnServerDataBegin()
      virtual CAbkServerEvent *OnServerEvent (CAbkServerEvent *pEventData); // called when server sent an event
      virtual DWORD OnLongPollErrorResponse (int nHttpStatusCode, int nSessionId); // gets called when a server responds with an error status code. Gets called from the long-poll thread context!

    // event and daq methods  
    public:
      bool AddDaq (CAbkClientDaq *pAdd); // adds a daq list
      bool DeleteDaq (LPCTSTR pszDaqName); // deletes a daq list
      bool DeleteDaq (CAbkClientDaq *pDelete); // deletes a daq list
      CAbkClientDaq *FindDaq (LPCTSTR pszDaqName); // searches for a DAQ

    // implementation
    protected:
      static DWORD WINAPI LongPollThreadS (void *vpThis); // long polling thread static function
      int LongPollThread (void); // long polling thread
      CAbkClientDaq *FindDaq (const std::string &strDaqName); // searches for a DAQ
    };


  } // namespace
