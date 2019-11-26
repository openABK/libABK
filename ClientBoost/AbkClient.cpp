#include "stdafx.h"

#include "AbkClient.h"
#include "AbkClientDaq.h"
#include "JsonParserAtl.h"
#include "StopWatch.h"

#define DAQ_TIMEOUT 10000 // mutex timeout in ms
#define MIME_TYPE_TEXT "text/plain"

#define LONGPOLL_FAILURE_TOLERANCE_MS 2000 // maximum time span in ms where errors are tolerated
#define LONGPOLL_FAILURE_RECOVERY_MS 100 // 500 // time the longpoll thread is stalled after an http error occured. Used to reduce error log entry rate

//#define LOG_BOOST_ABK
// LOG_BOOST_ABK is defined in case you want logging
#ifndef LOG_BOOST_ABK

struct CustomLog
{
	template<typename T>
	CustomLog& operator << (T &stream)
	{
		return *this;
	}
};

// define std::endl for CustomLog
namespace std
{
	inline CustomLog& endl(CustomLog& stream)
	{
		return stream;
	}
}

CustomLog nullStream;

static inline CustomLog& Log()
{
	return nullStream;
}

static inline CustomLog& LogErr()
{
	return nullStream;
}

#else

static inline std::ostream& Log()
{
	return std::cout;
}

static inline std::ostream& LogErr()
{
	return std::cerr;
}

#endif


