#pragma once

#include "JsonFormatter.h"
#include "JsonParser.h"
#include "ValuesFromSpec.h"

#include "LogQueue.h"
#include "AbkServerEvent.h"

#ifndef BOOST_ABK
#error "Boost Abk included, but not specified, probably a mistake"
#endif

using boost::asio::ip::tcp;

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

	class CAbkClient
	{
	public:
// HTTP client class
		class CBaseAbstraction
		{
			friend class CAbkClientDaq;
			friend class CAbkClient;

// Definitions
		private:
			enum eHttpRequestType {
				E_HTTP_GET,
				E_HTTP_POST,
				E_HTTP_PUT
			};


			enum eHttpHeaders {
				E_HEADER_INVALID,
				E_HEADER_CONTENT_LENGTH,
				E_HEADER_CONNECTION
			};

			/** Converts a string to lower case before comparing */
			struct insensitive_hash {
				std::size_t operator ()(const std::string &value) const;
			};


			typedef boost::unordered_map<std::string, eHttpHeaders, insensitive_hash> HashMap;

			// Maps a string to enum values
			const HashMap m_HashMap = {
				{"Content-Length", E_HEADER_CONTENT_LENGTH},
				{"Connection", E_HEADER_CONNECTION}
			};

// data members
		private:
			std::string m_strServerAddress; // c-string of the server address
			std::string m_strPort;
			int m_nPort; // port at the server. it is used to initialize the CAtlNavigateData
			CAbkClient *m_pOwner; // pointer to owning container object
			// CAtlNavigateData m_nav; // navigation information
			//CMyCriticalSection m_csNavigate; // prevent Navigate() from beeing called in different contexts
			CAbkMutex m_mutex;
		// construction/destruction/setup
			CJsonFormatter formatter;
		public:
			CBaseAbstraction(CAbkClient *pOwner);
			virtual ~CBaseAbstraction();
			// methods
		public:

// implementation
		protected:
			void SetServerAddr(const std::string &pszServerAddress, int nPort); // re-assigns the server address and port
			int GetPort(void) const { return m_nPort; } // returns port
			// TODO: MIME type
			bool NavigatePut(LPCTSTR pszPath, int nSessionId, CJsonFormatter *pPutData);
			/** Used to retrieve a string at a certain path
				@param pszPath
					Path on the server
				@param nSessionId
					Session ID to use in query, set to -1 to query without
				@return
					Response string
			 */
			std::string NavigateGet(LPCTSTR pszPath, int nSessionId);
			bool        NavigateGet(LPCTSTR pszPath, int nSessionId, std::ostream & out);
			std::string NavigatePost(LPCTSTR pszPath, int nSessionId, CJsonFormatter *pPostData);
			bool DeleteSession(int nSessionId); // deletes the actual session

			template <typename U>
			bool SetVarOrMailboxValue(LPCTSTR pszPath, const char *pszName, const U *pSet)
			{
				CJsonFormatter jfRequest;
				{
					CJsonStreamArray jaGetList(&jfRequest, ABK_REQ_VARVALUE_PUTLIST);
					{
						CJsonStreamObject joVarPut(&jaGetList);
						joVarPut.WriteValue(ABK_REQ_VARVALUE_NAME, pszName);
						joVarPut.WriteValue(ABK_REQ_VARVALUE_VALUE, *pSet);
					}
				} // let array object fall out of scope
				jfRequest.Close();
				return NavigatePut(pszPath, -1, &jfRequest);
			}

			bool GetVarOrMailboxList(LPCTSTR pszPath, std::list<CString> *pGet);
			int ObtainSessionId(LPCTSTR pszClientClass, LPCTSTR pszClientType, LPCTSTR pszClientSerial /*=NULL*/);



		protected:
// functions similar to ATL
			void Close();

// internal functions
			bool EnsureConnection();

			eHttpHeaders GetEnumFromString(const std::string &str) const;

			size_t WriteToSocket(tcp::socket & a_Socket, const std::string & a_Path, eHttpRequestType a_Type);
			size_t WriteToSocket(tcp::socket & a_Socket, const std::string & a_Path, const std::string & a_Message, eHttpRequestType a_Type);

			size_t ReadFromSocket(tcp::socket & a_Socket, std::string & a_Message, unsigned int *pnStatusCode = nullptr);
			size_t ReadFromSocket(tcp::socket & a_Socket, std::ostream & sstream, unsigned int *pnStatusCode = nullptr);

			bool IsSocketOpen() const;

		public:
			boost::asio::io_context io_context;
			tcp::resolver resolver /*(io_context)*/;
			tcp::socket socket /*(io_context)*/;
		};

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
			std::list<CString> m_lstOptions; // options for list/combo
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
			CBaseAbstraction *m_pClient;
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



	private:
		std::string m_strServerAddress;
		std::string m_strPort;
		int m_nPort;
		int m_nSessionId;

		void AddLog(LOGSEVERITY nSeverity, LPCTSTR pszMessage, ...);

	// construction/destruction/setup
	public:
		CAbkClient();
		virtual ~CAbkClient();
		bool Create(LPCTSTR pszServerAddress, int nPort, CAbkServerEvent *pEventRxBuffer, LPCTSTR pszClientClass, LPCTSTR pszClientType, LPCTSTR pszClientSerial = NULL); // creates the client and initializes


		std::string GetServerInfo(void);



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

		// data members
		private:
			bool m_bTerminateLongPoll; // true if long-polling thread shall terminate
			CClientPtr m_pClientAux; // auxiliary client for blocking non-long-polling actions
			CClientPtr m_pClientEvent; // auxiliary client for long-polling data and event transfer actions
			std::map<std::string, CAbkClientDaq *> m_mapDaq; // DAQ lists currently used to transfer data

		// blocking ABK methods
		public:

		void TidyUp(bool bLostConnection); // cleans object
		void SetServerAddr(LPCTSTR pszServerAddress, int nPort); // re-assigns the server address and port
		const CString &GetServerAddr(void) const { return (CString)m_strServerAddress.c_str(); } // returns server address string
		int GetPort(void) const; // returns port of server connection
		int GetSessionId(void);



		bool SetVarValue(LPCTSTR pszVarName, const CString &strSet); // sets a variable value
		bool SetVarValue(LPCTSTR pszVarName, const std::string &strSet); // sets a variable value
		bool SetVarValue(LPCTSTR pszVarName, double dSet); // sets a variable value
		bool SetVarValue(LPCTSTR pszVarName, int nSet); // sets a variable value
		bool SetVarValue(LPCTSTR pszVarName, bool bSet); // sets a variable value


		virtual void OnLogAdded(void);

		virtual DWORD OnLongPollErrorResponse(int nHttpStatusCode, int nSessionId);

		bool GetVarList(std::list<CString> *pGet);

		bool DownloadFile(LPCTSTR pszUrl, std::ostream & out);
		bool DownloadFile(LPCTSTR pszUrl, LPCTSTR pszStorePath);

		bool ReceiveEvent();

		bool IsConnected() const;
	};

} // namespace Abk