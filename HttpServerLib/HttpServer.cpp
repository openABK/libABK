// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------------------------
//  _____  __   __  _____  _    _        ____              
// |  ___||  \ /  ||  _  \| |  | |      / ___|  _   _  ___ 
// |  __| |   V   ||  -  /| |  | |  __  \___ \ | | | |/ __|
// | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
// |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
//                                              |___/      
// Filename:    HttpServer.cpp
// Created:     2014-01-31 (19:18)
// Author:      D. Burger
// Description: http server
//------------------------------------------------------------------------------------------------

// special thanks to Souren M. Abeghyan who gave me a lot of ideas to build this server code


#include "stdafx.h"
#include "HttpServer.h"

#include <direct.h>
#include <algorithm>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif


#define PERSISTENCE_TIMEOUT 10000 // 09.Jul.14: changed to from 1000 to 10000

#define THREADKILL_TIMEOUT_MS 1000 // time to wait for a thread to terminate



//--------------------------------------------------------------------------
// CHttpServer()           Constructor of CHttpServer
// -------------
// Input: -
// Return: 

CHttpServer::CHttpServer()
  {
  ResetStatistics();
  m_bRunning=FALSE;
  
  // build the status code-to-string map
  m_mapStatusText[100]="Continue";
  m_mapStatusText[101]="Switching Protocols";
  m_mapStatusText[102]="Processing";
  m_mapStatusText[118]="Connection timed out";
  m_mapStatusText[200]="OK";
  m_mapStatusText[201]="Created";
  m_mapStatusText[202]="Accepted";
  m_mapStatusText[203]="Non-Authoritative Information";
  m_mapStatusText[204]="No Content";
  m_mapStatusText[205]="Reset Content";
  m_mapStatusText[206]="Partial Content";
  m_mapStatusText[207]="Multi-Status";
  m_mapStatusText[300]="Multiple Choices";
  m_mapStatusText[301]="Moved Permanently";
  m_mapStatusText[302]="Found";
  m_mapStatusText[303]="See Other";
  m_mapStatusText[304]="Not Modified";
  m_mapStatusText[305]="Use Proxy";
  m_mapStatusText[307]="Temporary Redirect";
  m_mapStatusText[400]="Bad Request";
  m_mapStatusText[401]="Unauthorized";
  m_mapStatusText[402]="Payment Required";
  m_mapStatusText[403]="Forbidden";
  m_mapStatusText[404]="Not Found";
  m_mapStatusText[405]="Method Not Allowed";
  m_mapStatusText[406]="Not Acceptable";
  m_mapStatusText[407]="Proxy Authentication Required";
  m_mapStatusText[408]="Request Time-out";
  m_mapStatusText[409]="Conflict";
  m_mapStatusText[410]="Gone";
  m_mapStatusText[411]="Length Required";
  m_mapStatusText[412]="Precondition Failed";
  m_mapStatusText[413]="Request Entity Too Large";
  m_mapStatusText[414]="Request-URL Too Long";
  m_mapStatusText[415]="Unsupported Media Type";
  m_mapStatusText[416]="Requested range not satisfiable";
  m_mapStatusText[417]="Expectation Failed";
  m_mapStatusText[418]="Im a teapot";
  m_mapStatusText[421]="There are too many connections from your internet address";
  m_mapStatusText[422]="Unprocessable Entity";
  m_mapStatusText[423]="Locked";
  m_mapStatusText[424]="Failed Dependency";
  m_mapStatusText[425]="Unordered Collection";
  m_mapStatusText[426]="Upgrade Required";
  m_mapStatusText[500]="Internal Server Error";
  m_mapStatusText[501]="Not Implemented";
  m_mapStatusText[502]="Bad Gateway";
  m_mapStatusText[503]="Service Unavailable";
  m_mapStatusText[504]="Gateway Time-out";
  m_mapStatusText[505]="HTTP Version not supported";
  m_mapStatusText[506]="Variant Also Negotiates";
  m_mapStatusText[507]="Insufficient Storage";
  m_mapStatusText[509]="Bandwidth Limit Exceeded";
  m_mapStatusText[510]="Not Extended";

  // initialize the use of windows sockets
  WORD wVersionRequested=MAKEWORD(2,2);
  WSADATA wsaData;
  int nResult=WSAStartup(wVersionRequested,&wsaData);
  if(nResult==0)
    {
    if((LOBYTE(wsaData.wVersion)!=2) || HIBYTE(wsaData.wVersion)!=2)
      LogMessage(_T("Socket version 2.2 not existent\n"));
    }
  else
    LogMessage(_T("failed to initialize the use of windows(TM) sockets (WSAStartup-failure)\n"));
  }



//--------------------------------------------------------------------------
// ~CHttpServer()          Destructor of CHttpServer
// --------------
// Input: -
// Return: 

CHttpServer::~CHttpServer()
  {

  }



//--------------------------------------------------------------------------
// GetStatistics()         returns statistical information
// ---------------
// Input: statReturn = reference to recieve statistical information
// Return: -

void CHttpServer::GetStatistics (STATISTICS &statReturn)
  {
  CSingleLock lockSection(&m_statistics.m_csStatistics,TRUE);
  statReturn.llTotalRecv=m_statistics.llTotalRecv;
  statReturn.llTotalSent=m_statistics.llTotalSent;
  statReturn.nClientsConnected = m_statistics.nClientsConnected;
  }