namespace Abk
{

//----------------------------------------------------------------------

CAbkClient::CBaseAbstraction::CBaseAbstraction(CAbkClient *pOwner): 
	resolver(io_context), socket(io_context)
{
	m_pOwner = pOwner;
	m_nPort = 0;
	m_bConnected = false;
}

CAbkClient::CBaseAbstraction::~CBaseAbstraction()
{
}

void CAbkClient::CBaseAbstraction::SetServerAddr(const std::string &strServerAddress, int nPort)
{
	m_strServerAddress = strServerAddress;
	m_nPort = nPort;

	std::stringstream sstream;
	sstream << nPort;
	m_strPort = sstream.str();
}

//------------------------------------------------------------------------------------------------

size_t CAbkClient::CBaseAbstraction::WriteToSocket(tcp::socket &a_Socket, const std::string &a_Path, CAbkClient::CBaseAbstraction::eHttpRequestType a_Type)
{
	try
	{
		boost::asio::streambuf request;
		std::ostream request_stream(&request);

		assert(a_Type == E_HTTP_GET);

		request_stream << "GET ";
		request_stream << a_Path;
		request_stream << " HTTP/1.1\r\n";

		request_stream << "Host: " << m_strServerAddress << "\r\n";
		request_stream << "User-Agent: AbkClientBoost\r\n";
		request_stream << "Connection: keep-alive\r\n\r\n";

		return boost::asio::write(socket, request);
	}
	catch (std::exception &e)
	{
		boost::ignore_unused(e);
		m_bConnected = false;
		throw AbkNetworkException();
	}
}

size_t CAbkClient::CBaseAbstraction::WriteToSocket(tcp::socket &a_Socket, const std::string &a_Path, const std::string &a_Message, CAbkClient::CBaseAbstraction::eHttpRequestType a_Type)
{
	try
	{
		boost::asio::streambuf request;
		std::ostream request_stream(&request);

		switch (a_Type)
		{
		case E_HTTP_GET:
			request_stream << "GET ";
			// There's no reason why you would like to use GET with a message
			assert(false);
			break;
		case E_HTTP_POST:
			request_stream << "POST ";
			break;
		case E_HTTP_PUT:
			request_stream << "PUT ";
			break;
		case E_HTTP_DELETE:
			request_stream << "DELETE ";
			break;
		default:
			request_stream << "POST ";
			break;
		}

		request_stream << a_Path;
		request_stream << " HTTP/1.1\r\n";

		request_stream << "Host: " << m_strServerAddress << "\r\n";
		request_stream << "User-Agent: AbkClientBoost\r\n";
		request_stream << "Connection: keep-alive\r\n";

		request_stream << "Content-Length: " << a_Message.size() << "\r\n";
		request_stream << "Content-Type: application/json\r\n";
		request_stream << "\r\n";
		request_stream << a_Message;

		return boost::asio::write(socket, request);
	}
	catch (std::exception &e)
	{
		boost::ignore_unused(e);
		m_bConnected = false;
		throw AbkNetworkException();
	}
}

CAbkClient::CBaseAbstraction::eHttpHeaders CAbkClient::CBaseAbstraction::GetEnumFromString(const std::string &str) const
{
	eHttpHeaders returnValue = E_HEADER_INVALID;
	HashMap::const_iterator search = m_HashMap.find(str);
	if (search != m_HashMap.end())
	{
		returnValue = search->second;
	}
	return returnValue;
}

size_t CAbkClient::CBaseAbstraction::ReadFromSocket(tcp::socket &a_Socket, std::string &a_Message)
{
	std::stringstream sstream;
	size_t bytes = ReadFromSocket(a_Socket, sstream);
	a_Message = sstream.str();
	return bytes;
}

size_t CAbkClient::CBaseAbstraction::ReadFromSocket(tcp::socket &a_Socket, std::ostream &sstream)
{
	try
	{
		// Read the response status line. The response streambuf will automatically
		// grow to accommodate the entire line. The growth may be limited by passing
		// a maximum size to the streambuf constructor.
		boost::asio::streambuf response;
		boost::asio::read_until(socket, response, "\r\n");

		// Check that response is OK.
		std::istream response_stream(&response);
		std::string http_version;
		response_stream >> http_version;
		response_stream >> m_uStatus;

		std::string status_message;
		std::getline(response_stream, status_message);
		if (!response_stream || http_version.substr(0, 5) != "HTTP/")
		{
#ifdef LOG_BOOST_ABK
			Log() << "Invalid response\n";
#endif
			return -1;
		}
		// We still want to receive error messages
		if (!(m_uStatus == 200 || (m_uStatus >= 400 && m_uStatus <= 499)))
		{
#ifdef LOG_BOOST_ABK
			Log() << "Response returned with status code " << m_uStatus << "\n";
#endif
			return -1;
		}

		// Read the response headers, which are terminated by a blank line.
		boost::asio::read_until(socket, response, "\r\n\r\n");

		size_t responseBytesExpected;

		// Process the response headers.
		std::string header;
		while (std::getline(response_stream, header) && header != "\r")
		{
#ifdef LOG_BOOST_ABK
			Log() << header << "\n";
#endif

			std::vector<std::string> header_tokens;
			// TODO: if a value potentially has a space, this breaks
			// Consider transforming this to std::find
			boost::split(header_tokens, header, boost::is_any_of(": \r"), boost::token_compress_on);
			eHttpHeaders headerType = GetEnumFromString(header_tokens[0]);
			if (headerType == E_HEADER_CONTENT_LENGTH)
			{
				if (!boost::conversion::try_lexical_convert<size_t, std::string>(header_tokens[1], responseBytesExpected))
					responseBytesExpected = -1;
			}
		}

#ifdef LOG_BOOST_ABK
		Log() << "\n";
#endif

		// Inspect remaining size
		size_t responseBytesRead = response.size();
		if (responseBytesRead > 0)
			sstream << &response;

		// Not enough bytes available, read more lines
		size_t bytesLeftToRead = responseBytesExpected - responseBytesRead;

		while ((responseBytesRead < responseBytesExpected) && bytesLeftToRead)
		{ 
			responseBytesRead += boost::asio::read(socket, response,
				boost::asio::transfer_at_least(bytesLeftToRead));
			sstream << &response;
		}

	

		assert(responseBytesExpected == responseBytesRead);

		// eof means end of transmission and is being emitted when the connection is closed
		//if (error != boost::asio::error::eof)
		//	throw boost::system::system_error(error);

		return responseBytesExpected;
	}
	catch (std::exception &e)
	{
		boost::ignore_unused(e);
		//std::cerr << "Failed to read from socket: " << e.what() << std::endl;
		m_bConnected = false;
		throw AbkNetworkException();
	}
	return 0;
}

bool CAbkClient::CBaseAbstraction::IsSocketOpen() const
{
	return m_bConnected && socket.is_open();
}

bool CAbkClient::IsConnected() const
{
	CClientPtrRefConst a(m_pClientAux);
	CClientPtrRefConst e(m_pClientEvent);

	return ((m_nPort > 0) && a.IsValid() && e.IsValid() && e->IsSocketOpen() && a->IsSocketOpen());
}

void Abk::CAbkClient::CBaseAbstraction::Close()
{
// TODO: Consider shutdown
	socket.close();
}

bool CAbkClient::CBaseAbstraction::EnsureConnection()
{
	try
	{
		if (!IsSocketOpen())
		{
			boost::system::error_code ec;
			tcp::resolver::results_type endpoints = resolver.resolve(m_strServerAddress, m_strPort, ec);
			boost::asio::connect(socket, endpoints);
			if (!ec)
				m_bConnected = true;
			else
				m_bConnected = false;
		}

		return IsSocketOpen();
	}
	catch (boost::exception &e)
	{
		boost::ignore_unused(e);
#ifdef LOG_BOOST_ABK
		LogErr() << "Failed to connect due to exception" << std::endl;
#endif
		return false;
	}
}

std::string CAbkClient::CBaseAbstraction::NavigatePost(LPCTSTR pszPath, int nSessionId, CJsonFormatter *pPostData)
{
	assert(pPostData);
	assert(pszPath);
	assert(!m_strServerAddress.empty());
	//assert(pPostData->GetStream());
	//assert(pPostData->GetStream()->rdbuf()->in_avail > 0);
	std::string strPostData = pPostData->GetStream()->str();
	std::string response;

	if (!EnsureConnection())
		return response;

	try
	{
		WriteToSocket(socket, std::string(CT2A(pszPath)), strPostData, E_HTTP_POST);
		ReadFromSocket(socket, response);

#ifdef LOG_BOOST_ABK
		LogErr() << "Response was: " << response << std::endl;
#endif
	}
	catch (AbkNetworkException &e)
	{
		boost::ignore_unused(e);
#ifdef LOG_BOOST_ABK
		LogErr() << "Failed to navigate POST due to Network exception" << std::endl;
#endif
		response.clear();
	}
	return response;
}

bool CAbkClient::CBaseAbstraction::NavigateDelete(LPCTSTR pszPath, int nSessionId, CJsonFormatter * pPostData)
{
	assert(pPostData);
	assert(pszPath);
	assert(!m_strServerAddress.empty());
	std::string strPostData = pPostData->GetStream()->str();
	std::string response;

	if (!EnsureConnection())
		return false;

	try
	{
		WriteToSocket(socket, std::string(CT2A(pszPath)), strPostData, E_HTTP_DELETE);
		ReadFromSocket(socket, response);

#ifdef LOG_BOOST_ABK
		LogErr() << "Response was: " << response << std::endl;
#endif
		return true;
	}
	catch (AbkNetworkException &e)
	{
		boost::ignore_unused(e);
#ifdef LOG_BOOST_ABK
		LogErr() << "Failed to navigate DELETE due to Network exception" << std::endl;
#endif
		response.clear();
	}
	return false;
}

bool CAbkClient::CBaseAbstraction::DeleteSession(int nSessionId)
{
	CJsonFormatter jfSend;
	return NavigateDelete(_T(ABK_REQUESTURL_SESSIONID), nSessionId, &jfSend);
}

std::string CAbkClient::CBaseAbstraction::NavigatePut(LPCTSTR pszPath, int nSessionId, const std::string &strData)
{
	assert(pszPath);
	assert(!m_strServerAddress.empty());
	//assert(pPostData->GetStream());
	//assert(pPostData->GetStream()->rdbuf()->in_avail > 0);
	std::string response;

	if (!EnsureConnection())
		return false;

	try
	{
		WriteToSocket(socket, std::string(CT2A(pszPath)), strData, E_HTTP_PUT);
		ReadFromSocket(socket, response);

#ifdef LOG_BOOST_ABK
		LogErr() << "Response was: " << response << std::endl;
#endif
	}
	catch (AbkNetworkException &e)
	{
		boost::ignore_unused(e);
#ifdef LOG_BOOST_ABK
		LogErr() << "Failed to navigate POST due to Network exception" << std::endl;
#endif
		response.clear();
	}
	return response;
}

bool CAbkClient::CBaseAbstraction::NavigatePut(LPCTSTR pszPath, int nSessionId, CJsonFormatter *pPutData)
{
	assert(pPutData);
	assert(pszPath);
	assert(!m_strServerAddress.empty());
	//assert(pPostData->GetStream());
	//assert(pPostData->GetStream()->rdbuf()->in_avail > 0);
	std::string strPutData = pPutData->GetStream()->str();
	std::string response;

	if (!EnsureConnection())
		return false;

	try
	{
		if (nSessionId >= 0)
		{
			CString strPathAndQuery;
			strPathAndQuery.Format(_T("%s?") _T(ABK_QRY_SESSIONID) _T("=%d"), pszPath, nSessionId);
			WriteToSocket(socket, std::string(CT2A(strPathAndQuery)), strPutData, E_HTTP_PUT);
		}
		else
		{
			WriteToSocket(socket, std::string(CT2A(pszPath)), strPutData, E_HTTP_PUT);
		}
		ReadFromSocket(socket, response);

#ifdef LOG_BOOST_ABK
		LogErr() << "Response was: " << response << std::endl;
#endif
	}
	catch (AbkNetworkException &e)
	{
		boost::ignore_unused(e);
#ifdef LOG_BOOST_ABK
		LogErr() << "Failed to navigate POST due to Network exception" << std::endl;
#endif
		response.clear();
	}
	return (m_uStatus == 200);
}

std::string CAbkClient::CBaseAbstraction::NavigateGet(LPCTSTR pszPath, int nSessionId)
{
	assert(pszPath);
	std::string response;

	if (!EnsureConnection())
		return response;

	try
	{
		if (nSessionId >= 0)
		{
			CString strPathAndQuery;
			strPathAndQuery.Format(_T("%s?") _T(ABK_QRY_SESSIONID) _T("=%d"), pszPath, nSessionId);
			WriteToSocket(socket, std::string(CT2A(strPathAndQuery)), E_HTTP_GET);
		}
		else
		{
			WriteToSocket(socket, std::string(CT2A(pszPath)), E_HTTP_GET);
		}
		ReadFromSocket(socket, response);
	}
	catch (AbkNetworkException &e)
	{
		boost::ignore_unused(e);
#ifdef LOG_BOOST_ABK
		LogErr() << "Failed to navigate GET due to Network exception" << std::endl;
#endif
		response.clear();
	}

	return response;
}

bool CAbkClient::CBaseAbstraction::NavigateGet(LPCTSTR pszPath, int nSessionId, std::ostream &out)
{
	assert(pszPath);

	if (!EnsureConnection())
		return false;

	try
	{
		if (nSessionId >= 0)
		{
			CString strPathAndQuery;
			strPathAndQuery.Format(_T("%s?") _T(ABK_QRY_SESSIONID) _T("=%d"), pszPath, nSessionId);
			WriteToSocket(socket, std::string(CT2A(strPathAndQuery)), E_HTTP_GET);
		}
		else
		{
			WriteToSocket(socket, std::string(CT2A(pszPath)), E_HTTP_GET);
		}
		ReadFromSocket(socket, out);
		return true;
	}
	catch (AbkNetworkException &e)
	{
		boost::ignore_unused(e);
#ifdef LOG_BOOST_ABK
		LogErr() << "Failed to navigate GET due to Network exception" << std::endl;
#endif
	}

	return false;
}

void CAbkClient::AddLog(CAbkClient::LOGSEVERITY nSeverity, LPCTSTR pszMessage, ...)
{
	va_list args;
	va_start(args, pszMessage);
	//AddLogV(nSeverity,pszMessage,args);
	vprintf(CT2A(pszMessage), args);
	va_end(args);
}

int CAbkClient::CBaseAbstraction::ObtainSessionId(LPCTSTR pszClientClass, LPCTSTR pszClientType, LPCTSTR pszClientSerial)
{
	int nSessionId = -1; // result
	assert(pszClientClass);
	assert(pszClientType);
	CJsonFormatter jfPost;
	jfPost.WriteValue(ABK_REQ_SESSIONID_CLASS, CT2A(pszClientClass));
	jfPost.WriteValue(ABK_REQ_SESSIONID_TYPE, CT2A(pszClientType));
	if (pszClientSerial)
		jfPost.WriteValue(ABK_REQ_SESSIONID_SERIAL, CT2A(pszClientSerial));

	std::string pReturn = NavigatePost(_T(ABK_REQUESTURL_SESSIONID), -1, &jfPost);
	if (pReturn.empty())
		return -1;

	// extract the session id
	CJsonParser parsResponse(pReturn.c_str());
	for (; !parsResponse.IsDone(); ++parsResponse)
		parsResponse.ExtractValue(ABK_RSP_SESSIONID_ID, &nSessionId);
	if (nSessionId < 0)
	{
		m_pOwner->AddLog(LOGSEVERITY_ERROR, _T("Got no session id from server."));
		return -1;
	}
	return nSessionId;
}

CAbkClient::CAbkClient()
{
	m_bTerminateLongPoll = false;
	m_pNextEventData = NULL;
}


/*virtual*/ CAbkClient::~CAbkClient()
{
	TidyUp(false);
}

bool CAbkClient::Create(LPCTSTR pszServerAddress, int nPort, CAbkServerEvent *pEventRxBuffer, LPCTSTR pszClientClass, LPCTSTR pszClientType, LPCTSTR pszClientSerial)
{
	bool bSuccess = false;
	std::stringstream sstream;

	m_strClientClass = CT2A(pszClientClass);
	m_strClientType = CT2A(pszClientType); // regular client type, except when querying firmware info
	if (pszClientSerial)
		m_strClientSerial = CT2A(pszClientSerial);

	sstream << nPort;
	m_strPort = std::string(sstream.str());


	m_nPort = nPort;
	m_strServerAddress = std::string(CT2A(pszServerAddress));
	m_pNextEventData = pEventRxBuffer;

#ifdef LOG_BOOST_ABK
	Log() << "Printing port: " << m_strPort << std::endl;
#endif

	if (nPort > 0)
	{
		// create clients
		m_pClientAux = new CBaseAbstraction(this);
		m_pClientEvent = new CBaseAbstraction(this);
		CClientPtrRef pClientAux(m_pClientAux);
		CClientPtrRef pClientEvent(m_pClientEvent);
		pClientAux->SetServerAddr(m_strServerAddress, nPort);
		pClientEvent->SetServerAddr(m_strServerAddress, nPort);

		pClientAux->EnsureConnection();
		pClientEvent->EnsureConnection();

		// Connect and obtain session id
		m_nSessionId = pClientAux->ObtainSessionId(pszClientClass, pszClientType, pszClientSerial);
		if (m_nSessionId >= 0)
		{
			// Start long-polling thread
			m_longPollThread = boost::thread(LongPollThreadS, this);

			bSuccess = true;
		}
	}

	return true;
}

std::string CAbkClient::GetServerInfo()
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid()); // no connection with the aux http client established
	if (!pClientAux.IsValid())
		return NULL;
	if (!IsConnected())
		return NULL;
	return pClientAux->NavigateGet(_T(ABK_REQUESTURL_SERVERINFO), -1);
}

