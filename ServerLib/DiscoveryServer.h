//------------------------------------------------------------------------------------------------
//  _____  __   __  _____  _    _        ____              
// |  ___||  \ /  ||  _  \| |  | |      / ___|  _   _  ___ 
// |  __| |   ´   ||  -  /| |  | |  __  \___ \ | | | |/ __|
// | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
// |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
//                                              |___/      
// Filename:    DiscoveryServer.h
// Created:     2015-02-21 (15:29)
// Author:      D. Burger
// Description: Discovery-Server, based on an UDP listening and answering strategy
//------------------------------------------------------------------------------------------------



#pragma once

#include "CrossPlatform.h"
#include <set>

class CJsonFormatter;


namespace Abk
  {

  class CDiscoveryServer;


  class CDiscoveryServerPool
    {
    friend class CDiscoveryServer;

    // data members
    protected:
      CAbkMutex m_mutexPool; // mutex protecting the pool
      CAbkMutex m_mutexKill; // mutex protecting the phase of deferred deletion 
      std::set<CDiscoveryServer *> m_setPool; // the pool of running discovery services

    // construction/destruction
    public:
      CDiscoveryServerPool (); // constructor
      virtual ~CDiscoveryServerPool (); // destructor

    // attrs and methods
    public:
      bool AddAndStartServer (CDiscoveryServer *pAdd); // adds a discovery service
      bool RemoveAndDeleteServer (CDiscoveryServer *pRemove); // removes a discovery service and then deletes it
      bool RemoveAndDeleteAll (void); // removes and deletes all discovery servers, blocks until all done
      int GetCount (void) const; // returns number of currently running services

    // implementation
    protected:
      bool UnregisterAndDelete (CDiscoveryServer *pKill); // unregisters and deletes a discovery server
    };

  
  class CDiscoveryServer
    {
    friend class CDiscoveryServerPool;
    protected:
    struct SRequest
      {
      std::string m_strClientClass; // class of the requesting potential client, e.g. "Display"
      std::string m_strClientType; // type of the requesting potential client, e.g. "MyTronicsSuperDisplay3000"
      std::string m_strClientSerial; // serial number of the requesting potential client "0012"

      SRequest () {Clear();}
      void Clear (void); // clears content
      bool Decode (const char *pszReq); // decodes the raw request json formatted request into the members
      };

    struct SServerInfo
      {
      std::string m_strMyIpAddr;      // ip address where the http and this discovery server runs. May be "0.0.0.0" (INADDR_ANY) if we shall listen to all adapters. transferred over openABK only for human readability. Note that this address will not be placed to the answer since it can represent more than one adapter!
      int m_nPortHttp;                // port number. This port number will be put to the answer, so client knows on which port to send its request to
      std::string m_strServerClass;   // class name of server, typically "Logger" (ABK_CLASSNAME_LOGGER)
      std::string m_strServerType;    // type of server, e.g. "MyTronicx_SuperLogger3000"
      std::string m_strServerName;    // name describing the role in the system e.g. "Powertrain-Logger"
      std::string m_strServerSerial;  // serial number/string of the logger
      std::string m_strDescUrl;       // url with description of the server (optional) , e.g. "/index.html"
      };

    protected:
      enum {TERMINATE_TIMEOUT_MS=10000,};

    // data members
    protected:
      CAbkEvent m_evSleep; // event for timing and terminating the thread
      CAbkEvent m_evDone; // signalled when thread has terminated
      ABK_THREAD_HANDLE m_hThread; // !=ABK_INVALID_THREAD_HANDLE as long as the thread is running
      CDiscoveryServerPool *m_pOwningPool; // pointer to a discovery-server-pool, NULL if standalone
    protected:
      //CLoggerInterface *m_pLoggerIf; // logger interface feed through to the virtual functions giving them the ability to make interface-specific decisions


    // construction/destruction
    public:
      CDiscoveryServer ();
      virtual ~CDiscoveryServer ();

    // attributes and methods
    public:
      void Start (void); // starts discovery service. must be called explicitely, not done at consctruction
      void Stop (void); // stops discovery service
      bool IsRunning (void) const; // returns true if service is running

    protected: // overrideables concerning the association with the logger interface
      virtual bool OnGetConnectionPreference (const char *pszClass, const char *pszType, const char *pszSerial) const=0; // shall return preference of a connection
      virtual void OnGetStaticServerInfo (SServerInfo &rServerInfoGet) const=0; // shall provide the static server information

    protected: // oerrideables concerning socket abstraction
      virtual bool OnSocketBind (int nListenPort, const char *pszHttpIpAddress)=0; // a socket can be bound
      virtual bool OnSocketRxRequest (char *pRxBuf, size_t nBufLen, std::string &strLocalIpOfRequest)=0; // recieve request
      virtual bool OnSocketTxResponse (const char *pTxBuf, size_t nContentLen)=0; // send response
      virtual bool OnSocketShutdown (void)=0; // shutdown the socket
      virtual bool OnSocketTidyUp (void)=0; // tidy-up the sockets resources

    // implementation
    protected:
      static unsigned int THREAD_CALLCONV ThreadWrapperS (void *pArg); // static function wrapper for the thread
      unsigned int Run (void); // udp answering thread
      void FormatAnswer (CJsonFormatter &jfTarget, const SServerInfo &serverInfo, const std::string &strLocalIpOfRequest, bool bPreference) const; // formats the answer to json formatter
      void StopAndDeleteDeferred (CDiscoveryServerPool *pPoolUnregister); // stops and invokes a deferred unregistering and deletion

    };
  
  
  }