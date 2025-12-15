#pragma once

#include "JsonFormatter.h"
#include "JsonParser.h"
#include "ValuesFromSpec.h"

#include "LogQueue.h"
#include "AbkServerEvent.h"
#include "CrossPlatform.h"

#ifndef BOOST_ABK
#error "Boost Abk included, but not specified, probably a mistake"
#endif

using boost::asio::ip::tcp;

#define ABK_AUX_MAXRESPONSE_MS 2000 // response timeout for aux requests (30th June 2021: 500 ms turned out to be too short to query meta data)

namespace Abk {

	class CAbkClientMeta;
	class CAbkClientVar;
	class CAbkClient;
	class CAbkClientDaq;
	class CAbkServerEvent;

	typedef boost::exception BaseException;

	struct AbkException : virtual BaseException {};
	struct AbkNetworkException : virtual AbkException {};

	// tag is used to differentiate to avoid unintentional casts
	typedef boost::error_info<struct tag_network_info, int> AbkNetworkExceptionInfo;

	typedef bool (WINAPI *PFNSTATUSCALLBACK) (DWORD, DWORD_PTR);

	/** Client for communication via OpenABK */
	class CAbkClient
	{
		friend class CAbkClientDaq;

	public:
		class CFormElement // one element of a form
		{
		public:
			enum TYPE { TYPE_INVALID, TYPE_EDIT, TYPE_CHECKBOX, TYPE_COMBO, TYPE_BUTTON, NUMBEROFTYPES }; // form element types
			enum { READONLY = 0x01, PASSWORD = 0x02, NUMERIC = 0x04, SUBMIT = 0x08, CANCEL = 0x10, UPDATEABLE = 0x20 }; // flag attributes
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
			CFormElement();
			~CFormElement();
			// attributes and methods
		public:
			bool DecodeJson(CJsonParserAtl &jpElement); // decodes from JSON
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
			class CBaseAbstraction *m_pClient;
		public:
			boost::atomic<int> m_nUsage; // usage counter

		public:
			CClientPtr() { m_pClient = NULL; m_nUsage = 0; }
			~CClientPtr() {}
			CBaseAbstraction *GetPtr(void) { return m_pClient; }
			//BOOL IsValid (void) const {return m_pClient!=NULL;}
			CClientPtr & operator =(CBaseAbstraction *pSet) { ASSERT(m_pClient == NULL); m_pClient = pSet; return *this; }
			BOOL Delete(void);
		};

		class CClientPtrRef
		{
			CClientPtr &m_rRef;
			CBaseAbstraction *m_pClient;

		public:
			CClientPtrRef(CClientPtr &rClient) :m_rRef(rClient) { m_pClient = rClient.GetPtr(); m_rRef.m_nUsage.fetch_add(1, boost::memory_order_relaxed); }
			~CClientPtrRef() { m_rRef.m_nUsage.fetch_sub(1, boost::memory_order_release); }
			BOOL IsValid(void) const { return m_pClient != NULL; }
			const CBaseAbstraction *operator -> () const { return m_pClient; }
			CBaseAbstraction *operator -> () { return m_pClient; }
		};

		class CClientPtrRefConst
		{
			CClientPtr &m_rRef;
			const CBaseAbstraction *m_pClient;

		public:
			CClientPtrRefConst(const CClientPtr &rClient) :m_rRef(const_cast<CClientPtr &>(rClient)) { m_pClient = m_rRef.GetPtr(); m_rRef.m_nUsage.fetch_add(1, boost::memory_order_relaxed); }
			~CClientPtrRefConst() { m_rRef.m_nUsage.fetch_sub(1, boost::memory_order_release); }
			BOOL IsValid(void) const { return m_pClient != NULL; }
			const CBaseAbstraction *operator -> () const { return m_pClient; }
		};

		enum LOGSEVERITY // inherit the log severities from the logging queue
		{
			LOGSEVERITY_INFO = CLogQueue<TCHAR>::LOGSEVERITY_INFO,
			LOGSEVERITY_DEBUG = CLogQueue<TCHAR>::LOGSEVERITY_DEBUG,
			LOGSEVERITY_TRACE = CLogQueue<TCHAR>::LOGSEVERITY_TRACE,
			LOGSEVERITY_WARNING = CLogQueue<TCHAR>::LOGSEVERITY_WARNING,
			LOGSEVERITY_ERROR = CLogQueue<TCHAR>::LOGSEVERITY_ERROR
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
      CThreadLauncher m_thread;
      class CBaseAbstraction *m_pClient;
      static DWORD WINAPI RequestThreadS(void *vpThis);