bool CAbkClient::GetServerInfo(CString & strProtocolVersion, CString & strInterfaceVersion, CString & strFwVersion, CString & strHwVersion, CString & strServerName, CString & strServerType, CString & strDescUrl)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid()); // no connection with the aux http client established
	if (!pClientAux.IsValid())
		return NULL;
	if (!IsConnected())
		return NULL;
	return !(pClientAux->NavigateGet(_T(ABK_REQUESTURL_SERVERINFO), -1)).empty();
}

std::string CAbkClient::GetInterfaceStatistics(void)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid()); // no connection with the aux http client established
	if (!pClientAux.IsValid())
		return NULL;
	if (!IsConnected())
		return NULL;
	return pClientAux->NavigateGet(_T(ABK_REQUESTURL_INTERFACESTATS), -1);
}

//--------------------------------------------------------------------------
// SendEvent()             sends a client event to the server
// -----------
// Input: strEventType = envent type string
//        strStringParam = string parameter
//        jfString = formatted object to be sent as string parameter
//        dParam1 = numeric parameter 1
//        dParam2 = numeric parameter 2
//        bPrivate: if true, client intends to process the reflected event
//                  by itself and the server shall not reflect the event
//                  to other clients
// Return: true on success, false on error

bool CAbkClient::SendEvent(const char *pszEventType, LPCTSTR pszStringParam, double dParam1, double dParam2, bool bPrivate)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	CJsonFormatter jfEvent; // whole event formatted in json
	jfEvent.WriteValue(ABK_RSP_CLIENTEVENT_SENDER, m_nSessionId);
#ifdef WINCE
	time_t tmNow = time(NULL); // get actual time
#else
	time_t tmNow;
	time(&tmNow); // get actual time
#endif
	jfEvent.WriteValue(ABK_RSP_CLIENTEVENT_TIME, tmNow);
	jfEvent.WriteValue(ABK_RSP_CLIENTEVENT_TYPE, pszEventType);
	jfEvent.WriteValue(ABK_RSP_SERVEREVENT_STRPARAM, CT2A(pszStringParam, CP_UTF8));
	jfEvent.WriteValue(ABK_RSP_SERVEREVENT_PARAM1, dParam1);
	jfEvent.WriteValue(ABK_RSP_SERVEREVENT_PARAM2, dParam2);
	jfEvent.WriteValue(ABK_RSP_CLIENTEVENT_PRIVATE, bPrivate);
	jfEvent.Close();
	CStopwatch watch;
	watch.Start();
	bool bSuccess = NULL != pClientAux->NavigatePut(_T(ABK_REQUESTURL_CLIENTEVENT), -1, &jfEvent);
	watch.Stop();
	watch.OutputDebugTimeMs(_T("SendEvent"));
	return bSuccess;
}

bool CAbkClient::SendEvent(const char *pszEventType, CJsonFormatter &jfString, double dParam1, double dParam2, bool bPrivate)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	CJsonFormatter jfEvent; // whole event formatted in json
	jfEvent.WriteValue(ABK_RSP_CLIENTEVENT_SENDER, m_nSessionId);
#ifdef WINCE
	time_t tmNow = time(NULL); // get actual time
#else
	time_t tmNow;
	time(&tmNow); // get actual time
#endif
	jfEvent.WriteValue(ABK_RSP_CLIENTEVENT_TIME, tmNow);
	jfEvent.WriteValue(ABK_RSP_CLIENTEVENT_TYPE, pszEventType);
	jfEvent.WriteValue(ABK_RSP_SERVEREVENT_STRPARAM, jfString.GetStream()->str().c_str());
	jfEvent.WriteValue(ABK_RSP_SERVEREVENT_PARAM1, dParam1);
	jfEvent.WriteValue(ABK_RSP_SERVEREVENT_PARAM2, dParam2);
	jfEvent.WriteValue(ABK_RSP_CLIENTEVENT_PRIVATE, bPrivate);
	jfEvent.Close();
	return NULL != pClientAux->NavigatePut(_T(ABK_REQUESTURL_CLIENTEVENT), -1, &jfEvent);
}

bool CAbkClient::SendAlertConfirmEvent(LPCTSTR pszAlertClassName, int nSeverity, int nMerged, bool bPermanent, bool bSuppressed, bool bTimeout)
{
	assert(this);
	assert((!bSuppressed) || (bSuppressed && !bTimeout)); // if suppressed, timeout must not be set! Please check how you call the function
	CJsonFormatter jfSend;
	std::string strClassA = CT2A(pszAlertClassName, CP_UTF8);
	jfSend.WriteValue(ABK_ALERTCONFIRM_CLASS, strClassA.c_str()); // "Class": "KickDown"
	jfSend.WriteValue(ABK_ALERTCONFIRM_SEVERITY, nSeverity); // "Severity": 3
	jfSend.WriteValue(ABK_ALERTCONFIRM_COUNT, nMerged); // "Merged": 5
	jfSend.WriteValue(ABK_ALERTCONFIRM_SUPPRESSED, bSuppressed); // "Suppressed": false
	jfSend.WriteValue(ABK_ALERTCONFIRM_TIMEOUT, bTimeout); // "Timeout": false
	jfSend.WriteValue(ABK_ALERTCONFIRM_PERMASUPPRBYUSER, bPermanent); // "PermanentSuppressedByUser": false
	jfSend.Close();
	// TODO:
	return SendEvent(ABK_CLIENTEVENT_ALERT_CONFIRM, jfSend, 0, 0, false);
	//return true;
}

CAbkServerEvent *CAbkClient::OnServerEvent(CAbkServerEvent *pEventData)
{
	// default implementation: use the same event object for the next event
	return pEventData;
}

bool CAbkClient::DownloadFile(LPCTSTR pszUrl, std::ostream &out, PFNSTATUSCALLBACK pfnReadCallback, DWORD_PTR dwCookie)
{
	CClientPtrRef pClientAux(m_pClientAux);
	if (!pClientAux.IsValid())
		return false;
	bool bSuccess = false;
	bSuccess = pClientAux->NavigateGet(pszUrl, -1, out);
	return bSuccess;
}

bool CAbkClient::DownloadFile(LPCTSTR pszUrl, LPCTSTR pszStorePath, PFNSTATUSCALLBACK pfnReadCallback, DWORD_PTR dwCookie)
{
	std::ofstream file(CT2A(pszStorePath), std::ofstream::out);
	return DownloadFile(pszUrl, file);
}

bool CAbkClient::ReceiveEvent()
{
	CClientPtrRef pClientEvent(m_pClientEvent);
	assert(pClientEvent.IsValid());
	if (!pClientEvent.IsValid())
		return false;
	std::string strResponse = pClientEvent->NavigateGet(_T(ABK_REQUESTURL_SERVEREVENT), m_nSessionId); // request event and wait for answer (blocks here)
	return !strResponse.empty();
}

//--------------------------------------------------------------------------
// SetVarValue()           sets a variable value
// -------------
// Input: pszVarName = name of variable to be set
//        pSet = pointer to new value
// Return: 

bool CAbkClient::SetVarValue(LPCTSTR pszVarName, const CString &strSet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	std::string strValue = CT2A(strSet);
	return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_VARVALUE), CT2A(pszVarName, CP_UTF8), &strValue);
}

