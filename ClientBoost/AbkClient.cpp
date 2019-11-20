#include "stdafx.h"

#include "AbkClient.h"

using namespace Abk;

CAbkClient::CAbkClient() : resolver(io_context), socket(io_context), socket_long_poll(io_context)
{
}

size_t CAbkClient::WriteToSocket(tcp::socket &a_Socket, const std::string &a_Path, CAbkClient::eHttpRequestType a_Type)
{
	try
	{
		boost::asio::streambuf request;
		std::ostream request_stream(&request);

		assert(a_Type == E_HTTP_GET);

		request_stream << "GET ";
		request_stream << a_Path;
		request_stream << " HTTP/1.1\r\n";

		request_stream << "Host: " << m_pszServerAddress << "\r\n";
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

size_t CAbkClient::WriteToSocket(tcp::socket &a_Socket, const std::string &a_Path, const std::string &a_Message, CAbkClient::eHttpRequestType a_Type)
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

		request_stream << "Host: " << m_pszServerAddress << "\r\n";
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

CAbkClient::eHttpHeaders CAbkClient::GetEnumFromString(const std::string &str) const
{
	eHttpHeaders returnValue = E_HEADER_INVALID;
	HashMap::const_iterator search = m_HashMap.find(str);
	if (search != m_HashMap.end())
	{
		returnValue = search->second;
	}
	return returnValue;
}

size_t CAbkClient::ReadFromSocket(tcp::socket &a_Socket, std::string &a_Message, unsigned int *pnStatusCode)
{
	std::stringstream sstream;
	size_t bytes = ReadFromSocket(a_Socket, sstream, pnStatusCode);
	a_Message = sstream.str();
	return bytes;
}

size_t CAbkClient::ReadFromSocket(tcp::socket &a_Socket, std::ostream &sstream, unsigned int *pnStatusCode)
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
			std::cout << "Invalid response\n";
			return -1;
		}
		// We still want to receive error messages
		if (!(status_code == 200 || (status_code >= 400 && status_code <= 499)))
		{
			std::cout << "Response returned with status code " << status_code << "\n";
			return -1;
		}

		// Read the response headers, which are terminated by a blank line.
		boost::asio::read_until(socket, response, "\r\n\r\n");

		size_t responseBytesExpected;

		// Process the response headers.
		std::string header;
		while (std::getline(response_stream, header) && header != "\r")
		{
			std::cout << header << "\n";

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
		std::cout << "\n";

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

bool CAbkClient::IsConnected()
{
	return socket.is_open();
}

bool CAbkClient::EnsureConnection()
{
	try
	{
		if (!IsConnected())
		{

			tcp::resolver::results_type endpoints = resolver.resolve(m_pszServerAddress, m_pszPort);
			boost::asio::connect(socket, endpoints);
		}

		return IsConnected();
	}
	catch (boost::exception &e)
	{
		boost::ignore_unused(e);
		std::cerr << "Failed to connect due to exception" << std::endl;
		return false;
	}
}

std::string CAbkClient::NavigatePost(LPCTSTR pszPath, int nSessionId, CJsonFormatter *pPostData)
{
	assert(pPostData);
	assert(pszPath);
	assert(!m_pszServerAddress.empty());
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

		std::cout << "Response was: " << response << std::endl;
	}
	catch (AbkNetworkException &e)
	{
		boost::ignore_unused(e);
		std::cerr << "Failed to navigate POST due to Network exception" << std::endl;
		response.clear();
	}
	return response;
}

bool CAbkClient::NavigatePut(LPCTSTR pszPath, int nSessionId, CJsonFormatter *pPutData)
{
	assert(pPutData);
	assert(pszPath);
	assert(!m_pszServerAddress.empty());
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

		std::cout << "Response was: " << response << std::endl;
	}
	catch (AbkNetworkException &e)
	{
		boost::ignore_unused(e);
		std::cerr << "Failed to navigate POST due to Network exception" << std::endl;
		response.clear();
	}
	return (status_code == 200);
}

std::string CAbkClient::NavigateGet(LPCTSTR pszPath, int nSessionId)
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
		std::cerr << "Failed to navigate GET due to Network exception" << std::endl;
		response.clear();
	}

	return response;
}

bool CAbkClient::NavigateGet(LPCTSTR pszPath, int nSessionId, std::ostream &out)
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
		std::cerr << "Failed to navigate GET due to Network exception" << std::endl;
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

int CAbkClient::ObtainSessionId(LPCTSTR pszClientClass, LPCTSTR pszClientType, LPCTSTR pszClientSerial)
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
		AddLog(LOGSEVERITY_ERROR, _T("Got no session id from server."));
		return -1;
	}
	return nSessionId;
}

bool CAbkClient::Create(LPCTSTR pszServerAddress, int nPort, CAbkServerEvent *pEventRxBuffer, LPCTSTR pszClientClass, LPCTSTR pszClientType, LPCTSTR pszClientSerial)
{
	std::stringstream sstream;

	sstream << nPort;
	m_pszPort = std::string(sstream.str());
	m_pszServerAddress = std::string(CT2A(pszServerAddress));

	std::cout << "Printing port: " << m_pszPort << std::endl;

	tcp::resolver::results_type endpoints = resolver.resolve(m_pszServerAddress, m_pszPort);

	try
	{
		boost::asio::connect(socket, endpoints);
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << '\n';
	}

	//boost::asio::connect(socket, endpoints);

	const std::string storage_info = "/abk/system_information/storage_info?SessionId=4";
	const std::string server_event = "/abk/events/server_event?SessionId=4";

	if (nPort > 0)
	{
		// Connect and obtain session id
		m_nSessionId = ObtainSessionId(pszClientClass, pszClientType, pszClientSerial);
		if (m_nSessionId)
		{
			// Start long-polling thread
		}
	}

	return true;
}

std::string CAbkClient::GetServerInfo()
{
	if (!IsConnected())
		return nullptr;

	return NavigateGet(_T(ABK_REQUESTURL_SERVERINFO), -1);
}

CAbkServerEvent *CAbkClient::OnServerEvent(CAbkServerEvent *pEventData)
{
	// default implementation: use the same event object for the next event
	return pEventData;
}

bool CAbkClient::DownloadFile(LPCTSTR pszUrl, std::ostream &out)
{
	return NavigateGet(pszUrl, -1, out);
}

bool CAbkClient::DownloadFile(LPCTSTR pszUrl, LPCTSTR pszStorePath)
{
	std::ofstream file(CT2A(pszStorePath), std::ofstream::out);
	return DownloadFile(pszUrl, file);
}

bool CAbkClient::SetVarValue(LPCTSTR pszVarName, const double dSet)
{
	return SetVarOrMailboxValue(_T(ABK_REQUESTURL_VARVALUE), CT2A(pszVarName), &dSet);
}

bool CAbkClient::GetVarOrMailboxList(LPCTSTR pszPath, std::list<CString> *pGet)
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
	return GetVarOrMailboxList(_T(ABK_REQUESTURL_VARLIST), pGet);
}

std::size_t CAbkClient::insensitive_hash::operator()(const std::string &value) const
{
	std::string copy(value);
	boost::to_lower(copy);
	return boost::hash<std::string>()(copy);
}

int CAbkClient::GetSessionId()
{
	return m_nSessionId;
}

/** Overload to consume messages on queue */
/*virtual*/ void CAbkClient::OnLogAdded(void)
{

}