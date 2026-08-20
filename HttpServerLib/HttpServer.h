// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------------------------
//  _____  __   __  _____  _    _        ____              
// |  ___||  \ /  ||  _  \| |  | |      / ___|  _   _  ___ 
// |  __| |   V   ||  -  /| |  | |  __  \___ \ | | | |/ __|
// | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
// |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
//                                              |___/      
// Filename:    HttpServer.h
// Created:     2014-01-31 (19:17)
// Author:      D. Burger
// Description: http server
//------------------------------------------------------------------------------------------------


#pragma once


#include <string>
#include <map>
#include <vector>
#include <afxmt.h>
#include <winsock2.h>




class CHttpServer
  {
  public:
    struct STATISTICS
      {
      CCriticalSection m_csStatistics; // critiacl section protecting the statistics
      volatile unsigned int nClientsConnected; // number of currently connected clients
      long long llTotalSent; // total bytes sent
      long long llTotalRecv; // total bytes recieved
      STATISTICS () {Reset();}
      void Reset (void) {m_csStatistics.Lock(); nClientsConnected=0; llTotalSent=0; llTotalRecv=0; m_csStatistics.Unlock();}
      void IncRxCount (unsigned int nInc) {m_csStatistics.Lock(); llTotalRecv+=nInc; m_csStatistics.Unlock();}
      void IncTxCount (unsigned int nInc) {m_csStatistics.Lock(); llTotalSent+=nInc; m_csStatistics.Unlock();}
      };

    struct REQUEST
      {
      std::string strMethod; // method, one of "GET", "PUT", "POST", "DELETE"
      std::string strUrl; // requested url, without any query string
      std::string strQuery; // query string in an unescaped form
      std::string strClientAddr; // address of the requesting client
      std::string strData; // body of the request
      };

    struct RESPONSE
      {
      std::string strExtraHeaders; // user defined headers to be sent. each entity must be followed by exactly one(!) \r\n sequence
      std::vector<BYTE> vectData; // response data
      BOOL bKeepAlive; // TRUE for keep-alive semantics. Will be pre-set according to the headers in the request, can be pitched by the handler
      int nStatusCode; // http status code, must be set by the handler. if left 0, the handler indicates that it has not handled the request and default file handling shall be done
      };

    enum URLOPTIONS_FLAGS : int
      {
      URLO_DEFAULT=0,
      URLO_FILE_ALLOW_READ    =0x0001, // allow file read operations
      URLO_FILE_ALLOW_WRITE   =0x0002, // allow file write operations
      URLO_FILE_ALLOW_CREATE  =0x0004, // allow files to be created
      URLO_FILE_ALLOW_DELETE  =0x0008, // allow files to be deleted
      };
  
    typedef void (*PFN_HANDLER)(void *pObject, const REQUEST &rRequest, RESPONSE &rResponse, const std::string &strLocalDir, URLOPTIONS_FLAGS nUrlOptionFlags); // callback for a http-handler
      // the status code of the response shall be set to non-zero to indicate that the response was handled

  private:
    struct CONNECTION
      {
      CHttpServer *pServer; // server belonging to the connection, used to feed into a thread
      SOCKET sockTcp; // socket for TCP data traffic of the connection
      std::string strClientAddr; // address of the client
      };

    struct CWsaEvent // class allowing to reliably close the event when returning from a function
      {
      WSAEVENT m_hEvent;
      CWsaEvent() {m_hEvent=WSACreateEvent();}
      ~CWsaEvent() {if(IsValid()) WSACloseEvent(m_hEvent);}
      BOOL IsValid (void) {return m_hEvent!=WSA_INVALID_EVENT;} 
      operator WSAEVENT() const {return m_hEvent;}
      void Set (void) {SetEvent(m_hEvent);}
      void Reset (void) {ResetEvent(m_hEvent);}
      };

    struct URLOPTIONS // options for url specific handling
      {
      std::string m_strUrl; // url or directory as seen by the client
      void *m_pObject; // object pointer passed to the handler
      PFN_HANDLER m_pfnHandler; // handler called when request to strUrl or a sub-item came in
      std::string m_strLocalDir; // local directory for file accesses
      std::string m_strDefaultFile; // name of the default file, e.g. "index.html"
      URLOPTIONS_FLAGS m_nFlags; // option flags
      URLOPTIONS () {m_pObject=NULL; m_pfnHandler=NULL; m_nFlags=URLO_DEFAULT;}
      };

    typedef std::map<std::string,std::string> MIMETYPES;
    typedef std::map<int,const char *> STATUSCODES;
    typedef std::map<std::string,URLOPTIONS> URLOPTIONSMAP;

  // data members
  private:
    HANDLE m_hAcceptThread; // handle of accept Thread
    CWsaEvent m_evShutdown; // event to shutdown the threads (when set, the accept and connection thread will terminate)
    STATISTICS m_statistics; // statistic information
    struct sockaddr_in m_addrBindLocal; // descrives where the listen socket shall be bound to (which network adapter to be used)
    BOOL m_bRunning; // TRUE indicates that server is running
    STATUSCODES m_mapStatusText; // map holding the status texts
    URLOPTIONSMAP m_mapUrlOptions; // map with options for various urls

  // construction/destruction
  public:
    CHttpServer ();
    virtual	~CHttpServer ();

  // attributes and methods
  public:
    BOOL AddUrlOption (LPCTSTR pszUrl, void *pObject, PFN_HANDLER pfnHandler, LPCTSTR pszLocalDir, LPCTSTR pszDefaultFile=NULL, URLOPTIONS_FLAGS nFlags=URLO_DEFAULT); // adds options for treatment of an url
    void GetStatistics (STATISTICS &statReturn); // returns statistics
    void ResetStatistics (void); // resets statistic information
    BOOL Run (const struct sockaddr_in &addrBindLocal); // starts server
    BOOL Stop (void); // stops server

  // implementation
  private:
    static DWORD WINAPI AcceptThreadS (_In_  LPVOID lpParameter);
    DWORD AcceptThread (void); // accept thread
    static DWORD WINAPI ConnectionThreadS (_In_  LPVOID lpParameter);
    DWORD ConnectionThread (const CONNECTION *pNewConn); // client thread running for each connection
    BOOL AddConnection (SOCKET sockConn, const CString &strClientAddr, int nPort); // adds a client to the visitors list and starts a client thread
    static int TestRequestCompleteness (const std::string &strResponse); // tests whether the incoming response data is complete
    BOOL ParseAndHandleRequest (const std::string &strRequest, int nStartOfData, const std::string &strClientAddr, std::vector<BYTE> &vectResponse, BOOL bKeepAlive); // parses and handles request
    static void UnescapeString (std::string &strConvert, BOOL bIsUrl); // unescapes a query/url string
    BOOL HandleFileRequest (const REQUEST &rRequest, RESPONSE &rResponse, const URLOPTIONS *pUrlOptions) const; // handles a file request
    const char *GetHttpStatusText (int nStatusCode); // gets string for status code
    static void AddDateToHeader (std::string &strTarget); // appends date header
    BOOL Send (SOCKET sockSend, const std::vector<BYTE> &vectSend, int &nAdvance); // sends content
    const URLOPTIONS *FindUrlOption (const std::string &strUrl) const; // returns options defined for an url and its sub-items
    void HandleUserRequest (const REQUEST &rRequest, RESPONSE &rResponse, const URLOPTIONS *pUrlOptions) const; // handles a request
    static size_t FindSubstringCi (const std::string &strSearchIn, const std::string &strSearchFor); // case insensitive search of a string
    void LogMessage (LPCTSTR szFormat, ...); // outputs a logging message

  // overrideables
  protected:
    virtual void OnNewConnection (const CString &strAddress, int nPort); // called when new connection established
    virtual void OnLogMessage (LPCTSTR szLogInfo); // called whenever a log entity shall be made. can be in any (!) thread context

  };