bool CAbkClient::SetVarValue(LPCTSTR pszVarName, const std::string &strSet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_VARVALUE), CT2A(pszVarName, CP_UTF8), &strSet);
}

bool CAbkClient::SetVarValue(LPCTSTR pszVarName, const double dSet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_VARVALUE), CT2A(pszVarName, CP_UTF8), &dSet);
}

bool CAbkClient::SetVarValue(LPCTSTR pszVarName, const int nSet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_VARVALUE), CT2A(pszVarName, CP_UTF8), &nSet);
}

bool CAbkClient::SetVarValue(LPCTSTR pszVarName, const bool bSet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_VARVALUE), CT2A(pszVarName, CP_UTF8), &bSet);
}

/** Sets a mailbox value */
bool CAbkClient::SetMailboxValue(LPCTSTR pszMailboxName, const CString &strSet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	std::string strValue = CT2A(strSet);
	return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_MAILBOXVALUE), CT2A(pszMailboxName, CP_UTF8), &strValue);
}

bool CAbkClient::SetMailboxValue(LPCTSTR pszMailboxName, const std::string &strSet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_MAILBOXVALUE), CT2A(pszMailboxName, CP_UTF8), &strSet);
}

bool CAbkClient::SetMailboxValue(LPCTSTR pszMailboxName, const double dSet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_MAILBOXVALUE), CT2A(pszMailboxName, CP_UTF8), &dSet);
}

bool CAbkClient::SetMailboxValue(LPCTSTR pszMailboxName, const int nSet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_MAILBOXVALUE), CT2A(pszMailboxName, CP_UTF8), &nSet);
}

bool CAbkClient::SetMailboxValue(LPCTSTR pszMailboxName, const bool bSet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_MAILBOXVALUE), CT2A(pszMailboxName, CP_UTF8), &bSet);
}

/** Retrieves the current time of the server as local time
	@param pGet
		Pointer to return the server time, returned in client local time
	@return
		true on success, false on error
*/
bool CAbkClient::GetCurrentServerTime(time_t *pGet)
{
	bool bSuccess = FALSE;
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (pClientAux.IsValid())
	{
		std::string strReturn = pClientAux->NavigateGet(_T(ABK_REQUESTURL_CURRENTTIME), -1);
		if (!strReturn.empty())
		{
			CJsonParser parsResponse(strReturn.c_str());
			for (; !parsResponse.IsDone(); ++parsResponse)
			{
				bSuccess = parsResponse.ExtractValue(ABK_RSP_CURRENTTIME_TIME, pGet);
				if (bSuccess)
					break;
			}
			if (!bSuccess)
			{
				AddLog(LOGSEVERITY_ERROR, _T("Error: No date included in the answer of %s"), _T(ABK_REQUESTURL_CURRENTTIME));
			}
		}
	}
	return bSuccess;
}

bool CAbkClient::GetMailboxList(std::list<CString> *pGet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	return pClientAux->GetVarOrMailboxList(_T(ABK_REQUESTURL_MAILBOXLIST), pGet);
}

bool CAbkClient::GetMailboxMeta(const std::list<LPCTSTR>& lstMailboxNames, std::list<CAbkClientMeta>* pGet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	return pClientAux->GetVarOrMailboxMeta(lstMailboxNames, pGet, true);
}

bool CAbkClient::GetMailboxMeta(LPCTSTR pszMailboxName, CAbkClientMeta *pGet)
{
	std::list<LPCTSTR> lstVarNames;
	std::list<CAbkClientMeta> lstMeta;
	lstVarNames.push_back(pszMailboxName); // compose a list with one entity
	bool bSuccess = GetMailboxMeta(lstVarNames, &lstMeta); // request the meta data
	assert(lstMeta.size() == 1);
	if (bSuccess)
		*pGet = *lstMeta.begin();
	return bSuccess;
}

//--------------------------------------------------------------------------
// GetVarMeta()            requests meta data of a variable
// ------------
// Input: lstVarNames = list with variable names
//        pGet = pointer to return the meta data
// Return: true on success, false on error

bool CAbkClient::GetVarMeta(const std::list<LPCTSTR> &lstVarNames, std::list<CAbkClientMeta> *pGet)
{
	bool bSuccess = false;
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (pClientAux.IsValid())
		bSuccess = pClientAux->GetVarOrMailboxMeta(lstVarNames, pGet, false);
	return bSuccess;
}


//--------------------------------------------------------------------------
// GetVarMeta()            requests meta data of a variable
// ------------
// Input: pszVarName = name of variable to be queried
//        pGet = pointer to return the meta data
// Return: true on success, false on error

bool CAbkClient::GetVarMeta(LPCTSTR pszVarName, CAbkClientMeta *pGet)
{
	std::list<LPCTSTR> lstVarNames;
	std::list<CAbkClientMeta> lstMeta;
	lstVarNames.push_back(pszVarName); // compose a list with one entity
	bool bSuccess = GetVarMeta(lstVarNames, &lstMeta); // request the meta data
	assert(lstMeta.size() == 1);
	if (bSuccess)
		*pGet = *lstMeta.begin();
	return bSuccess;
}

bool CAbkClient::SendForm(LPCTSTR pszFormName, const std::list<CFormElement>& lstSend)
{
	ASSERT(pszFormName);
	// assert(m_pClientAux);

	CClientPtrRef pClientAux(m_pClientAux);
	if (!pClientAux.IsValid())
	{
		AddLog(LOGSEVERITY_ERROR, _T("Tried to send the filled form \"%s\" but connection to server was lost in the meanwhile."), pszFormName);
		return false;
	}
	bool bSuccess = true;
	CJsonFormatter jfForm;  // {
	std::list<CFormElement>::const_iterator iterElement;
	for (iterElement = lstSend.begin(); iterElement != lstSend.end(); ++iterElement)
	{
		const CFormElement *pElement = &*iterElement;
		switch (pElement->m_varValue.vt)
		{
		case VT_I2:
			jfForm.WriteValue(CT2A(pElement->m_strName, CP_UTF8), (int)pElement->m_varValue.iVal); // "Elementname":123
			break;
		case VT_I4:
			jfForm.WriteValue(CT2A(pElement->m_strName, CP_UTF8), (int)pElement->m_varValue.lVal); // "Elementname":123
			break;
		case VT_INT:
			jfForm.WriteValue(CT2A(pElement->m_strName, CP_UTF8), (int)pElement->m_varValue.intVal); // "Elementname":123
			break;
		case VT_R8:
			jfForm.WriteValue(CT2A(pElement->m_strName, CP_UTF8), pElement->m_varValue.dblVal); // "Elementname":1.23
			break;
		case VT_BSTR:
			jfForm.WriteValue(CT2A(pElement->m_strName, CP_UTF8), (char *)(CW2A(pElement->m_varValue.bstrVal, CP_UTF8))); // "Elementname":"string"
			break;
		case VT_BOOL:
			jfForm.WriteValue(CT2A(pElement->m_strName, CP_UTF8), (bool)(pElement->m_varValue.boolVal != 0)); // "Elementname":true
			break;
		case VT_EMPTY:
			jfForm.WriteValue(CT2A(pElement->m_strName, CP_UTF8), std::numeric_limits<double>::quiet_NaN()); // null
			break;
		default:
			assert(false); // encountered an unimplemented variant type
			bSuccess = false;
		}
	}
	jfForm.Close(); // }
	if (!bSuccess)
		return false;

	// send form
	CString strUrl;
	strUrl.Format(_T("%s/%s"), _T(ABK_SERVICE_FORMS), pszFormName);
	if (!pClientAux->NavigatePut(strUrl, -1, &jfForm))
		return false;
	return true;
}

//--------------------------------------------------------------------------
// SendAudioRecHeader()    sends an audio header
// --------------------
// Input: nId = general purpose id the server wants to be reflected when the server initiated the recording operation
//        nSampleRateHz = sample rate in Hz
//        nBitsPerSample = bits per sample, 8 and 16 allowed
//        nChannels = number of channels. Allowed is 1 (mono) and 2 (stereo)
// Return: true on success, false on error

bool CAbkClient::SendAudioRecHeader(int nId, int nSampleRateHz, int nBitsPerSample, int nChannels)
{
	bool bSuccess = false;
	assert(nBitsPerSample == 8 || nBitsPerSample == 16); // invalid bits per sample??
	assert(nChannels == 1 || nChannels == 2); // invalid number of channels??
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (pClientAux.IsValid() && IsConnected())
	{
		CJsonFormatter jfHeader; // header data formatted in json
		jfHeader.WriteValue(ABK_AUDIOREC_ID, nId);
		jfHeader.WriteValue(ABK_AUDIOREC_SAMPLERATE_HZ, nSampleRateHz);
		jfHeader.WriteValue(ABK_AUDIOREC_CHANNELS, nChannels);
		jfHeader.WriteValue(ABK_AUDIOREC_BITSPERSAMPLE, nBitsPerSample);
		jfHeader.Close();
		bSuccess = NULL != pClientAux->NavigatePut(_T(ABK_REQUESTURL_AUDIOREC_HEADER), -1, &jfHeader);
	}
	return bSuccess;
}


