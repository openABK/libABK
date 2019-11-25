#include "stdafx.h"

#include "AbkClient.h"
#include "AbkClientDaq.h"
#include "JsonParserAtl.h"

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

size_t CAbkClient::CBaseAbstraction::ReadFromSocket(tcp::socket &a_Socket, std::string &a_Message, unsigned int *pnStatusCode)
{
	std::stringstream sstream;
	size_t bytes = ReadFromSocket(a_Socket, sstream, pnStatusCode);
	a_Message = sstream.str();
	return bytes;
}

size_t CAbkClient::CBaseAbstraction::ReadFromSocket(tcp::socket &a_Socket, std::ostream &sstream, unsigned int *pnStatusCode)
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
		unsigned int status_code;
		response_stream >> status_code;
		if (pnStatusCode)
			*pnStatusCode = status_code;

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
		if (!(status_code == 200 || (status_code >= 400 && status_code <= 499)))
		{
#ifdef LOG_BOOST_ABK
			Log() << "Response returned with status code " << status_code << "\n";
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
		if (responseBytesRead < responseBytesExpected)
		{
			boost::system::error_code error;
			while (responseBytesRead += boost::asio::read(socket, response,
														  boost::asio::transfer_at_least(responseBytesExpected - responseBytesRead), error))
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
		throw AbkNetworkException();
	}
	return 0;
}

bool CAbkClient::CBaseAbstraction::IsSocketOpen() const
{
	return socket.is_open();
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

			tcp::resolver::results_type endpoints = resolver.resolve(m_strServerAddress, m_strPort);
			boost::asio::connect(socket, endpoints);
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

bool CAbkClient::CBaseAbstraction::DeleteSession(int nSessionId)
{
	CJsonFormatter jfSend;
	//return NavigateDelete(_T(ABK_REQUESTURL_SESSIONID), nSessionId, &jfSend);
	// TODO:
	return true;
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
	unsigned int status_code = 400;

	if (!EnsureConnection())
		return false;

	try
	{
		WriteToSocket(socket, std::string(CT2A(pszPath)), strPutData, E_HTTP_PUT);
		ReadFromSocket(socket, response, &status_code);

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
	return (status_code == 200);
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
}


/*virtual*/ CAbkClient::~CAbkClient()
{
	TidyUp(false);
}

bool CAbkClient::Create(LPCTSTR pszServerAddress, int nPort, CAbkServerEvent *pEventRxBuffer, LPCTSTR pszClientClass, LPCTSTR pszClientType, LPCTSTR pszClientSerial)
{
	bool bSuccess = false;
	std::stringstream sstream;

	sstream << nPort;
	m_strPort = std::string(sstream.str());


	m_nPort = nPort;
	m_strServerAddress = std::string(CT2A(pszServerAddress));

#ifdef LOG_BOOST_ABK
	Log() << "Printing port: " << m_pszPort << std::endl;
#endif

	//boost::asio::connect(socket, endpoints);

	const std::string storage_info = "/abk/system_information/storage_info?SessionId=4";
	const std::string server_event = "/abk/events/server_event?SessionId=4";

	if (nPort > 0)
	{
		// create clients
		m_pClientAux = new CBaseAbstraction(this);
		m_pClientEvent = new CBaseAbstraction(this);
		CClientPtrRef pClientAux(m_pClientAux);
		CClientPtrRef pClientEvent(m_pClientEvent);
		pClientAux->SetServerAddr(m_strServerAddress, nPort);
		pClientEvent->SetServerAddr(m_strServerAddress, nPort);

		// Connect and obtain session id
		m_nSessionId = pClientAux->ObtainSessionId(pszClientClass, pszClientType, pszClientSerial);
		if (m_nSessionId >= 0)
		{
			// Start long-polling thread
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

CAbkServerEvent *CAbkClient::OnServerEvent(CAbkServerEvent *pEventData)
{
	// default implementation: use the same event object for the next event
	return pEventData;
}

bool CAbkClient::DownloadFile(LPCTSTR pszUrl, std::ostream &out)
{
	CClientPtrRef pClientAux(m_pClientAux);
	if (!pClientAux.IsValid())
		return false;
	bool bSuccess = false;
	bSuccess = pClientAux->NavigateGet(pszUrl, -1, out);
	return bSuccess;
}

bool CAbkClient::DownloadFile(LPCTSTR pszUrl, LPCTSTR pszStorePath)
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

bool CAbkClient::GetVarList(std::list<CString> *pGet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	return pClientAux->GetVarOrMailboxList(_T(ABK_REQUESTURL_VARLIST), pGet);
}

std::size_t CAbkClient::CBaseAbstraction::insensitive_hash::operator()(const std::string &value) const
{
	std::string copy(value);
	boost::to_lower(copy);
	return boost::hash<std::string>()(copy);
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
			//m_evLongPollEnable.SetEvent(); // in case the long-poll-thread is stalled, wake it up so it can terminate
			//WaitForSingleObject(m_evLongPollDone.m_hObject, LONGPOLL_FAILURE_TOLERANCE_MS * 2); // wait until terminated
			//pClientEvent->Close(); // close connection so request of long-polling gets interrrupted
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

int CAbkClient::GetSessionId()
{
	return m_nSessionId;
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