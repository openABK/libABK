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

public:
	CAbkClient() : resolver(io_context), socket(io_context)
	{
	}

public:
	CJsonFormatter formatter;
	const char *NavigatePost(LPCTSTR pszPath, int nSessionId, CJsonFormatter *pPostData)
	{
		std::cout << pszPath << std::endl;
		return "{\"SessionId\":19}";
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

		const char *pReturn = NavigatePost(_T(ABK_REQUESTURL_SESSIONID), -1, &jfPost);
		if (!pReturn)
			return -1;

		// extract the session id
		CJsonParser parsResponse(pReturn);
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
		std::string szPort(sstream.str());
		std::string hostname(pszServerAddress);

		std::cout << "Printing port: " << szPort << std::endl;

		tcp::resolver::results_type endpoints = resolver.resolve(hostname, szPort);
		boost::asio::connect(socket, endpoints);

		const std::string storage_info = "/abk/system_information/storage_info?SessionId=4";
		const std::string server_event = "/abk/events/server_event?SessionId=4";

		// Form the request. We specify the "Connection: close" header so that the
		// server will close the socket after transmitting the response. This will
		// allow us to treat all data up until the EOF as the content.
		boost::asio::streambuf request;
		std::ostream request_stream(&request);

		//request_stream << "GET " << storage_info /*argv[2]*/ << " HTTP/1.0\r\n";
		//request_stream << "Host: " << argv[1] << "\r\n";
		//request_stream << "Accept: */*\r\n";
		//request_stream << "User-Agent: AbkClientBoost\r\n";
		//request_stream << "Connection: keep-alive\r\n\r\n";

		const std::string session_id_body = "{\"Class\":\"Display\",\"Type\":\"EMBU-Sys_EMBU-Boost\",\"Serial\":\"40-8d-5c-c2-6d-79\"}";
		std::cout << "Size of body: " << session_id_body.size() << std::endl;
		request_stream << "POST "
					   << "/abk/system_information/session_id"
					   << " HTTP/1.0\r\n";
		request_stream << "Host: " << hostname << "\r\n";
		request_stream << "Content-Length: " << session_id_body.size() << "\r\n";
		request_stream << "Content-Type: application/json\r\n";
		request_stream << "User-Agent: AbkClientBoost\r\n";
		request_stream << "Connection: keep-alive\r\n\r\n";

		request_stream << session_id_body;

		// Send the request.
		boost::asio::write(socket, request);

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
			return 1;
		}
		if (status_code != 200)
		{
			std::cout << "Response returned with status code " << status_code << "\n";
			return 1;
		}

		// Read the response headers, which are terminated by a blank line.
		boost::asio::read_until(socket, response, "\r\n\r\n");

		// Process the response headers.
		std::string header;
		while (std::getline(response_stream, header) && header != "\r")
			std::cout << header << "\n";
		std::cout << "\n";

		// Write whatever content we already have to output.
		if (response.size() > 0)
			std::cout << &response;

		// Read until EOF, writing data to output as we go.
		boost::system::error_code error;
		while (boost::asio::read(socket, response,
								 boost::asio::transfer_at_least(1), error))
			std::cout << &response;
		if (error != boost::asio::error::eof)
			throw boost::system::system_error(error);

		return true;
	}
};

int main(int argc, char *argv[])
{
	CAbkClient testClient;
	CJsonFormatter formatter;
	testClient.NavigatePost("/abk/events/server_event", 1, &formatter);
	int sessionId = testClient.ObtainSessionId("Display", "EMBU-Sys_EMBU-Boost", "12-34-45-67-89-0a");

	std::cout << "Got session id: " << sessionId << std::endl;

	bool bPrivate = true;
	formatter.WriteValue("private", bPrivate);
	formatter.Close();

	std::cout << "Formatter output: " << formatter.GetStream()->str() << std::endl;
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