//--------------------------------------------------------------------------
// SendAudioRecData()      sends audio data
// ------------------
// Input: nId = general purpose id the server wants to be reflected when the server initiated the recording operation
//        pData = data.
//                if bits per sample == 8: BYTES
//                if bits per sample == 16: WORDs in  little endian format.
//                value seauence for stereo: left, right, left, right ...
//        nBitsPerSample = bits per sample, 8 and 16 allowed
//        nChannels = number of channels. Allowed is 1 (mono) and 2 (stereo)
//        nSamplesPerChannel = number of samples of each channel in pData
// Return: true on success, false on error

bool CAbkClient::SendAudioRecData(int nId, const void *pData, int nBitsPerSample, int nChannels, int nSamplesPerChannel)
{
	bool bSuccess = false;
	assert(nBitsPerSample == 8 || nBitsPerSample == 16); // invalid bits per sample??
	assert(nChannels == 1 || nChannels == 2); // invalid number of channels??
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (pClientAux.IsValid() && IsConnected())
	{
		CJsonFormatter jfData; // header data formatted in json
		jfData.WriteValue(ABK_AUDIOREC_ID, nId);

		CJsonStreamArray jaData(&jfData, ABK_AUDIOREC_DATA);
		if (nBitsPerSample == 8)
		{
			const BYTE *pData8 = (const BYTE *)pData;
			for (int nSample = 0; nSample < nSamplesPerChannel; ++nSample)
			{
				for (int nChannel = 0; nChannel < nChannels; ++nChannel)
				{
					jaData.WriteValue((int)(*pData8));
					++pData8;
				}
			}
		}
		else if (nBitsPerSample == 16)
		{
			const WORD *pData16 = (const WORD *)pData;
			for (int nSample = 0; nSample < nSamplesPerChannel; ++nSample)
			{
				for (int nChannel = 0; nChannel < nChannels; ++nChannel)
				{
					jaData.WriteValue((int)(*pData16));
					++pData16;
				}
			}
		}
		else
		{
			assert(false);
		}
		jaData.Close();

		jfData.Close();
		bSuccess = NULL != pClientAux->NavigatePut(_T(ABK_REQUESTURL_AUDIOREC_DATA), -1, &jfData);
	}
	return bSuccess;
}


//--------------------------------------------------------------------------
// SendAudioRecFooter()    sends audio footer
// --------------------
// Input: nId = general purpose id the server wants to be reflected when the server initiated the recording operation
// Return: true on success, false on error

bool CAbkClient::SendAudioRecFooter(int nId)
{
	bool bSuccess = false;
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (pClientAux.IsValid() && IsConnected())
	{
		CJsonFormatter jfFooter; // header data formatted in json
		jfFooter.WriteValue(ABK_AUDIOREC_ID, nId);
		jfFooter.Close();
		bSuccess = NULL != pClientAux->NavigatePut(_T(ABK_REQUESTURL_AUDIOREC_FOOTER), -1, &jfFooter);
	}
	return bSuccess;
}



//--------------------------------------------------------------------------
// SendAudioRecRejectEvent() sends event that user rejected audio recording
// -------------------------
// Input: nId = general purpose ID the sender wants to be reflected
// Return: 

bool CAbkClient::SendAudioRecRejectEvent(int nId)
{
	return SendEvent(ABK_CLIENTEVENT_AUDIOREC_REJECT, _T(""), (double)nId, 0, false);
}

//--------------------------------------------------------------------------
// SuspendLongPolling()      pauses the long-poll thread
// ------------------
// Input: -
// Return: 

bool CAbkClient::SuspendLongPolling(void)
{
	//if (!m_hLongPollThread)
	//	return false;
	m_evLongPollEnable.Reset(); // stall the long polling thread
	return true;
}

//--------------------------------------------------------------------------
// ResumeLongPolling()     resumes long poll thread
// -------------------
// Input: -
// Return: 

bool CAbkClient::ResumeLongPolling(void)
{
	//if (!m_hLongPollThread)
	//	return false;
	m_evLongPollEnable.Set(); // no longer stall the long polling thread
	return true;
}

//--------------------------------------------------------------------------
// LongPollThreadS()        long polling thread
// ----------------
// Input: vpThis = pointer to CAbkClient
// Return: -

/*static*/ DWORD WINAPI CAbkClient::LongPollThreadS(void *vpThis)
{
	assert(vpThis);
	return (reinterpret_cast<CAbkClient *>(vpThis))->LongPollThread();
}

//--------------------------------------------------------------------------
// LongPollThread()        long polling thread
// ----------------
// Input: -
// Return: -

int CAbkClient::LongPollThread(void)
{
	AddLog(LOGSEVERITY_TRACE, _T("LongPollThread() started"));
	// int nErrorCount=0; // incrementing on errors, decrementing on http success
	DWORD dwTickLastSuccessfulResponse = 0; // ticks when the last successfull response was received
	while (!m_bTerminateLongPoll)
	{
		const char *pszResponse = NULL; // answer from server with events and data

		//WaitForSingleObject(m_evLongPollEnable.m_hObject, INFINITE); // if stalled, block here until the event gets set
		m_evLongPollEnable.Wait(INFINITE);

		CClientPtrRef pClientEvent(m_pClientEvent);
		assert(pClientEvent.IsValid()); // no connection with the aux http client established
		if (!pClientEvent.IsValid())
			break;
		//    DWORD dwTickBefore=GetTickCount(); // tick count before the request
		std::string strResponse = pClientEvent->NavigateGet(_T(ABK_REQUESTURL_SERVEREVENT), m_nSessionId); // request event and wait for answer (blocks here);
		
		int nHttpStatus = pClientEvent->GetStatus();

		if (nHttpStatus == 200)
			pszResponse = strResponse.c_str();

		DWORD dwTickAfter = GetTickCount();

		//char buf[1024];
		//pszResponse=buf;
		//memcpy(buf,"{\"DataLists\":{\"DaqListVar\":[74,72174,72174,72174,72174,72174,72174,72174,72174,72174]}}",1024);
		//Sleep(50);

		if (nHttpStatus == 200 && pszResponse)
		{
			dwTickLastSuccessfulResponse = dwTickAfter;
			CJsonParserAtl jpEvent(pszResponse);
			for (; !jpEvent.IsDone(); ++jpEvent)  // each event
			{
				if (jpEvent.TestObject(ABK_RSP_SERVEREVENT_DATALISTS)) // is there DataLists:{
				{
					//OnServerDataBegin();
					for (++jpEvent; !jpEvent.IsDone(); ++jpEvent)  // each data list
					{
						std::string strDaqName;
						if (jpEvent.TestArray(&strDaqName)) // if array
						{
							CAbkSingleLock lockDaq(&m_mutexDaq, true, DAQ_TIMEOUT); // lock the daq map
							assert(m_mutexDaq.IsLocked());
							CAbkClientDaq *pDaq = FindDaq(strDaqName); // get the client-side daq list
							if (pDaq)
							{
								bool bNotifyVars; // false if callback wishes the variables not to be updated
								bNotifyVars = pDaq->OnBeginDataFromServer();
								if (bNotifyVars)
								{
									if (const size_t nVarCount = pDaq->m_vectVars.size())
									{
										CAbkClientVar **ppVars = &pDaq->m_vectVars[0];
										size_t nVar = 0;
										for (++jpEvent; !jpEvent.IsDone(); ++jpEvent) // each value
										{
											if (nVar >= nVarCount) // if the returned data from server contains more data than our daq list specifies..
											{ //.. it is an error
												AddLog(LOGSEVERITY_ERROR, _T("The returned data from server in daq \"%s\" contains more data than the daq list specifies"), (LPCTSTR)pDaq->m_strName);
												break;
											}
											CAbkClientVar *pVar = ppVars[nVar]; // this variable gets the new data
											assert(pVar);
											pDaq->OnValueFromServer(pVar, &jpEvent);
											++nVar;
										}
									}
								}
								else // no variable update
								{
									for (++jpEvent; !jpEvent.IsDone(); ++jpEvent); // skip each value
								}
								pDaq->OnEndDataFromServer(); // notify the derived class that variable updates are done
							}
						} // end of data array
						jpEvent.SkipItem(); // skip unexpected items
					} // for each data list

					//OnServerDataEnd();
				}
				else if (jpEvent.TestArray(ABK_RSP_SERVEREVENT_EVENTS)) // is there Events:[
				{
					assert(m_pNextEventData); // there must be a location to store the event params
					for (++jpEvent; !jpEvent.IsDone(); ++jpEvent)  // each event
					{
						BOOL bSuccessDecode = m_pNextEventData->SetEvent(jpEvent); // decode event into m_pNextEventData
						jpEvent.SkipItem(); // skip any unknown items
						if (bSuccessDecode)
						{
							m_pNextEventData = OnServerEvent(m_pNextEventData); // call the event handler and get the location for the next event
						}
						else // error in syntax or completelyness of the event data
						{
							AddLog(LOGSEVERITY_ERROR, _T("The event data was incomplete or had incorrect syntax")/*,m_pNextEventData->m_data.m_strType*/);
							break;
						}
						jpEvent.SkipItem(); // skip unexpected items            
					} // each event
				}
				jpEvent.SkipItem(); // skip unexpected items
			} // for each event
		}
		else if (pszResponse == NULL)
		{
			if (dwTickAfter >= dwTickLastSuccessfulResponse + LONGPOLL_FAILURE_TOLERANCE_MS) // tolerated error time span exceeded
			{
				DWORD dwErrorDuration = 0;
				if (dwTickLastSuccessfulResponse)
					dwErrorDuration = dwTickAfter - dwTickLastSuccessfulResponse;
				m_bTerminateLongPoll = true; // terminate and signal that thread terminated itself (an error occured)
				AddLog(LOGSEVERITY_ERROR, _T("Successive http errors for %d ms."), (int)dwErrorDuration/*LONGPOLL_FAILURE_TOLERANCE_MS*/);
			}
			else
			{
				Sleep(LONGPOLL_FAILURE_RECOVERY_MS);
			}
		}
		else // server responded but with error
		{
			DWORD dwWaitBeforeResume = OnLongPollErrorResponse(nHttpStatus, m_nSessionId); // call custom handler
			while (!m_bTerminateLongPoll && dwWaitBeforeResume != 0)
			{
				Sleep(10);
				if (dwWaitBeforeResume > 10)
					dwWaitBeforeResume -= 10;
				else
					dwWaitBeforeResume = 0;
			}
		}
	}
	AddLog(LOGSEVERITY_TRACE, _T("LongPollThread() terminating"));
	//m_longPollThread.detach();
	m_evLongPollDone.Set();
	return 0;
}