//--------------------------------------------------------------------------
// AddConnection()       adds a client-connection and starts a client thread
// ---------------
// Input: sockConn = socket of the recently established connection
//        strClientAddr = address of the client as string
//        nPort = port of the client (ephemeral port at the client side of the TCP connection)
// Return: TRUE on success, FALSE on error

BOOL CHttpServer::AddConnection (SOCKET sockConn, const CString &strClientAddr, int nPort)
  {
  OnNewConnection(strClientAddr,nPort);
  CONNECTION *pNewConn= new CONNECTION;
  pNewConn->pServer=this;
  pNewConn->sockTcp=sockConn;
  pNewConn->strClientAddr=CT2A(strClientAddr);
  HANDLE hConnectionThread=CreateThread(NULL,0,&CHttpServer::ConnectionThreadS,pNewConn,0,NULL);
  ASSERT(hConnectionThread);
  if(!hConnectionThread)
    return FALSE;
  return TRUE;
  }



//--------------------------------------------------------------------------
// Run()                   starts the server
// -----
// Input: addrBindLocal = [in] address identifying on which adapter to work on
//                        Only the sin_port and sin_addr is used. Ohter members are ignored
//                        sin_addr can be set to INADDR_ANY if only one network adapter is present or if the server shall accept connections on all adapters
//                        ATTENTION: Use htons() for setting the server port
// Return: TRUE on success, FALSE on error or if already running

BOOL CHttpServer::Run (const struct sockaddr_in &addrBindLocal)
  {
  BOOL bSuccess=FALSE;
  if(!m_bRunning)
    {
    memset(&m_addrBindLocal,0,sizeof(m_addrBindLocal));
    m_addrBindLocal.sin_port=addrBindLocal.sin_port;
    m_addrBindLocal.sin_addr=addrBindLocal.sin_addr;
    m_addrBindLocal.sin_family=AF_INET;
    ResetStatistics();
    m_hAcceptThread=CreateThread(NULL,0,&CHttpServer::AcceptThreadS,this,0,NULL); // start the accept thread
    ASSERT(m_hAcceptThread);
    if(m_hAcceptThread)
      {
      m_bRunning=TRUE;
      bSuccess=TRUE;
      }
    }
  return bSuccess;
  }



//--------------------------------------------------------------------------
// Stop()              stops server
// ------
// Input: -
// Return: TRUE on success, FALSE on error

BOOL CHttpServer::Stop (void)
  {
  if(!m_bRunning)
    return FALSE;

  BOOL bSuccess=TRUE;
  HANDLE hThreads[]={m_hAcceptThread};

  m_evShutdown.Set(); // let the threads shut down
  DWORD dwWaitResult=WaitForMultipleObjects(_countof(hThreads),hThreads,TRUE,THREADKILL_TIMEOUT_MS); // wait until all threads terminated

  if((dwWaitResult==WAIT_TIMEOUT) || (dwWaitResult==WAIT_FAILED))
    {
    LogMessage(_T("Failed to shutdown the accept client thread\n"));
    TerminateThread(m_hAcceptThread,0); // kill thread ungracefully
    bSuccess=FALSE;
    }
  CloseHandle(m_hAcceptThread); 

  for(int i=0;i<100;i++)
    {
    if(1)
      {
      CSingleLock lockSection(&m_statistics.m_csStatistics,TRUE);
      if(m_statistics.nClientsConnected==0) // wait until all connection threads are dead
        break;
      }
    Sleep(100);
    }

  if(m_statistics.nClientsConnected!=0) // if failed to shutdown a connection thread
    {
    LogMessage(_T("Failed to shutdown %d connection thread(s)\n"),m_statistics.nClientsConnected);
    }

  m_bRunning=FALSE;
  return bSuccess;
  }



//--------------------------------------------------------------------------
// ResetStatistics()       resets statistic information
// -----------------
// Input: -
// Return: 

void CHttpServer::ResetStatistics (void)
  {
  m_statistics.Reset();
  }



//--------------------------------------------------------------------------
// AcceptThreadS()         static thread wrapper
// ---------------
// Input: lpParameter = pointer to the http server (this pointer)
// Return: 0 on success, !=0 on any error

/*static*/ DWORD WINAPI CHttpServer::AcceptThreadS (_In_ LPVOID lpParameter)
  {
  CHttpServer *pThis=static_cast<CHttpServer *>(lpParameter);
  return pThis->AcceptThread();
  }


//--------------------------------------------------------------------------
// AcceptThread()          accept thread
// --------------
// Input: -
// Return: 0 on success, !=0 on any error

