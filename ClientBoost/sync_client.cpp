//
// sync_client.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "stdafx.h"

#include "JsonFormatter.h"
#include "JsonParser.h"
#include "ValuesFromSpec.h"

#include "LogQueue.h"
#include "AbkServerEvent.h"

using namespace Abk;
using boost::asio::ip::tcp;

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
	CAbkClient() : resolver(io_context), socket(io_context), socket_long_poll(io_context)
	{
	}

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
		std::size_t operator ()(const std::string &value) const
		{
			std::string copy(value);
			boost::to_lower(copy);
			return boost::hash<std::string>()(copy);
		}
	};


	typedef boost::unordered_map<std::string, eHttpHeaders, insensitive_hash> HashMap;
	// Maps a string to enum values
	const HashMap m_HashMap = {
		{"Content-Length", E_HEADER_CONTENT_LENGTH},
		{"Connection", E_HEADER_CONNECTION}
	};

	size_t WriteToSocket(tcp::socket & a_Socket, const std::string & a_Path, eHttpRequestType a_Type)
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
		catch (std::exception & e)
		{
			throw AbkNetworkException();
		}
	}

	size_t WriteToSocket(tcp::socket & a_Socket, const std::string & a_Path, const std::string & a_Message, eHttpRequestType a_Type)
	{
		try
		{
			boost::asio::streambuf request;
			std::ostream request_stream(&request);

			switch(a_Type)
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
		catch (std::exception & e)
		{
			throw AbkNetworkException();
		}
	}

	eHttpHeaders GetEnumFromString(const std::string &str) const
	{
		eHttpHeaders returnValue = E_HEADER_INVALID;
		HashMap::const_iterator search = m_HashMap.find(str);
		if(search != m_HashMap.end())
		{
			returnValue = search->second;
		}
		return returnValue;
	}

	size_t ReadFromSocket(tcp::socket & a_Socket, std::string & a_Message, unsigned int *pnStatusCode = nullptr)
	{
		std::stringstream sstream;
		size_t bytes = ReadFromSocket(a_Socket, sstream, pnStatusCode);
		a_Message = sstream.str();
		return bytes;
	}

	size_t ReadFromSocket(tcp::socket & a_Socket, std::ostream & sstream, unsigned int *pnStatusCode = nullptr)
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
			if(pnStatusCode)
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
			//std::cerr << "Failed to read from socket: " << e.what() << std::endl;
			throw AbkNetworkException();
		}
		return 0;
	}

public:
	bool IsConnected()
	{
		return socket.is_open();
	}

private:

	bool EnsureConnection()
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
			std::cerr << "Failed to connect due to exception" << std::endl;
			return false;
		}
	}

public:
	CJsonFormatter formatter;
	std::string NavigatePost(LPCTSTR pszPath, int nSessionId, CJsonFormatter *pPostData)
	{
		assert(pPostData);
		assert(pszPath);
		assert(!m_pszServerAddress.empty());
		//assert(pPostData->GetStream());
		//assert(pPostData->GetStream()->rdbuf()->in_avail > 0);
		std::string strPostData=pPostData->GetStream()->str();
		std::string response;

		if(!EnsureConnection())
			return response;

		try
		{
			WriteToSocket(socket, pszPath, strPostData, E_HTTP_POST);
			ReadFromSocket(socket, response);

			std::cout << "Response was: " << response << std::endl;
		}
		catch(AbkNetworkException &e)
		{
			std::cerr << "Failed to navigate POST due to Network exception" << std::endl;
			response.clear();
		}
		return response;
	}

// TODO: MIME type
	bool NavigatePut(LPCTSTR pszPath, int nSessionId, CJsonFormatter *pPutData)
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
			WriteToSocket(socket, pszPath, strPutData, E_HTTP_PUT);
			ReadFromSocket(socket, response, &status_code);

			std::cout << "Response was: " << response << std::endl;
		}
		catch (AbkNetworkException &e)
		{
			std::cerr << "Failed to navigate POST due to Network exception" << std::endl;
			response.clear();
		}
		return (status_code == 200);
	}