bool CAbkClient::AddDaq(CAbkClientDaq *pAdd)
{
	bool bSuccess = false;

	if (m_nSessionId >= 0)
	{
		CAbkSingleLock lockDaq(&m_mutexDaq, true, DAQ_TIMEOUT);
		assert(m_mutexDaq.IsLocked());
		std::string strDaqNameA = CT2A(pAdd->m_strName, CP_UTF8);
		if (!FindDaq(strDaqNameA))
		{
			std::pair<std::map<std::string, CAbkClientDaq *>::iterator, bool> iterInsert; // result of the insert operation
			iterInsert = m_mapDaq.insert(std::pair<std::string, CAbkClientDaq *>(strDaqNameA, pAdd));
			assert(iterInsert.second); // error inserting the daq?
			pAdd->m_pOwner = this;
			bSuccess = pAdd->Update(); // send it to server
		}
		else
			AddLog(LOGSEVERITY_ERROR, _T("Tried to add a DAQ while another DAQ with same name exists: \"%s\""), (LPCTSTR)pAdd->m_strName);
	}
	else
		AddLog(LOGSEVERITY_ERROR, _T("Tried to add a DAQ without having a valid session ID"));
	return bSuccess;
}

//--------------------------------------------------------------------------
// FindDaq()               searches for a DAQ
// ---------
// Input: strDaqName = name of daq to search for
// Return: pointer to DAQ list, NULL if not found

CAbkClientDaq *CAbkClient::FindDaq(const std::string &strDaqName)
{
	CAbkSingleLock lockDaq(&m_mutexDaq, true, DAQ_TIMEOUT);
	assert(m_mutexDaq.IsLocked());
	std::map<std::string, CAbkClientDaq *>::iterator iterDaq;
	iterDaq = m_mapDaq.find(strDaqName);
	if (iterDaq == m_mapDaq.end())
		return NULL; // not found
	return (*iterDaq).second;
}

BOOL CAbkClient::PopLog(LOGSEVERITY & nSeverityGet, CString & strMessageGet)
{
	// TODO:
	return TRUE;
}

//--------------------------------------------------------------------------
// SendButtonEvent()           sends a button press/release event to the server
// -----------------
// Input: strButtonName = name of the button
//        bPressedState = TRUE if button is pressed, FALSE if released
//        nTime = time value of key event.
//                positive values indicate the time since key was pressed in ms
//                negative values indicate the time since key was released in ms
//        bPrivate: if true, client intends to process the reflected event
//                  by itself and the server shall not reflect the event
//                  to other clients
// Return: true on success, false on error

bool CAbkClient::SendButtonEvent(LPCTSTR pszButtonName, bool bPressedState, int nTime, bool bPrivate)
{
	return SendEvent(ABK_CLIENTEVENT_BUTTON, pszButtonName, (double)(bPressedState != 0), (double)nTime, bPrivate);
}

std::string CAbkClient::GetClientState(LPCTSTR pszFileExtension)
{
	CClientPtrRef pClientAux(m_pClientAux);
	//assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return NULL;
	CString strUrl;
	assert(pszFileExtension);
	assert(pszFileExtension[0] != '\0'); // please no empty extension
	assert(pszFileExtension[0] == '.'); // extension must start with delimiter dot
	strUrl.Format(_T("%s/%s_%s_%s%s"), _T(ABK_SERVICE_CLIENTSTATES), CString(m_strClientClass.c_str()), CString(m_strClientType.c_str()), CString(m_strClientSerial.c_str()), pszFileExtension);
	std::string strResponse = pClientAux->NavigateGet(strUrl, -1); // read data
	int nStatus = pClientAux->GetStatus();
	if (nStatus != 200) // if not responded with OK (200)..
		strResponse.clear(); // .. devalidate the result
	return strResponse;
}

bool CAbkClient::SetClientState(const char * pConfigString, LPCTSTR pszFileExtension)
{
	bool bSuccess = false;
	CClientPtrRef pClientAux(m_pClientAux);
	if (pClientAux.IsValid() && IsConnected())
	{
		CString strUrl;
		assert(pszFileExtension);
		assert(pszFileExtension[0] != '\0'); // please no empty extension
		assert(pszFileExtension[0] == '.'); // extension must start with delimiter dot
		strUrl.Format(_T("%s/%s_%s_%s%s"), _T(ABK_SERVICE_CLIENTSTATES), CString(m_strClientClass.c_str()), CString(m_strClientType.c_str()), CString(m_strClientSerial.c_str()), pszFileExtension);
		std::string response = pClientAux->NavigatePut(strUrl, -1, std::string(pConfigString) /*, _T(MIME_TYPE_TEXT)*/);
		bool bSuccess = !response.empty();
		if (bSuccess)
		{
			int nStatus = pClientAux->GetStatus();
			if ((nStatus < 200) || (nStatus >= 300)) // if not responded with an OK-code (2xx)..
				bSuccess = false; // .. error in writing at the server
		}
	}
	return bSuccess;
}


bool CAbkClient::CBaseAbstraction::GetVarOrMailboxList(LPCTSTR pszPath, std::list<CString> *pGet)
{
	bool bSuccess = false;
	std::string response = NavigateGet(pszPath, -1);
	if (!response.empty())
	{
		CJsonParser jpVars(response.c_str());
		for (; !jpVars.IsDone(); ++jpVars)
		{
			if (jpVars.TestArray(ABK_RSP_VARLIST)) // is there "VarList": [
			{
				for (++jpVars; !jpVars.IsDone(); ++jpVars)
				{
					std::string strVarName;
					jpVars.ExtractValue(&strVarName);
					pGet->push_back(CString(CA2T(strVarName.c_str())));
				}
				bSuccess = true;
			}
			jpVars.SkipItem();
		}
	}
	return bSuccess;
}

bool CAbkClient::CBaseAbstraction::GetVarOrMailboxMeta(const std::list<LPCTSTR>& lstVarNames, std::list<CAbkClientMeta>* pGet, bool bMailboxFlag)
{
	bool bSuccess = false;
	LPCTSTR pszPath;
	if (bMailboxFlag)
		pszPath = _T(ABK_REQUESTURL_MAILBOXMETA);
	else
		pszPath = _T(ABK_REQUESTURL_VARMETA);
	CJsonFormatter jfRequest;
	{
		CJsonStreamArray jaVarList(&jfRequest, ABK_REQ_VARMETA_VARLIST); // "VarList": [
		std::list<LPCTSTR>::const_iterator iterVarNames;
		for (iterVarNames = lstVarNames.begin(); iterVarNames != lstVarNames.end(); ++iterVarNames)
		{
			LPCTSTR pszVarName = *iterVarNames;
			jaVarList.WriteValue(CT2A(pszVarName, CP_UTF8)); // "Var1",
		}
	} // jaVarList falls out of scope => "]"
	jfRequest.Close(); // "}"

	std::string strResponse = NavigatePost(pszPath, -1, &jfRequest);
	if (!strResponse.empty())
	{
		CJsonParser jpMeta(strResponse.c_str());
		for (; !jpMeta.IsDone(); ++jpMeta)
		{
			if (jpMeta.TestArray(ABK_RSP_VARMETA_METADATA)) // is there an array named "MetaData":
			{
				for (++jpMeta; !jpMeta.IsDone(); ++jpMeta)
				{
					CAbkClientMeta metaVar;
					bSuccess = metaVar.ExtractFromJson(jpMeta);
					metaVar.m_bIsMailbox = bMailboxFlag;
					pGet->push_back(metaVar); // append meta data to result list
				}
			}
			jpMeta.SkipItem(); // skip any other members
		}
		bSuccess = true;
	}
	else
	{
		m_pOwner->AddLogHttp(LOGSEVERITY_WARNING, GetStatus(), pszPath, _T("POST"), strResponse.c_str());
	}
	return bSuccess;
}