DWORD CHttpServer::AcceptThread (void)
  {
  struct CScopedSocket
    {
    SOCKET m_socket;
    CScopedSocket () {m_socket=INVALID_SOCKET;}
    ~CScopedSocket () {if(IsValid()) closesocket(m_socket);}
    BOOL IsValid (void) {return m_socket!=INVALID_SOCKET;}
    operator SOCKET() const {return m_socket;}
    };

  CScopedSocket sockListen; // main listening socket, waiting for requests

  sockListen.m_socket=WSASocket(AF_INET,SOCK_STREAM,0,NULL,0,WSA_FLAG_OVERLAPPED);
  if(sockListen == INVALID_SOCKET)
    {
    LogMessage(_T("Failed to create the listen socket\n"));
    return 1;
    }

  int nResult = ::bind(sockListen,(struct sockaddr *)&m_addrBindLocal,sizeof(m_addrBindLocal));
  if(nResult == SOCKET_ERROR)
    {
    LogMessage(_T("Failed to bind the listen socket\n"));
    return 1;
    }

  nResult=::listen(sockListen,SOMAXCONN);
  if(nResult==SOCKET_ERROR)
    {
    LogMessage(_T("Failed to listen with socket\n"));
    return 1;
    }	

  CWsaEvent evListen;
  nResult=WSAEventSelect(sockListen,evListen,FD_ACCEPT); // associate the event with the network accept-event
  if(nResult==SOCKET_ERROR)
    {
    LogMessage(_T("Failed to associate an event with the network accept-event\n"));
    return 1;
    }

  WSANETWORKEVENTS wsaEvents;
  WSAEVENT hListenAndShutdown[]={m_evShutdown,evListen}; // array to wait for a network event and the shutdown event simultanousely
  for(;;)
    {
    DWORD dwEventCause=WSAWaitForMultipleEvents(_countof(hListenAndShutdown),hListenAndShutdown,FALSE,WSA_INFINITE,FALSE); // block as long no requests and not shutdown
    if(dwEventCause == WAIT_FAILED || dwEventCause == WAIT_OBJECT_0) // if shutdown event fired
      {
      if(dwEventCause == WAIT_FAILED)
        LogMessage(_T("Event error in accept thread\n"));
      return 1;
      }
    nResult=WSAEnumNetworkEvents(sockListen,evListen,&wsaEvents); // which event was it?
    if(nResult==SOCKET_ERROR)						 
      {
      LogMessage(_T("Failed to enum network event\n"));
      return 1;
      }

    if(wsaEvents.lNetworkEvents == FD_ACCEPT)  // if a connection request came in
      {
      sockaddr_in sain;
      sockaddr saClient;
      int nAddrlen=sizeof(saClient);
      SOCKET sockClient=WSAAccept(sockListen,&saClient,&nAddrlen,NULL,NULL);
      memcpy(&sain,&saClient,nAddrlen);
      CString strClientAddr;
      strClientAddr.Format(_T("%d.%d.%d.%d"),sain.sin_addr.S_un.S_un_b.s_b1, sain.sin_addr.S_un.S_un_b.s_b2, sain.sin_addr.S_un.S_un_b.s_b3, sain.sin_addr.S_un.S_un_b.s_b4);
      if(sockClient==INVALID_SOCKET)
        LogMessage(_T("Failed to accept the connection\n"));        
      else
        {
        if(!AddConnection(sockClient,strClientAddr,sain.sin_port))
          LogMessage(_T("Failed to add a connection\n"));
        }
      }
    }
  return 0; // return with no error (when ending the thread on a shutdown request)
  }	



//--------------------------------------------------------------------------
// ConnectionThreadS()          thread for each connection
// -------------------
// Input: lpParameter = [in] pointer to the parameter block of type CONNECTION
//                      It must be created elsewhere with the new operator. It will be deleted after thread termination
// Return: 0 on success, !=0 on any error
// Remark: For one client there may be multiple threads (pipelining)

/*static*/ DWORD WINAPI CHttpServer::ConnectionThreadS (_In_ LPVOID lpParameter)
  {
  CONNECTION *pNewConn = static_cast<CONNECTION *>(lpParameter);
  CHttpServer *pServer=pNewConn->pServer;
  pServer->m_statistics.m_csStatistics.Lock();
  pServer->m_statistics.nClientsConnected++;
  pServer->m_statistics.m_csStatistics.Unlock();

  DWORD dwResult=pServer->ConnectionThread(pNewConn);

  // tidy-up
  // 13.Feb.15: shutdown(pNewConn->sockTcp,SD_BOTH); // on non-windows, call shutdown() before close(pNewConn->sockTcp). This is required for the client to get notified about the closure
  // see also Graceful Shutdown, Linger Options, and Socket Closure, https://msdn.microsoft.com/en-us/library/windows/desktop/ms738547(v=vs.85).aspx
  closesocket(pNewConn->sockTcp);
  delete pNewConn;

  pServer->m_statistics.m_csStatistics.Lock();
  pServer->m_statistics.nClientsConnected--;
  pServer->m_statistics.m_csStatistics.Unlock();

  return dwResult;
  }



//--------------------------------------------------------------------------
// ConnectionThread()          client thread running for each connection
// ------------------
// Input: pNewConn = [in] additional information for the new connection
// Return: 0 on success, !=0 on any error
// remarks: for each connection of a client a thread will be started.
//          This means that one client can have more than one of this 
//          thread when http pipelining is in action