    public:
      CTempConnection(class CBaseAbstraction *pClient);
      ~CTempConnection();
    };

  // data members
	private:
		bool m_bTerminateLongPoll; // true if long-polling thread shall terminate
		CClientPtr m_pClientAux; // auxiliary client for blocking non-long-polling actions
		CClientPtr m_pClientEvent; // auxiliary client for long-polling data and event transfer actions
		std::map<std::string, CAbkClientDaq *> m_mapDaq; // DAQ lists currently used to transfer data
		std::string m_strServerAddress;
		std::string m_strPort;
		std::string m_strClientClass; // class string of client
		std::string m_strClientType; // type string of client
		std::string m_strClientSerial; // serial number string of client
    std::string m_strClientFwRev;      // firmware revision string
    std::string m_strClientHwRev;      // hardware revision string
    int m_nPort;
    int m_nSessionId;

		CAbkMutex m_mutexDaq; // mutex to protect the daq items
		CAbkServerEvent *m_pNextEventData; // when event is received, it will be stored to this location. Will not be deleted on destruction!
		bool m_bSuppressLog; // true suppresses log file output (currently not implemented)
		bool m_bTextTranslationByServer; // true requests the server to translate values in text, if applicable. false instructs the server to send non-translated values

		boost::thread m_longPollThread;

  public:
		void AddLog(LOGSEVERITY nSeverity, LPCTSTR pszMessage, ...);

	// construction/destruction/setup
	public:
		CAbkClient(bool bSuppressLog = false, bool bTextTranslationByServer = true);
		virtual ~CAbkClient();
		bool Create(LPCTSTR pszServerAddress, int nPort, CAbkServerEvent *pEventRxBuffer, LPCTSTR pszClientClass, LPCTSTR pszClientType, LPCTSTR pszClientSerial = NULL, LPCTSTR pszClientFwRev = NULL, LPCTSTR pszClientHwRev = NULL); // creates the client and initializes

		/** Called when server sent an event.

			You can override this function to receive events.
			This function is called in the long-polling-thread's context.

			@param pEventData
				Pointer to event with the params of the event
			@return
				Point to event object, receiving the next event.
			@remark
				You can either return the same event object when you have only
				one buffer for event reception. Alternatively you can return a
				pointer to another event object if you have a queue. In this case
				pEventData must be deleted manually
		*/
		virtual CAbkServerEvent *OnServerEvent(CAbkServerEvent *pEventData);



		public:
			void AddLogHttp(LOGSEVERITY nSeverity, int nHttpStatusCode, LPCTSTR pszUrl, LPCTSTR pszMethod, const char *pcszResponse, const char *pcszoPostPutData = NULL);

		// blocking ABK methods
		public:

		void TidyUp(bool bLostConnection); // cleans object
		void SetServerAddr(LPCTSTR pszServerAddress, int nPort); // re-assigns the server address and port
		const CString GetServerAddr(void) const { return (CString)m_strServerAddress.c_str(); } // returns server address string
		int GetServerPort(void) const; // returns port of server connection
		int GetSessionId(void);

		// sends a client event to the server
		bool SendEvent(const char * pszEventType, LPCTSTR pszStringParam, double dParam1, double dParam2, bool bPrivate);
		// sends a client event to the server
		bool SendEvent(const char * pszEventType, CJsonFormatter & jfString, double dParam1, double dParam2, bool bPrivate);

		bool GetClientConfigInfo(CString &strUrl, CString &strMd5, LPCTSTR pszClientType/*=NULL*/);
		bool GetForm(LPCTSTR pszFormName, std::vector<CFormElement> &vectGet, CString &strCaptionGet, int &nPersitenceMs);



		bool SetVarValue(LPCTSTR pszVarName, const CString &strSet); // sets a variable value
		bool SetVarValue(LPCTSTR pszVarName, const std::string &strSet); // sets a variable value
		bool SetVarValue(LPCTSTR pszVarName, double dSet); // sets a variable value
		bool SetVarValue(LPCTSTR pszVarName, int nSet); // sets a variable value
		bool SetVarValue(LPCTSTR pszVarName, bool bSet); // sets a variable value