bool CAbkClient::GetVarList(std::list<CString> *pGet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	return pClientAux->GetVarOrMailboxList(_T(ABK_REQUESTURL_VARLIST), pGet);
}

bool CAbkClient::GetClientFirmwareInfo(std::list<CFirmwareInfo>& lstGet, LPCTSTR pszClientType)
{
	CClientPtrRef pClientAux(m_pClientAux);
	if (!pClientAux.IsValid())
		return false;
	bool bSuccess = true;
	lstGet.clear();

	// compose and send request
	if (!pszClientType) // if no client type name specified, use the stored one
		pszClientType = CA2T(m_strClientType.c_str());
	CJsonFormatter jfReq;
	jfReq.WriteValue(ABK_REQ_FIRMWARE_CLASS, m_strClientClass);
	jfReq.WriteValue(ABK_REQ_FIRMWARE_TYPE, CT2A(pszClientType, CP_UTF8));
	jfReq.WriteValue("Serial", m_strClientSerial); // send serial unsolicitedly
	std::string strResponse = pClientAux->NavigatePost(_T(ABK_REQUESTURL_FIRMWARE), -1, &jfReq); // send own info and get list of firmware files file info
	if (strResponse.empty())
		return false;

	// decode response
	CJsonParserAtl jpResp(strResponse.c_str());
	bool bAnyVersionOmitted = false; // if we found at least one entity without version info
	for (; !jpResp.IsDone(); ++jpResp)
	{
		if (jpResp.TestArray(ABK_RSP_FIRMWARE_IMAGELIST)) // is there "Images":[
		{
			CFirmwareInfo fwi;
			bool bUrlDecoded = false;
			bool bMd5Decoded = false;
			bool bVersionDecoded = false;
			for (; !jpResp.IsDone(); ++jpResp)
			{
				bUrlDecoded |= jpResp.ExtractValueAtl(ABK_RSP_FIRMWARE_URL, fwi.m_strUrl); // get URL
				bMd5Decoded |= jpResp.ExtractValueAtl(ABK_RSP_FIRMWARE_MD5, fwi.m_strMd5); // get MD5 hash
				bVersionDecoded |= jpResp.ExtractValueAtl(ABK_RSP_FIRMWARE_VERSION, fwi.m_strVersion); // get version info
			}
			if (!bVersionDecoded)
				bAnyVersionOmitted = true;
			if (bUrlDecoded && bMd5Decoded) // at least the server has to fill in these fields
			{
				lstGet.push_back(fwi);
			}
			else
			{
				if (!bUrlDecoded)
					AddLog(LOGSEVERITY_ERROR, _T("GetClientFwInfo: Firmware info response: URL field is missing"));
				if (!bMd5Decoded)
					AddLog(LOGSEVERITY_ERROR, _T("GetClientFwInfo: Firmware info response: MD5 field is missing"));
				bSuccess = FALSE;
			}
			jpResp.SkipItem();
		}
	}
	if (bAnyVersionOmitted && lstGet.size() > 1) // if more than one entity returned and a version field was omitted
	{
		AddLog(LOGSEVERITY_ERROR, _T("GetClientFwInfo: Firmware info response: Version field is missing while returning multiple entities"));
		bSuccess = FALSE;
	}
	if (!bSuccess)
		lstGet.clear(); // discard decoded content if an error occured
	return bSuccess;
}

std::size_t CAbkClient::CBaseAbstraction::insensitive_hash::operator()(const std::string &value) const
{
	std::string copy(value);
	boost::to_lower(copy);
	return boost::hash<std::string>()(copy);
}

void CAbkClient::AddLogHttp(LOGSEVERITY nSeverity, int nHttpStatusCode, LPCTSTR pszUrl, LPCTSTR pszMethod, const char *pcszResponse, const char *pcszoPostPutData/*=NULL*/)
{
	//ASSERT(nHttpStatusCode>=0);
	if (pcszoPostPutData)
		AddLog(LOGSEVERITY_ERROR, _T("HTTP status code %d. Url: \"%s\", Method: %s, Request: \"%s\", Response: \"%s\""), nHttpStatusCode, pszUrl, pszMethod, (LPCTSTR)CA2T(pcszoPostPutData, CP_UTF8), (LPCTSTR)CA2T(pcszResponse, CP_UTF8));
	else
		AddLog(LOGSEVERITY_ERROR, _T("HTTP status code %d. Url: \"%s\", Method: %s, Response: \"%s\""), nHttpStatusCode, pszUrl, pszMethod, (LPCTSTR)CA2T(pcszResponse, CP_UTF8));
	if (nHttpStatusCode < 0)
	{
		// 23.10.18: Desktop-PC, Fehler in c:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\atlmfc\include\atlspriv.inl Zeile 218, inline bool ZEvtSyncSocket::Read()=> WSARecv()-Fehler. WSAGetLastError(): 10053
		DWORD dwError = GetLastError();
		AddLog(LOGSEVERITY_ERROR, _T("HTTP status code %d => Error-Code %d"), nHttpStatusCode, (int)dwError);
	}
}

/** Cleans Object
	@param bLostConnection
		true, if connection is no longer available and remote objects
			shall not be tidied up
		false, if connection is apparently avilable
			and remote objects shall be tidied up via http
*/
void CAbkClient::TidyUp(bool bLostConnection)
{
	bool bLongPollSelfTerminated = m_bTerminateLongPoll; // if true, indicates that the longpoll thread terminated itself due to an error

	// stop the long-polling thread
	if (1)
	{
		CClientPtrRef pClientAux(m_pClientAux);
		CClientPtrRef pClientEvent(m_pClientEvent);
		if (pClientEvent.IsValid())
		{
			m_bTerminateLongPoll = true;
			m_evLongPollEnable.Set(); // in case the long-poll-thread is stalled, wake it up so it can terminate
			//WaitForSingleObject(m_evLongPollDone.m_hObject, LONGPOLL_FAILURE_TOLERANCE_MS * 2); // wait until terminated
			m_evLongPollDone.Wait(LONGPOLL_FAILURE_TOLERANCE_MS * 2);
			pClientEvent->Close(); // close connection so request of long-polling gets interrrupted
			m_bTerminateLongPoll = false;
		}

		if (bLostConnection)
		{
			if (pClientAux.IsValid())
				pClientAux->SetServerAddr("", 0); // inhibit further requests since connection is dead
			if (pClientEvent.IsValid())
				pClientEvent->SetServerAddr("", 0);
		}

		// tidy-up aux items
		if (pClientAux.IsValid())
		{
			// delete the daqs
			std::map<std::string, CAbkClientDaq *>::iterator iterDaq;
			for (iterDaq = m_mapDaq.begin(); iterDaq != m_mapDaq.end(); ++iterDaq)
			{
				if (bLongPollSelfTerminated) // if it is likely that the server is inresponsive..
					iterDaq->second->m_pOwner = NULL; // prevent the daq from deleting at the server
				delete iterDaq->second;
			}
			m_mapDaq.clear();

			// delete session
			if (IsConnected() && !bLongPollSelfTerminated)
				pClientAux->DeleteSession(m_nSessionId);

			pClientAux->Close();
		}

	}

	// finally delete the clients
	m_pClientEvent.Delete();
	m_pClientAux.Delete();

	m_strServerAddress = "";
	m_nPort = 0;
	m_strPort.clear();
	m_nSessionId = -1;
}

void CAbkClient::SetServerAddr(LPCTSTR pszServerAddress, int nPort)
{
	if ((m_nPort != nPort) || (boost::equals(pszServerAddress, m_strServerAddress))) // if changes in address or port
	{
		// TODO:
		Create(pszServerAddress, nPort, m_pNextEventData, CA2T(m_strClientClass.c_str()), CA2T(m_strClientType.c_str()), CA2T(m_strClientSerial.c_str()));
	}
}

int CAbkClient::GetPort(void) const
{
	return m_nPort;
}

int CAbkClient::GetSessionId()
{
	return m_nSessionId;
}