DWORD CHttpServer::ConnectionThread (const CONNECTION *pNewConn)
  {
  struct CRxBuffer
    {
    WSABUF buffer;
    CRxBuffer (int nSize) {buffer.len=nSize; buffer.buf=new char[buffer.len];}
    ~CRxBuffer () {if(buffer.buf) delete buffer.buf;}
    operator char *() const {return buffer.buf;}
    operator WSABUF *() {return &buffer;}
    };

  SOCKET sockTcp=pNewConn->sockTcp;
  CRxBuffer bufRx(32768); // buffer for recieving data
  int nResult;

  int nSendAdvance=0; // starting point (character index within response) what to send of the response
  BOOL bKeepAlive=TRUE; // true as long the connection shall be maintained
  std::string strRequest; // buffer to cumulate the request
  std::vector<BYTE> vectResponse; // buffer to compose the raw response (headers and body) which will be sent back to the client

  CWsaEvent evSelect;

  if(!evSelect.IsValid())
    {
    ASSERT(FALSE);
    LogMessage(_T("Failed to create an WSA event\n"));
    return 0;
    }

  nResult=WSAEventSelect(sockTcp,evSelect,FD_READ|FD_WRITE|FD_CLOSE); // associate the TCP socket events with the WSA event
  if(nResult == SOCKET_ERROR)
    {
    LogMessage(_T("failed to associate the TCP socket events with the WSA event\n"));
    return 0;
    }

  WSAEVENT hEvSelectShutdown[]={evSelect,m_evShutdown}; // event array with socket event and shutdown event

  while (bKeepAlive || nSendAdvance>0) // loop as long keeping alive desired or if resend in progress
    {
    DWORD dwEventCaused = WSAWaitForMultipleEvents(_countof(hEvSelectShutdown),hEvSelectShutdown,FALSE,PERSISTENCE_TIMEOUT/*WSA_INFINITE*/,FALSE);
    if(dwEventCaused==WSA_WAIT_FAILED)
      {
      LogMessage(_T("Failure in WSAWaitForMultipleEvents() in the connection thread\n"));
      break;
      }
    else if(dwEventCaused==1) // if shutdown
      {
      LogMessage(_T("Closing connection since server is about to stop\n"));
      break;
      }
    else if(dwEventCaused==WSA_WAIT_TIMEOUT) // if timed-out
      {
      LogMessage(_T("Closing connection due to time-out\n"));
      break;
      }
    WSANETWORKEVENTS wsaEvents;
    nResult=WSAEnumNetworkEvents(sockTcp,evSelect,&wsaEvents); // which network event was it?
    if(nResult==SOCKET_ERROR)
      {
      LogMessage(_T("failed to determine the network event in the connection thread\n"));
      continue; 
      }

    // READ event
    if(wsaEvents.lNetworkEvents & FD_READ)
      {
      DWORD dwRecieved;
      DWORD dwFlags=0;
      nResult=WSARecv(sockTcp,bufRx,1,&dwRecieved,&dwFlags,NULL,NULL); // read data
      if(nResult!=SOCKET_ERROR)
        {
        m_statistics.IncRxCount(dwRecieved); // statistics
        strRequest+=std::string(bufRx,dwRecieved); // append rx data to the input buffer
        int nStartOfData=TestRequestCompleteness(strRequest); // pre-parse and test for completeness of the request
        if(nStartOfData>0) // if all data is recieved:
          {
          if(!ParseAndHandleRequest(strRequest,nStartOfData,pNewConn->strClientAddr,vectResponse,bKeepAlive)) // handle the request
            return 0;
          nSendAdvance=0; // start sending at the beginning of the response
          if(!Send(sockTcp,vectResponse,nSendAdvance)) // send the response
            {
            CString strRequestT=CA2T(strRequest.c_str());
            CString strResponseT=CA2T((LPCSTR)vectResponse.data());
            LogMessage(_T("Failed to send data back to client. Request:=%s, Response=%s\n"),(LPCTSTR)strRequestT,(LPCTSTR)strResponseT);
            bKeepAlive=FALSE; // when error occured, invoke close of connection
            }
          strRequest.clear(); // make ready for next request. It can be assumed that network events are issued on HTTP boundaries since the last TCP packet has always a push flag
          }
        }
      else
        LogMessage(_T("Failed to read data from client\n"));
      }

    // WRITE event (occurs when data could not be written-back to the client immediately)
    if((wsaEvents.lNetworkEvents&FD_WRITE) && nSendAdvance>0) // if a deferred write event (write request when in a re-send condition)
      {
      if(!Send(sockTcp,vectResponse,nSendAdvance))
        {
        LogMessage(_T("failed to send data\n"));
        bKeepAlive=FALSE;
        }
      strRequest.clear(); // make ready for next request
      }	

    // CLOSE event
    if(wsaEvents.lNetworkEvents & FD_CLOSE)  // socket close event is when client closed the connection
      {
      LogMessage(_T("Client has closed the connection\n"));
      break;
      }
    }

  return 0;
  }	


//--------------------------------------------------------------------------
// Send()                  sends content
// ------
// Input: sockSend = [in] socket to send data
//        vectSend = [in] data to be sent
// obsolete:       bResend = reference to return TRUE if data could not be sent yet and needs to be re-sent
//        nAdvance = [in/out] the starting byte index within the response data
//                   to be sent. 0 means that whole response shall be sent
//                   After sending, it indicates the starting point for the
//                   next send operation. returning 0 indicates that everything was sent
// Return: TRUE on success, FALSE on error

