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
#include <iostream>
#include <istream>
#include <ostream>
#include <string>

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
					break;
				case E_HTTP_POST:
					request_stream << "POST ";
					break;
				case E_HTTP_PUT:
					request_stream << "PUT ";
					break;
				default:
					request_stream << "GET ";
					break;
			}
			
			request_stream << "/abk/system_information/session_id";
			request_stream << " HTTP/1.0\r\n";

			request_stream << "Host: " << m_pszServerAddress << "\r\n";
			request_stream << "Content-Length: " << a_Message.size() << "\r\n";
			request_stream << "Content-Type: application/json\r\n";
			request_stream << "User-Agent: AbkClientBoost\r\n";
			request_stream << "Connection: keep-alive\r\n\r\n";
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

	size_t ReadFromSocket(tcp::socket & a_Socket, std::string & a_Message)
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
		std::string status_message;
		std::getline(response_stream, status_message);
		if (!response_stream || http_version.substr(0, 5) != "HTTP/")
		{
			std::cout << "Invalid response\n";
			return -1;
		}
		if (status_code != 200)
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
				if(headerType == E_HEADER_CONTENT_LENGTH)
				{
					if(!boost::conversion::try_lexical_convert<size_t, std::string>(header_tokens[1], responseBytesExpected))
						responseBytesExpected = -1;
				}
		}
		std::cout << "\n";

		std::stringstream sstream;

		size_t responseBytesRead = response.size();
		if (responseBytesRead > 0)
			sstream << &response;

		// Read until EOF, writing data to output as we go.
		boost::system::error_code error;
		while (boost::asio::read(socket, response,
								 boost::asio::transfer_at_least(1), error))
			sstream << &response;
		if (error != boost::asio::error::eof)
			throw boost::system::system_error(error);

		a_Message = sstream.str();
		return responseBytesExpected;
	}

public:
	bool IsConnected()
	{
		return socket.is_open();
	}

public:
	CJsonFormatter formatter;
	std::string NavigatePost(LPCTSTR pszPath, int nSessionId, CJsonFormatter *pPostData)
	{
		assert(pPostData);
		assert(!m_pszServerAddress.empty());
		//assert(pPostData->GetStream());
		//assert(pPostData->GetStream()->rdbuf()->in_avail > 0);
		std::string strPostData=pPostData->GetStream()->str();

		if(!IsConnected())
		{
			tcp::resolver::results_type endpoints = resolver.resolve(m_pszServerAddress, m_pszPort);
			boost::asio::connect(socket, endpoints);
		}

		std::string response;
		try
		{
			WriteToSocket(socket, pszPath, strPostData, E_HTTP_POST);
			ReadFromSocket(socket, response);

			std::cout << "Response was: " << response << std::endl;
		}
		catch(AbkNetworkException &e)
		{
			std::cerr << "Failed to navigate post due to Network exception" << std::endl;
			response.clear();
		}
		return response;
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
		boost::asio::connect(socket, endpoints);

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
};

int main(int argc, char *argv[])
{
	CAbkClient testClient;
	//CJsonFormatter formatter;
	//testClient.NavigatePost("/abk/events/server_event", 1, &formatter);


	//std::cout << "Got session id: " << sessionId << std::endl;

	//bool bPrivate = true;
	//formatter.WriteValue("private", bPrivate);
	//formatter.Close();

	//std::cout << "Formatter output: " << formatter.GetStream()->str() << std::endl;
	try
	{
		/*     if (argc != 3)
    {
      std::cout << "Usage: sync_client <server> <path>\n";
      std::cout << "Example:\n";
      std::cout << "  sync_client www.boost.org /LICENSE_1_0.txt\n";
      return 1;
    } */

		//boost::asio::io_context io_context;

		//const std::string hostname = "localhost";

		CAbkServerEvent seAbk;
		testClient.Create(_T("localhost"), 8080, &seAbk, _T("Display"), _T("EMBU-Sys_EMBU-Boost"), _T("01-23-45-67-89-ab"));

		int sessionId = testClient.ObtainSessionId("Display", "EMBU-Sys_EMBU-Boost", "12-34-45-67-89-0a");

		std::cout << "Obtained session id" << sessionId << std::endl;

		// Get a list of endpoints corresponding to the server name.
		//tcp::resolver resolver(io_context);
		//tcp::resolver::results_type endpoints = resolver.resolve(hostname, "8080");

		// Try each endpoint until we successfully establish a connection.
		//tcp::socket socket(io_context);
		//boost::asio::connect(socket, endpoints);
	}
	catch (std::exception &e)
	{
		std::cout << "Exception: " << e.what() << "\n";
	}

	return 0;
}