//--------------------------------------------------------------------------
// GetClientConfigInfo()      queries the client configuration/app information
// ---------------------
// Input: strUrl = [out] URL where to download the configuration file. if no file
//                 is provided, this string will get empty
//        strMd5 = [out] MD5 of the file, only valid if strUrl is not empty
//        pszClientType=NULL = [in, optional] client type name when querying
//                             the info. If NULL, the standard client type which
//                             was specified in Create() will be used
// Return: true on success, even if no config file is available and the strUrl was
//              emptied
//         false on error or if server does not support client config hosting

bool CAbkClient::GetClientConfigInfo(CString &strUrl, CString &strMd5, LPCTSTR pszClientType/*=NULL*/)
{
	CClientPtrRef pClientAux(m_pClientAux);
	//assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;

	// compose and send request
	if (!pszClientType) // if no client type name specified, use the stored one
		pszClientType = (LPCTSTR)m_strClientType.c_str();
	CJsonFormatter jfReq;
	jfReq.WriteValue(ABK_REQ_CLIENTCONFIG_CLASS, m_strClientClass);
	jfReq.WriteValue(ABK_REQ_CLIENTCONFIG_TYPE, CT2A(pszClientType));
	jfReq.WriteValue(ABK_REQ_CLIENTCONFIG_SERIAL, m_strClientSerial);
	std::string strResponse = pClientAux->NavigatePost(_T(ABK_REQUESTURL_CLIENTCONFIG_INFO), -1, &jfReq); // send own info and get config file info
	if (strResponse.empty())
		return false;

	// decode response
	CJsonParserAtl jpResp(strResponse.c_str());
	bool bUrlDecoded = false;
	bool bMd5Decoded = false;
	for (; !jpResp.IsDone(); ++jpResp)
	{
		bUrlDecoded |= jpResp.ExtractValueAtl(ABK_RSP_CLIENTCONFIG_URL, strUrl); // get URL
		bMd5Decoded |= jpResp.ExtractValueAtl(ABK_RSP_CLIENTCONFIG_MD5, strMd5); // get MD5 hash
	}
	if (!bUrlDecoded)
	{
		AddLog(LOGSEVERITY_ERROR, _T("ClientConfig info response: URL field is missing"));
		return false;
	}
	if (!strUrl.IsEmpty() && (!bMd5Decoded || strMd5.IsEmpty())) // if there was an URL returned, a valid MD5 must be there too
	{
		AddLog(LOGSEVERITY_ERROR, _T("ClientConfig info response: MD5 field is missing"));
		return false;
	}
	return true;
}

bool CAbkClient::GetForm(LPCTSTR pszFormName, std::list<CFormElement>& lstGet, CString & strCaptionGet, int & nPersitenceMs)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	CString strUrl;
	strUrl.Format(_T("%s/%s"), _T(ABK_SERVICE_FORMS), pszFormName);
	std::string strResponse = pClientAux->NavigateGet(strUrl, -1);
	if (strResponse.empty())
		return false;
	CJsonParserAtl jpForm(strResponse.c_str());
	bool bCaptionDecoded = false;
	nPersitenceMs = 0; // default: infinite display time
	for (; !jpForm.IsDone(); ++jpForm)
	{
		bCaptionDecoded |= jpForm.ExtractValueAtl(ABK_RSP_FORMS_CAPTION, strCaptionGet); // get caption of the form
		jpForm.ExtractValue(ABK_RSP_FORMS_PERSISTENCE, &nPersitenceMs); // get persistence time
		if (jpForm.TestArray(ABK_RSP_FORMS_CONTROLS)) // is there "Controls":[
		{
			for (++jpForm; !jpForm.IsDone(); ++jpForm) // each element
			{
				CFormElement elGet;
				if (!elGet.DecodeJson(jpForm)) // decode the element
					return false; // error in element
				lstGet.push_back(elGet);
			}
		}
		jpForm.SkipItem();
	}
	if (!bCaptionDecoded)
	{
		AddLog(LOGSEVERITY_ERROR, _T("Caption is missing in form \"%s\""), pszFormName);
		return false;
	}
	return true;
}

/** Overload to consume messages on queue */
/*virtual*/ void CAbkClient::OnLogAdded(void)
{

}

DWORD Abk::CAbkClient::OnLongPollErrorResponse(int nHttpStatusCode, int nSessionId)
{
	// in derived classes, handle the error. No need to call the base class implementation.
	DWORD dwWaitBeforeResume = 0;
	if (nHttpStatusCode >= 400 && nHttpStatusCode <= 499)
		dwWaitBeforeResume = INFINITE; // default: no further requests
	return dwWaitBeforeResume;
}

//---------------------------------------------------------------------------------------

CAbkClient::CFormElement::CFormElement()
{
	m_nType = TYPE_INVALID;
	m_nMaxLen = 0;
}


CAbkClient::CFormElement::~CFormElement()
{
	VariantClear(&m_varValue);  // clear byte array e.g. when m_varValue holds a string
}


/** Decodes a JSON and sets member variables accordingly
	@param jpElement
		JSON parser containing a form element
	@return
		TRUE on success, FALSE on decode error
*/
bool CAbkClient::CFormElement::DecodeJson(CJsonParserAtl &jpElement)
{
	bool bNameSent = false;
	bool bCaptionSent = false;
	bool bTypeSent = false;
	bool bValueSent = false;
	CString strType;
	m_lstOptions.clear(); // empty the option list
	m_nMaxLen = 0;
	m_nFlags = 0;
	m_nType = TYPE_INVALID;

	for (++jpElement; !jpElement.IsDone(); ++jpElement) // each attribute
	{
		bNameSent |= jpElement.ExtractValueAtl(ABK_RSP_FORMS_CONTROL_NAME, m_strName);
		bCaptionSent |= jpElement.ExtractValueAtl(ABK_RSP_FORMS_CONTROL_CAPTION, m_strCaption);
		bTypeSent |= jpElement.ExtractValueAtl(ABK_RSP_FORMS_CONTROLTYPE, strType);
		bValueSent |= jpElement.ExtractValueAtl(ABK_RSP_FORMS_CONTROL_INITIALVALUE, m_varValue);
		jpElement.ExtractValue(ABK_RSP_FORMS_CONTROL_MAXLEN, &m_nMaxLen); // ocassionally decode the maximum len
		if (jpElement.TestArray(ABK_RSP_FORMS_CONTROL_OPTIONS)) // is there "Options":[
		{
			CString strOption;
			for (++jpElement; !jpElement.IsDone(); ++jpElement) // each option
			{
				jpElement.ExtractValueAtl(strOption);
				m_lstOptions.push_back(strOption);
			}
		}

		// test for additional attributes
		bool bAttribute;
		if (jpElement.ExtractValue(ABK_RSP_FORMS_CONTROL_READONLY, &bAttribute) && bAttribute)
			m_nFlags |= READONLY;
		if (jpElement.ExtractValue(ABK_RSP_FORMS_CONTROL_PASSWORD, &bAttribute) && bAttribute)
			m_nFlags |= PASSWORD;
		if (jpElement.ExtractValue(ABK_RSP_FORMS_CONTROL_NUMERIC, &bAttribute) && bAttribute)
			m_nFlags |= NUMERIC;
		if (jpElement.ExtractValue(ABK_RSP_FORMS_CONTROL_SUBMIT, &bAttribute) && bAttribute)
			m_nFlags |= SUBMIT;
		if (jpElement.ExtractValue(ABK_RSP_FORMS_CONTROL_CANCEL, &bAttribute) && bAttribute)
			m_nFlags |= CANCEL;
		if (jpElement.ExtractValue(ABK_RSP_FORMS_CONTROL_UPDATEABLE, &bAttribute) && bAttribute)
			m_nFlags |= UPDATEABLE;
	}

	// decode type
	if (!strType.Compare(_T(ABK_RSP_FORMS_CONTROLTYPE_INPUT)))
		m_nType = TYPE_EDIT;
	else if (!strType.Compare(_T(ABK_RSP_FORMS_CONTROLTYPE_CHECKBOX)))
		m_nType = TYPE_CHECKBOX;
	else if (!strType.Compare(_T(ABK_RSP_FORMS_CONTROLTYPE_COMBO)))
		m_nType = TYPE_COMBO;
	else if (!strType.Compare(_T(ABK_RSP_FORMS_CONTROLTYPE_BUTTON)))
		m_nType = TYPE_BUTTON;
	else
		return false;

	// test if all mandatory fields were present
	if (!bNameSent || !bCaptionSent || !bTypeSent)
		return false;
	if ((m_nType != TYPE_BUTTON) && (!bValueSent)) // all excapt button requires a initial value field
		return false;

	return true;
}


BOOL CAbkClient::CClientPtr::Delete(void)
{
	if (!m_pClient)
		return TRUE; // successfully deleted nothing
	BOOL bSuccess = FALSE;
	for (int nRetry = 0; nRetry < 100; nRetry++)
	{
		if (m_nUsage.load(boost::memory_order_acquire) == 0)
		{
			delete m_pClient;
			m_pClient = NULL;
			bSuccess = TRUE;
			break;
		}
		Sleep(10);
	}
	return bSuccess;
}

}