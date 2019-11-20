#pragma once

#include "JsonFormatter.h"
#include "JsonParser.h"
#include "ValuesFromSpec.h"

#include "LogQueue.h"
#include "AbkServerEvent.h"

using boost::asio::ip::tcp;

namespace Abk {

	typedef boost::exception BaseException;

	struct AbkException : virtual BaseException {};
	struct AbkNetworkException : virtual AbkException {};

	// tag is used to differentiate to avoid unintentional casts
	typedef boost::error_info<struct tag_network_info, int> AbkNetworkExceptionInfo;

	class CAbkClient
	{
	public:
		enum LOGSEVERITY // inherit the log severities from the logging queue
		{
			LOGSEVERITY_INFO = CLogQueue<TCHAR>::LOGSEVERITY_INFO,
			LOGSEVERITY_DEBUG = CLogQueue<TCHAR>::LOGSEVERITY_DEBUG,
			LOGSEVERITY_TRACE = CLogQueue<TCHAR>::LOGSEVERITY_TRACE,
			LOGSEVERITY_WARNING = CLogQueue<TCHAR>::LOGSEVERITY_WARNING,
			LOGSEVERITY_ERROR = CLogQueue<TCHAR>::LOGSEVERITY_ERROR
		};

	public:
		boost::asio::io_context io_context;
		tcp::resolver resolver /*(io_context)*/;
		tcp::socket socket /*(io_context)*/;
		tcp::socket socket_long_poll /*(io_context)*/;

	private:
		std::string m_pszServerAddress;
		std::string m_pszPort;
		int m_nSessionId;

	public:
		CAbkClient();

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

		size_t WriteToSocket(tcp::socket & a_Socket, const std::string & a_Path, eHttpRequestType a_Type);

		size_t WriteToSocket(tcp::socket & a_Socket, const std::string & a_Path, const std::string & a_Message, eHttpRequestType a_Type);

		eHttpHeaders GetEnumFromString(const std::string &str) const;

		size_t ReadFromSocket(tcp::socket & a_Socket, std::string & a_Message, unsigned int *pnStatusCode = nullptr);

		size_t ReadFromSocket(tcp::socket & a_Socket, std::ostream & sstream, unsigned int *pnStatusCode = nullptr);

	public:
		bool IsConnected();

	private:

		bool EnsureConnection();

	public:
		CJsonFormatter formatter;
		std::string NavigatePost(LPCTSTR pszPath, int nSessionId, CJsonFormatter *pPostData);

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

		bool NavigateGet(LPCTSTR pszPath, int nSessionId, std::ostream & out);

		void AddLog(LOGSEVERITY nSeverity, LPCTSTR pszMessage, ...);

		int ObtainSessionId(LPCTSTR pszClientClass, LPCTSTR pszClientType, LPCTSTR pszClientSerial /*=NULL*/);

		bool Create(LPCTSTR pszServerAddress, int nPort, CAbkServerEvent *pEventRxBuffer, LPCTSTR pszClientClass, LPCTSTR pszClientType, LPCTSTR pszClientSerial = NULL);

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

		bool DownloadFile(LPCTSTR pszUrl, std::ostream & out);

		bool DownloadFile(LPCTSTR pszUrl, LPCTSTR pszStorePath);

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

		bool SetVarValue(LPCTSTR pszVarName, const double dSet);

		bool GetVarOrMailboxList(LPCTSTR pszPath, std::list<CString> *pGet);

		bool GetVarList(std::list<CString> *pGet);

		int GetSessionId(void);

		virtual void OnLogAdded(void);
	};

} // namespace Abk