BOOL CHttpServer::Send (SOCKET sockSend, const std::vector<BYTE> &vectSend, int &nAdvance)
  {
  static const DWORD dwSendGranul=16384; // split send calls into these sizes
  const BYTE *pSend=vectSend.data()+nAdvance;
  DWORD dwRemaining=vectSend.size()-nAdvance;
  int nResult=0;
  while(dwRemaining)
    {
    WSABUF buffer;
    buffer.len=min(dwSendGranul,dwRemaining);
    buffer.buf=(char *)(const_cast<BYTE *>(pSend));
    DWORD dwSent=0;  
    nResult=WSASend(sockSend,&buffer,1,&dwSent,0,0,NULL);
    if(nResult==SOCKET_ERROR)
      break;
    ASSERT(dwSent<=dwRemaining); // more sent than available??
    dwRemaining-=dwSent;
    nAdvance+=dwSent;
    pSend+=dwSent;
    m_statistics.IncTxCount(dwSent); // maintain statistics
    }
  if(nResult==SOCKET_ERROR)
    {
    int nError=WSAGetLastError();
    if(nError==WSAEWOULDBLOCK) // if data can not be sent fast enough..
      {
      return TRUE; // .. do it later
      }
    else
      {
      nAdvance=0; // indicate that no more data should be sent
      return FALSE;
      }
    }
  ASSERT(dwRemaining==0);
  nAdvance=0; // all data could be sent
  return TRUE;
  }



//--------------------------------------------------------------------------
// TestRequestCompleteness()        tests whether the headers of the incoming request data is complete
// -------------------------
// Input: strResponse = [in] request string to be tested
// Return: >0: request is completely. value is the position of start of request data
//         ==0: request is not complete

/*static*/ int CHttpServer::TestRequestCompleteness (const std::string &strRequest)
  {
  size_t nColEndOfHeaders=strRequest.find("\r\n\r\n"); // search the end of headers mark
  if(nColEndOfHeaders==std::string::npos) 
    return -1; // end of headers not found

  // headers are complete, now find the content length header
  int nContentLen=0; // default 0 if no content length header is specified
  std::string strKey("Content-Length:");
  size_t nColContentLen=FindSubstringCi(strRequest,strKey); // search for the content-length header
  if(nColContentLen!=std::string::npos)
    {
    nColContentLen+=strKey.size();
    for(;nColContentLen<nColEndOfHeaders;nColContentLen++)
      {
      if(strRequest[nColContentLen]!=' ')
        {
        if(sscanf_s(strRequest.c_str()+nColContentLen,"%d\r\n",&nContentLen)!=1)
          nContentLen=0;
        break;
        }
      }
    }

  // test whether data length fullfills the content length
  size_t nColStartOfContent=nColEndOfHeaders+4; // skip "\r\n\r\n"
  if(strRequest.size()>=nColStartOfContent+nContentLen) // if the specified content length and the headers are recieved
    return nColStartOfContent; // .. return the start position of the request data (body)

  return 0; // not complete, please read more data
  }



//--------------------------------------------------------------------------
// ParseAndHandleRequest() parses and handles request
// -----------------------
// Input: strRequest = [in] raw request data, including headers
//                     typical request: "GET /filename.ext HTTP/1.1\r\n"
//        nStartOfData = [in] position of content data. if no content data, it points to the end of strRequest
//        strClientAddr = [in] address of the client in form of a string
//        vectResponse = [out] reference to return the response, including headers
//        bKeepAlive = [in] keep-alive state of the request headers
// Return: TRUE on success, FALSE on error

BOOL CHttpServer::ParseAndHandleRequest (const std::string &strRequest, int nStartOfData, const std::string &strClientAddr, std::vector<BYTE> &vectResponse, BOOL bKeepAlive)
  {
  BOOL bSuccess=TRUE;

  size_t nColEndOfMethod=strRequest.find(" "); // a valid request has a space
  if(nColEndOfMethod!=std::string::npos)
    {
    REQUEST reqData;
    reqData.strClientAddr=strClientAddr;
    reqData.strMethod=strRequest.substr(0,nColEndOfMethod);
    size_t nColEndOfUrl=strRequest.find(" ",nColEndOfMethod+1);
    if(nColEndOfUrl!=std::string::npos)
      {
      reqData.strUrl=strRequest.substr(nColEndOfMethod+1,nColEndOfUrl-nColEndOfMethod-1); // extract the url
      size_t nColQuery=reqData.strUrl.find("?"); // is there a query string?
      if(nColQuery!=std::string::npos)
        {
        reqData.strQuery=reqData.strUrl.substr(nColQuery+1); // extract the query string
        UnescapeString(reqData.strQuery,FALSE); // replace the %xx encoding to characters
        reqData.strUrl=reqData.strUrl.substr(0,nColQuery); // cut the url by the query string
        }
      UnescapeString(reqData.strUrl,TRUE);
      reqData.strData=strRequest.substr(nStartOfData); // the rest is the request data

      // decode the interesting headers
      size_t nColKeepAlive=strRequest.find("\r\nConnection: keep-alive",nColEndOfUrl);
      if(nColKeepAlive!=std::string::npos)
        bKeepAlive=TRUE;
      nColKeepAlive=strRequest.find("\r\nConnection: close",nColEndOfUrl);
      if(nColKeepAlive!=std::string::npos)
        bKeepAlive=FALSE;

      // handle the request
      RESPONSE responseData;
      responseData.nStatusCode=0; // mark as unhandled
      responseData.bKeepAlive=bKeepAlive;
      const URLOPTIONS *pUrlOptions=FindUrlOption(reqData.strUrl); // find specific options for the branch of the requested url
      if(pUrlOptions) // handle only if options are defined for the requested url
        {
        HandleUserRequest(reqData,responseData,pUrlOptions); // generic user-handler
        if(responseData.nStatusCode==0) // if not handled by the user handler..
          {
          bSuccess&=HandleFileRequest(reqData,responseData,pUrlOptions); // .. default file processing
          }
        }
      else // no options for the url defined..
        responseData.nStatusCode=404; // .. signal "resource not found"

      // compose the raw response
      char cHeaderBuf[1024];
      const char *szStatusText=GetHttpStatusText(responseData.nStatusCode);
      if(NULL==szStatusText)
        szStatusText="";
      int nHeaderLen=sprintf_s(cHeaderBuf,_countof(cHeaderBuf),"HTTP/1.1 %d %s\r\nContent-Length: %d\r\nConnection: %s\r\n%s\r\n",
        responseData.nStatusCode,
        szStatusText,
        (int)responseData.vectData.size(),
        responseData.bKeepAlive ? "keep-alive" : "close",
        responseData.strExtraHeaders.c_str()
        );
      // if desired: AddDateToHeader(strResponse);

      if(nHeaderLen>=0)
        {
        int nResponseLen=nHeaderLen+responseData.vectData.size();
        vectResponse.resize(nResponseLen);
        BYTE *pResponse=vectResponse.data();
        memcpy(pResponse+0,cHeaderBuf,nHeaderLen); // copy the header
        memcpy(pResponse+nHeaderLen,responseData.vectData.data(),responseData.vectData.size()); // append the response data
        }
      }
    }
  return bSuccess;
  }