		bool SetMailboxValue(LPCTSTR pszMailboxName, const CString &strSet); // sets a mailbox value
		bool SetMailboxValue(LPCTSTR pszMailboxName, const std::string &strSet); // sets a mailbox value
		bool SetMailboxValue(LPCTSTR pszMailboxName, double dSet); // sets a mailbox value
		bool SetMailboxValue(LPCTSTR pszMailboxName, int nSet); // sets a mailbox value
		bool SetMailboxValue(LPCTSTR pszMailboxName, bool bSet); // sets a mailbox value

		bool GetCurrentServerTime(time_t *pGet);
		bool GetMailboxList(std::vector<CString> *pGet);
		bool GetMailboxMeta(const std::vector<LPCTSTR> &vectMailboxNames, std::vector<CAbkClientMeta> *pGet);
		bool GetMailboxMeta(LPCTSTR pszMailboxName, CAbkClientMeta *pGet);

		bool GetVarMeta(const std::vector<LPCTSTR> &vectVarNames, std::vector<CAbkClientMeta> *pGet);
		bool GetVarMeta(LPCTSTR pszVarName, CAbkClientMeta *pGet);


		bool SendForm(LPCTSTR pszFormName, const std::vector<CFormElement> &vectSend);
		bool SendAudioRecHeader(int nId, int nSampleRateHz, int nBitsPerSample, int nChannels);
		bool SendAudioRecData(int nId, const void *pData, int nBitsPerSample, int nChannels, int nSamplesPerChannel);
		bool SendAudioRecFooter(int nId);
		bool SendAudioRecRejectEvent(int nId);
		bool TextTranslationByServer (void) const; // returns whether the server will be instructed to translate values into text representation

		bool SuspendLongPolling(void);
		bool ResumeLongPolling(void);

		bool AddDaq(CAbkClientDaq *pAdd); // adds a daq list
		CAbkClientDaq *FindDaq(const std::string &strDaqName); // searches for a DAQ

		BOOL PopLog(LOGSEVERITY &nSeverityGet, CString &strMessageGet); // pops one entity from the error log

		bool SendButtonEvent(LPCTSTR pszButtonName, bool bPressedState, int nTime, bool bPrivate); // sends a button press/release event to the server

		std::string GetClientState(LPCTSTR pszFileExtension); // reads client configuration from server
		bool SetClientState(const char *pConfigString, LPCTSTR pszFileExtension); // writes client configuration to server


		virtual void OnLogAdded(void);

		virtual DWORD OnLongPollErrorResponse(int nHttpStatusCode, int nSessionId);

		bool GetVarList(std::vector<CString> *pGet);

		bool GetClientFirmwareInfo(std::vector<CFirmwareInfo> &vectGet, LPCTSTR pszClientType = NULL); // queries the available client firmware information

		bool DownloadFile(LPCTSTR pszUrl, std::ostream & out, PFNSTATUSCALLBACK pfnReadCallback = NULL, DWORD_PTR dwCookie = 0);
		bool DownloadFile(LPCTSTR pszUrl, LPCTSTR pszStorePath, PFNSTATUSCALLBACK pfnReadCallback = NULL, DWORD_PTR dwCookie = 0);

		std::string GetServerInfo(void);
		bool GetServerInfo(CString &strProtocolVersion, CString &strInterfaceVersion, CString &strFwVersion, CString &strHwVersion, CString &strServerName, CString &strServerType, CString &strDescUrl); // retrieves information from server
		std::string GetInterfaceStatistics(void); // returns interface statistics of server as json formatted string

		bool SendAlertConfirmEvent(LPCTSTR pszAlertClassName, int nSeverity, int nMerged, bool bPermanent, bool bSuppressed, bool bTimeout); // sends confirmation event to server: user has confirmed an event


		bool ReceiveEvent();

		bool IsConnected() const;

		// implementation
		protected:
			static DWORD WINAPI LongPollThreadS(void *vpThis); // long polling thread static function
			int LongPollThread(void); // long polling thread

			CAbkEvent m_evLongPollEnable;
			CAbkEvent m_evLongPollDone;
	};

} // namespace Abk