/** Used to retrieve a string at a certain path
	@param pszPath
		Path on the server
	@param nSessionId
		Session ID to use in query, set to -1 to query without
	@return
		Response string
 */
	std::string NavigateGet(LPCTSTR pszPath, int nSessionId)
	{
		assert(pszPath);
		std::string response;

		if(!EnsureConnection())
			return response;

		try
		{
			if (nSessionId >= 0)
			{
				CString strPathAndQuery;
				strPathAndQuery.Format(_T("%s?") _T(ABK_QRY_SESSIONID) _T("=%d"), pszPath, nSessionId);
				WriteToSocket(socket, std::string(strPathAndQuery), E_HTTP_GET);
			}
			else
			{
				WriteToSocket(socket, std::string(pszPath), E_HTTP_GET);
			}
			ReadFromSocket(socket, response);
		}
		catch(AbkNetworkException &e)
		{
			std::cerr << "Failed to navigate GET due to Network exception" << std::endl;
			response.clear();
		}

		return response;
	}

	bool NavigateGet(LPCTSTR pszPath, int nSessionId, std::ostream & out)
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
				WriteToSocket(socket, std::string(strPathAndQuery), E_HTTP_GET);
			}
			else
			{
				WriteToSocket(socket, std::string(pszPath), E_HTTP_GET);
			}
			ReadFromSocket(socket, out);
			return true;
		}
		catch (AbkNetworkException &e)
		{
			std::cerr << "Failed to navigate GET due to Network exception" << std::endl;
		}

		return false;
	}

	void AddLog(LOGSEVERITY nSeverity, LPCTSTR pszMessage, ...)
	{
		va_list args;
		va_start(args, pszMessage);
		//AddLogV(nSeverity,pszMessage,args);
		vprintf(pszMessage, args);
		va_end(args);
	}

	int ObtainSessionId(LPCTSTR pszClientClass, LPCTSTR pszClientType, LPCTSTR pszClientSerial /*=NULL*/)
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

	bool Create(LPCTSTR pszServerAddress, int nPort, CAbkServerEvent *pEventRxBuffer, LPCTSTR pszClientClass, LPCTSTR pszClientType, LPCTSTR pszClientSerial = NULL)
	{
		std::stringstream sstream;

		sstream << nPort;
		m_pszPort = std::string(sstream.str());
		m_pszServerAddress = std::string(pszServerAddress);

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

		if(nPort > 0)
		{
			// Connect and obtain session id
			m_nSessionId = ObtainSessionId(pszClientClass, pszClientType, pszClientSerial);
			if(m_nSessionId)
			{
				// Start long-polling thread
			}
		}

		return true;
	}

	std::string GetServerInfo(void)
	{
		if(!IsConnected())
			return nullptr;

		return NavigateGet(_T(ABK_REQUESTURL_SERVERINFO), -1);
	}



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
	virtual CAbkServerEvent *OnServerEvent(CAbkServerEvent *pEventData)
	{
		// default implementation: use the same event object for the next event
		return pEventData;
	}

	bool DownloadFile (LPCTSTR pszUrl, std::ostream & out)
	{
		return NavigateGet(pszUrl, -1, out);
	}

	bool DownloadFile (LPCTSTR pszUrl, LPCTSTR pszStorePath)
	{
		std::ofstream file(CT2A(pszStorePath), std::ofstream::out);
		return DownloadFile(pszUrl, file);
	}

	template <typename U>
	bool SetVarOrMailboxValue (LPCTSTR pszPath, const char *pszName, const U *pSet)
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

	bool SetVarValue (LPCTSTR pszVarName, const double dSet)
	{
		return SetVarOrMailboxValue(_T(ABK_REQUESTURL_VARVALUE), CT2A(pszVarName), &dSet);
	}

	bool GetVarOrMailboxList(LPCTSTR pszPath, std::list<CString> *pGet)
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

	bool GetVarList (std::list<CString> *pGet)
	{
		return GetVarOrMailboxList(_T(ABK_REQUESTURL_VARLIST), pGet);
	}
};

int main(int argc, char *argv[])
{
	CAbkClient testClient;
	CAbkServerEvent seAbk;
	testClient.Create(_T("localhost"), 8080, &seAbk, _T("Display"), _T("EMBU-Sys_EMBU-Boost"), _T("01-23-45-67-89-ab"));

	int sessionId = testClient.ObtainSessionId("Display", "EMBU-Sys_EMBU-Boost", "12-34-45-67-89-0a");

	std::cout << "Obtained session id: " << sessionId << std::endl;
	std::cout << "Server info: " << testClient.GetServerInfo() << std::endl;

	testClient.DownloadFile(_T(ABK_REQUESTURL_VARLIST), "varlist.txt");

	char bufDest[8192];
	boost::iostreams::stream<boost::iostreams::array_sink> memoryStream(bufDest, sizeof(bufDest));
	testClient.DownloadFile(_T("/abk/client_states/Display_AbkDemoOnBrowser_0.txt"), memoryStream);

	bool bSuccess;
	bSuccess = testClient.SetVarValue(_T("Reifendruck"), 10.0);
	bSuccess = testClient.SetVarValue(_T("Vari3"), 10.0);
	if(bSuccess)
		std::cout << "Successfully set value" << std::endl;

	std::list<CString> varlist;
	testClient.GetVarList(&varlist);

	return 0;
}