//--------------------------------------------------------------------------
// UnescapeString()   unescapes a query or url string
// ---------------------
// Input: strConvert = [in, out] string to be converted
//        bIsUrl = [in] TRUE if strConvert is an url
//                      FALSE if strConvert is a query string
// Return: -

/*static*/ void CHttpServer::UnescapeString (std::string &strConvert, BOOL bIsUrl)
  {
  std::string strEscaped=strConvert; // make copy
  strConvert.clear();
  const char *szSource=strEscaped.c_str();
  int nRemaining=strEscaped.length();
  for(;*szSource;)
    {
    if((szSource[0]=='%') && (nRemaining>2))
      {
      int nLeft=szSource[1];
      int nRight=szSource[2];
      if((isxdigit(nLeft)) && (isxdigit(nRight))) // if the two characters followed by % are hexadecimal
        {
        nLeft=toupper(nLeft);
        if(nLeft<'A')
          nLeft-='0';
        else
          nLeft=nLeft-'A'+10;
        nRight=toupper(nRight);
        if(nRight<'A')
          nRight-='0';
        else
          nRight=nRight-'A'+10;
        int nCharCode=nLeft*16+nRight;
        strConvert.append(1,nCharCode);
        szSource+=2;
        nRemaining-=2;
        }
      } // if %
    else if((bIsUrl) && (szSource[0]=='+'))
      strConvert.append(1,' ');
    else
      strConvert.append(1,szSource[0]);
    szSource++;
    nRemaining--;
    }
  }


//--------------------------------------------------------------------------
// HandleUserRequest()         handles a request, calls user callback functions
// -------------------
// Input: rRequest = [in] request data
//        rResponse = [out] reference to return the response to be sent
//        pUrlOptions = [in] options specific to the requested url. Must not be NULL
// Return: -

void CHttpServer::HandleUserRequest (const REQUEST &rRequest, RESPONSE &rResponse, const URLOPTIONS *pUrlOptions) const
  {
  ASSERT(pUrlOptions); // if no options are specified for the requested url: shall be handled outside
  BOOL bHandled=FALSE;
  if(pUrlOptions->m_pfnHandler)
    pUrlOptions->m_pfnHandler(pUrlOptions->m_pObject,rRequest,rResponse,pUrlOptions->m_strLocalDir,pUrlOptions->m_nFlags); // call the user handler
  }



//--------------------------------------------------------------------------
// HandleFileRequest()     handles a file request
// -------------------
// Input: rRequest = [in] request data
//        rResponse = [out] response
//        pUrlOptions = [in] options specific to the requested url. Must not be NULL
// Return: TRUE on success, even if file could not be found
//         FALSE on fatal errors

BOOL CHttpServer::HandleFileRequest (const REQUEST &rRequest, RESPONSE &rResponse, const URLOPTIONS *pUrlOptions) const
  {
  ASSERT(pUrlOptions); // if no options are specified for the requested url: shall be handled outside
  if(pUrlOptions->m_strLocalDir.empty())
    {
    rResponse.nStatusCode=403; // forbidden (local directory not specified disables file access generally)
    return TRUE; // successfully rejected the request
    }
  BOOL bSuccess=FALSE;
  FILE *pFile=NULL;
  
  // make copy of file name and convert delimiter conventions
  std::string strPath=pUrlOptions->m_strLocalDir;
  ASSERT(strPath.size()>0); // you should have specified a non-empty local dir!
  if((rRequest.strMethod=="GET") && (rRequest.strUrl=="/")) // if requesting the default file
    strPath.append(pUrlOptions->m_strDefaultFile); // .. use the specified default file name
  else
    {
    size_t nCol=pUrlOptions->m_strUrl.size(); // skip the url already defined in the options and leave the path relative to the options url
    for(;nCol<rRequest.strUrl.size();nCol++)
      {
      char cCopy=rRequest.strUrl[nCol];
      if(cCopy=='/')
        cCopy='\\';
      strPath.append(1,cCopy);
      }
    }

  if((rRequest.strMethod=="GET") && (pUrlOptions->m_nFlags&URLO_FILE_ALLOW_READ))
    {
    fopen_s(&pFile,strPath.c_str(),"r+b");
    if(pFile)
      {
      std::fseek(pFile,0,SEEK_END); // get file size
      fpos_t nFileLen;
      std::fgetpos(pFile,&nFileLen);
      std::fseek(pFile,0,SEEK_SET); // rewind
      rResponse.vectData.resize((int)nFileLen);
      int nReadLen=std::fread(rResponse.vectData.data(),1,(size_t)nFileLen,pFile); // read file
      fclose(pFile);
      rResponse.nStatusCode=200;

      const char *pszCss=".css";
      if(!_stricmp(strPath.c_str()+strPath.length()-strlen(pszCss),pszCss))
        rResponse.strExtraHeaders="Content-Type: text/css\r\n";
      bSuccess=TRUE;
      }
    else
      {
      rResponse.nStatusCode=404;
      }
    }

  else if((rRequest.strMethod=="PUT") && (pUrlOptions->m_nFlags&URLO_FILE_ALLOW_WRITE))
    {
    // make the sub-directories, if needed
    int nColMkDir;
    int nPathLen=strPath.size();
    for(nColMkDir=0;nColMkDir<nPathLen;)
      {
      nColMkDir=strPath.find('\\',nColMkDir);
      if(nColMkDir==std::string::npos)
        break;
      std::string strMkDir=strPath.substr(0,nColMkDir);
      nColMkDir++;
      _mkdir(strMkDir.c_str());
      }

    fopen_s(&pFile,strPath.c_str(),"wb");
    if(pFile)
      {
      int nWritten=std::fwrite(rRequest.strData.c_str(),rRequest.strData.size(),1,pFile);
      if(nWritten==1)
        {
        rResponse.nStatusCode=201;
        bSuccess=TRUE;
        }
      else
        rResponse.nStatusCode=500;
      fclose(pFile);
      }
    else
      rResponse.nStatusCode=406;
    }

  else if(rRequest.strMethod=="POST")
    {
    rResponse.nStatusCode=501;
    }

  else if((rRequest.strMethod=="DELETE") && (pUrlOptions->m_nFlags&URLO_FILE_ALLOW_DELETE))
    {
    rResponse.nStatusCode=501;
    ASSERT(FALSE); // todo
    }

  if(!rResponse.nStatusCode)
    {
    rResponse.nStatusCode=403; // forbidden (no method with appropriate option flag matched)
    bSuccess=TRUE;
    }

  OutputDebugStringA(strPath.c_str());
  OutputDebugStringA(bSuccess ? " ->OK\n":" ->Failed\n");

  return TRUE; // bSuccess;
  }




//--------------------------------------------------------------------------
// GetHttpStatusText()     gets string for status code
// -------------------
// Input: nStatusCode = [in] code, e.g. 200
// Return: status text, e.g. "OK"
//         NULL if it could not be resolved

const char *CHttpServer::GetHttpStatusText (int nStatusCode)
  {
  const char *pResult=NULL;
  STATUSCODES::iterator itStatus=m_mapStatusText.find(nStatusCode);
  if(itStatus!=m_mapStatusText.end())
    pResult=itStatus->second;
  return pResult;
  }



//--------------------------------------------------------------------------
// AddDateToHeader()       appends date header
// -----------------
// Input: strTarget = [in] string to append the date header
// Return: -

/*static*/ void CHttpServer::AddDateToHeader (std::string &strTarget)
  {
  char szDateAndTime[128];
  struct tm *pTime=NULL;
  time_t ltime;
  time(&ltime);
  gmtime_s(pTime,&ltime);
  strftime(szDateAndTime,_countof(szDateAndTime),"%a, %d %b %Y %H:%M:%S GMT",pTime);
  strTarget+="Date: ";
  strTarget+=szDateAndTime;
  strTarget+="\r\n";
  }


//--------------------------------------------------------------------------
// OnNewConnection()       called when new connection established
// -----------------
// Input: strAddress = address of client
//        nPort = port of client
// Return: 

/*virtual*/ void CHttpServer::OnNewConnection (const CString &strAddress, int nPort)
  {
  // in derived classes, add specific code then call the base class implementation
  LogMessage(_T("Connection established with client at address %s.\n"),(LPCTSTR)strAddress);
  }



//--------------------------------------------------------------------------
// AddUrlOption()          adds options for treatment of an url.
// --------------
// Input: pszUrl = [in] url the options shall be set for
//                 if multiple overlapping options are set, the one with
//                 deeper detail depth (the longer path) will preceed the more shallow one
//        pObject = [in] object pointer which will be passed to the handler
//        pfnHandler = [in] handler to be called, NULL if no callback desired
//        pszLocalDir = [in] directory to be used for files on the server side.
//                      if NULL or empty string, file handling is generally disabled
//        pszDefaultFile = [in] file name of the default file, e.g. "index.html"
//        nFlags = [in] any combination of flags of URLOPTIONS_FLAGS
// Return: TRUE on success. Overwriting an existing item is treated as success.
//         FALSE on error
// Remarks: shall not be called when server is running since no thread-saveness
//          is provided for this map!

BOOL CHttpServer::AddUrlOption (LPCTSTR pszUrl, void *pObject, PFN_HANDLER pfnHandler, LPCTSTR pszLocalDir, LPCTSTR pszDefaultFile/*=NULL*/, URLOPTIONS_FLAGS nFlags/*=URLO_DEFAULT*/)
  {
  ASSERT(!m_bRunning); // you must not add options while the server is running since the options map is not implemented thread save
  if(pszLocalDir==NULL)
    pszLocalDir=_T("");
  URLOPTIONS optUrl;
  std::string strUrlA=CT2A(pszUrl,CP_UTF8);
  if(strUrlA.empty())
    return FALSE; // no empty url allowed
  if(strUrlA[strUrlA.size()-1]!='/') // if not ending with a delimiter, add it
    strUrlA+="/";

  URLOPTIONSMAP::iterator itSearch=m_mapUrlOptions.find(strUrlA);
  if(itSearch!=m_mapUrlOptions.end()) // if already defined
    m_mapUrlOptions.erase(itSearch); // .. remove setting

  if(!pszDefaultFile) // if no default file specified
    pszDefaultFile=_T("index.html"); // .. use this default
  std::string strLocalDirA=CT2A(pszLocalDir,CP_UTF8);
  if((!strLocalDirA.empty() && (strLocalDirA[strLocalDirA.size()-1])!='\\')) // if not ending with a delimiter, add it
    strLocalDirA+="\\";

  // build the options structure and insert it into the options map
  optUrl.m_strUrl=strUrlA;
  optUrl.m_pObject=pObject;
  optUrl.m_pfnHandler=pfnHandler;
  optUrl.m_strLocalDir=strLocalDirA;
  optUrl.m_strDefaultFile=CT2A(pszDefaultFile);
  optUrl.m_nFlags=nFlags;
  m_mapUrlOptions.insert(std::pair<std::string,URLOPTIONS>(strUrlA,optUrl));

  return TRUE;
  }




//--------------------------------------------------------------------------
// FindUrlOption()         returns options defined for an url and its sub-items
// ---------------
// Input: strUrl = [in] url for which the options shall be searched
// Return: options defined for an url and its sub-items
//         NULL if no options are defined
// Remarks: if options are defined for an item and options are also defined
//          for a sub-item of the first one, the options of the sub-item are returned

const CHttpServer::URLOPTIONS *CHttpServer::FindUrlOption (const std::string &strUrl) const
  {
  if(m_mapUrlOptions.empty())
    return NULL; // empty map: return no options
  URLOPTIONSMAP::const_iterator itSearch;
  itSearch=m_mapUrlOptions.lower_bound(strUrl); // find the point alphabetically after where the url would be in the map
  if(itSearch==m_mapUrlOptions.end()) // if the requesting url would be after the last element..
    --itSearch; // start the search one position before

  for(;;)
    {
    const URLOPTIONS *pTemp=&itSearch->second;
    unsigned int nOptionsUrlLen=pTemp->m_strUrl.size();
    if(nOptionsUrlLen<=strUrl.size()) // if options url go not more in detail than requested one, they may be suitable
      {
      if(!strUrl.compare(0,nOptionsUrlLen,pTemp->m_strUrl,0,nOptionsUrlLen)) // if url left part of url equal, options match
        {
        return pTemp; // // left part of url matching, so return the suitable options
        }
      }
    if(itSearch==m_mapUrlOptions.begin()) // are we done?
      break;
    --itSearch; // one position to begin of list
    }
  return NULL; 
  }




//--------------------------------------------------------------------------
// FindSubstringCi()       case insensitive search of a string
// -----------------
// Input: strSearchIn = string to search in
//        strSearchFor = string to be searched for in strSearchIn
// Return: index of first occurence, npos if not found

bool CiPredicate (char i, char j) {return toupper(i)==toupper(j);}

/*static*/ size_t CHttpServer::FindSubstringCi (const std::string &strSearchIn, const std::string &strSearchFor)
  {
  std::string::const_iterator it=std::search(strSearchIn.begin(),strSearchIn.end(),strSearchFor.begin(), strSearchFor.end(), CiPredicate);
  if(it!=strSearchIn.end())
    return it-strSearchIn.begin();
  else return std::string::npos; // not found
  }



//--------------------------------------------------------------------------
// LogMessage()            outputs a logging message
// ------------
// Input: szFormat = format string;
//        ... = more arguments
// Return: 

void CHttpServer::LogMessage (LPCTSTR szFormat, ...)
  {
  va_list argptr;
  va_start(argptr,szFormat);
  TCHAR tcOut[512];
  _vsntprintf_s(tcOut,_countof(tcOut),_TRUNCATE,szFormat,argptr);
  OnLogMessage(tcOut);
  }


//--------------------------------------------------------------------------
// OnLogMessage()          called whenever a log entity shall be made. can be in any (!) thread context
// --------------
// Input: szLogInfo = log info string
// Return: 

/*virtual*/ void CHttpServer::OnLogMessage (LPCTSTR szLogInfo)
  {
  // in derived classes, add specific code and then call the base class implementation
  OutputDebugString(szLogInfo);
  }




