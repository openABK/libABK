//------------------------------------------------------------------------------------------------
//  _____  __   __  _____  _    _        ____              
// |  ___||  \ /  ||  _  \| |  | |      / ___|  _   _  ___ 
// |  __| |   V   ||  -  /| |  | |  __  \___ \ | | | |/ __|
// | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
// |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
//                                              |___/      
// Filename:    DiscoveryServer.cpp
// Created:     2015-02-21 (15:30)
// Author:      D. Burger
// Description: Discovery-Server, based on an UDP listening and answering strategy
//------------------------------------------------------------------------------------------------


#include "stdafx.h"

#include "ValuesFromSpec.h"
#include "DiscoveryServer.h"
#include "JsonParser.h"   // used to decode the requests
#include "JsonFormatter.h"  // used to format the answer
#include "CrossPlatform.h"

#include <assert.h>

#include "LoggerIf.h"

using namespace std;

namespace Abk
{





//--------------------------------------------------------------------------
// CDiscoveryServerPool()  Constructor of CDiscoveryServerPool
// ----------------------
// Input: -
// Return: 

CDiscoveryServerPool::CDiscoveryServerPool ()
  {
  
  }


//--------------------------------------------------------------------------
// ~CDiscoveryServerPool() Destructor of CDiscoveryServerPool
// -----------------------
// Input: -
// Return: 

/*virtual*/ CDiscoveryServerPool::~CDiscoveryServerPool ()
  {
  RemoveAndDeleteAll(); // tidy-up, wait until all services stopped
  }


//--------------------------------------------------------------------------
// AddAndStartServer()      adds and starts a discovery service
// -------------------
// Input: pAdd = pointer to a discovery server, must be located at the heap
//               Do not delete this instance elsewhere, delete it with RemoveAndDeleteServer()
// Return: true if successfully inserted
//         false if discovery server is already inserted

bool CDiscoveryServerPool::AddAndStartServer (CDiscoveryServer *pAdd)
  {
  assert(pAdd); // you have to specify a valid discovery server!!
  CAbkSingleLock guard(&m_mutexPool,true);
  std::pair<std::set<CDiscoveryServer *>::iterator,bool>itResult= m_setPool.insert(pAdd);
  pAdd->Start();
  return itResult.second;
  }


//--------------------------------------------------------------------------
// RemoveAndDeleteServer() removes a discovery service and then deletes it
// -----------------------
// Input: pRemove = pointer to discovery server to be removed
// Return: true if the specified discovery server was found and told to shut-down
//         false if the specified discovery server was not found in the pool

bool CDiscoveryServerPool::RemoveAndDeleteServer (CDiscoveryServer *pRemove)
  {
  assert(pRemove); // you have to specify a valid discovery server!!
  
  // check whether pRemoveis valid
  CAbkSingleLock guardPool(&m_mutexPool,true);
  std::set<CDiscoveryServer *>::iterator itFind=m_setPool.find(pRemove);
  if(itFind==m_setPool.end())
    return false;

  // initiate deletion
  CAbkSingleLock guardKill(&m_mutexKill,true); // ensure atomic running state read-modify-write.
  if(pRemove->IsRunning()) // why does the thread is stopped. Deferred removal is not supported when thread gets stopped elsewhere
    pRemove->StopAndDeleteDeferred(this); // stop the service. As soon as it has stopped, it shall unregister and delete itself
  else
    UnregisterAndDelete(pRemove); // service is stopped: unregister it immediately

  return true;
  }


//--------------------------------------------------------------------------
// RemoveAndDeleteAll()    removes and deletes all discovery servers, blocks until all done
// --------------------
// Input: -
// Return: 

bool CDiscoveryServerPool::RemoveAndDeleteAll (void)
  {
  // stop all servers and tell them that they shall delete
  if(1)
    {
    CAbkSingleLock guardPool(&m_mutexPool,true);
    for(std::set<CDiscoveryServer *>::iterator itServer=m_setPool.begin();itServer!=m_setPool.end();++itServer)
      RemoveAndDeleteServer(*itServer); // stop and initiate deferred deletion. this will not block
    }

  // wait for completion
  for(;;)
    {
    if(GetCount()==0)
      break;
    AbkSleepMs(100);
    }

  return true;
  }


//--------------------------------------------------------------------------
// GetCount()              returns number of currently running services
// ----------
// Input: -
// Return: number of currently running services

int CDiscoveryServerPool::GetCount (void) const
  {
  CAbkSingleLock guard(&m_mutexPool,true);
  return m_setPool.size();
  }


//--------------------------------------------------------------------------
// UnregisterAndDelete()   unregisters and deletes a discovery server
// ---------------------
// Input: pKill = pointer to discovery server to be unregistered and deleted
// Return: true on success
//         false on error

bool CDiscoveryServerPool::UnregisterAndDelete (CDiscoveryServer *pKill)
  {
  assert(pKill); // you have to specify a valid discovery server!!
  CAbkSingleLock guard(&m_mutexPool,true);
  if(m_setPool.erase(pKill)==0)
    return false; // none removed from the list
  assert(!pKill->IsRunning()); // why does the thread still runs when unregistering??
  delete pKill;
  return true;
  }



















//--------------------------------------------------------------------------
// Clear()                 clears content
// -------
// Input: -
// Return: -

void CDiscoveryServer::SRequest::Clear (void)
  {
  m_strClientClass.clear();
  m_strClientType.clear();
  m_strClientSerial.clear();
  }


//--------------------------------------------------------------------------
// Decode()                decodes the raw request json formatted request into the members
// --------
// Input: pszReq = the raw request json formatted request into the members
// Return: true on successful dedoce, false on syntax error

bool CDiscoveryServer::SRequest::Decode (const char *pszReq)
  {
  assert(pszReq);
  Clear(); // clear any previousely generated content
  bool bSuccess=false;
  CJsonParser parsReq(pszReq);
  std::string strKey; // magic key we expect from the requesting device
  bool bClassDecoded=false;
  bool bTypeDecoded=false;
  bool bSerialDecoded=false;
  for(;!parsReq.IsDone();++parsReq)
    {
    parsReq.ExtractValue(ABK_DISCOVERY_REQ_KEYNAME,&strKey); // get the key of the request for qualification
    bClassDecoded|=parsReq.ExtractValue(ABK_DISCOVERY_REQ_CLASS,&m_strClientClass); // get information about requesting potential client
    bTypeDecoded|=parsReq.ExtractValue(ABK_DISCOVERY_REQ_TYPE,&m_strClientType);
    bSerialDecoded|=parsReq.ExtractValue(ABK_DISCOVERY_REQ_SERIAL,&m_strClientSerial);
    parsReq.SkipItem(); // discard any inadvertent content 
    }
  
  // check for validity
  if(bSerialDecoded && bTypeDecoded && bClassDecoded && (!strKey.compare(ABK_DISCOVERY_REQ_KEYVALUE))) // if all mandatory fields present and the key matches a valid request
    bSuccess=true;
  return bSuccess;
  }
















//--------------------------------------------------------------------------
// CDiscoveryServer()      Constructor of CDiscoveryServer
// ------------------
// Input: 
// Return: 

CDiscoveryServer::CDiscoveryServer ()
  {
  m_pOwningPool=NULL;
  m_hThread=ABK_INVALID_THREAD_HANDLE;
  }


 //--------------------------------------------------------------------------
 // ~CDiscoveryServer()     Destructor of CDiscoveryServer
 // -------------------
 // Input: -
 // Return: 

 /*virtual*/ CDiscoveryServer::~CDiscoveryServer ()
   {
   // if(IsRunning())
   //  Stop(); // this would be a virtual function call in the destructor
   assert(!IsRunning()); // please stop the service before destruction !!
   }


//--------------------------------------------------------------------------
// ThreadWrapperS()        static function wrapper for the thread
// ----------------
// Input: pArg = instance pointer to the discovery server
// Return: 0 if no errors occured
//         non-zero if error occured

/*static*/ unsigned int THREAD_CALLCONV CDiscoveryServer::ThreadWrapperS (void *pArg)
  {
  assert(pArg);
  CDiscoveryServer *pThis=static_cast<CDiscoveryServer *>(pArg);
  unsigned int nResult=pThis->Run();
  pThis->m_evDone.Set(); // signal that we are done
  pThis->m_hThread=ABK_INVALID_THREAD_HANDLE;

  // unregister myself
  if(pThis->m_pOwningPool)
    {
    pThis->m_pOwningPool->UnregisterAndDelete(pThis); // unregister me at the owning pool and delete pThis
    pThis=NULL; // pThis MUST NOT ACCESSED BELOW since it was deleted here. Any attempt would result in an access violation
    }

  return nResult;
  }




//--------------------------------------------------------------------------
// Run()                   udp answering thread
// -----
// Input: -
// Return: 0 if no errors occured
//         non-zero if error occured

unsigned int CDiscoveryServer::Run (void)
  {
  // m_pLoggerIf->AddLog(LOGSEVERITY_TRACE,"UDP answer thread started");

  SServerInfo serverInfo;
  OnGetStaticServerInfo(serverInfo); // get non dynamically changing information of the logger, used to bind the listening socket and to build the answers

  OnSocketBind(ABK_ENUM_PORT,serverInfo.m_strMyIpAddr.c_str()); // bind socket. socket can be bound to the ip address provided by the openABK logger interface (meaning the http server it works on)

  for(;;)
    {
    char cRxBuf[ABK_SERVERDISCOVER_MAXPAYLOAD+1];
    std::string strLocalIpOfRequest; // on this address the request was recieved
    bool bRequestRxed=OnSocketRxRequest(cRxBuf,sizeof(cRxBuf),strLocalIpOfRequest); // wait for answers
    if(!bRequestRxed) // we shall abort
      break;

    SRequest req;
    bool bValidRequest=req.Decode(cRxBuf);
    if(bValidRequest)
      {
      CJsonFormatter jfAnswer;
      bool bPreference=OnGetConnectionPreference(req.m_strClientClass.c_str(),req.m_strClientType.c_str(),req.m_strClientSerial.c_str()); // is the client preferred to coneect to me?
      FormatAnswer(jfAnswer,serverInfo,strLocalIpOfRequest,bPreference);

      // send the answer
      std::string strResponse=jfAnswer.GetStream()->str();
      bool bTxSuccess=OnSocketTxResponse(strResponse.c_str(),strResponse.length());
      }
    if(m_evSleep.Wait(1))
      break;
    }
  OnSocketTidyUp();  
  return 0;
  }


//--------------------------------------------------------------------------
// FormatAnswer()          formats the answer to json formatter
// --------------
// Input: jfTarget = [out] json formatter recieving the result. must be empty
//        serverInfo = [in] information needed to build the answer. m_strMyIpAddr is not used since it is the listening address and may also be "0.0.0.0" (INADDR_ANY)
//        strLocalIpOfRequest [in] = ip address for the response, saying the client where to put its requests to. This is typically the local address where a listening socket recieved a discovery request
//        bPreference = [in] = true if requesting device shall connect to me on ambiguity
// Return: -

void CDiscoveryServer::FormatAnswer (CJsonFormatter &jfTarget, const SServerInfo &serverInfo, const std::string &strLocalIpOfRequest, bool bPreference) const
  {
  // check validity in debug version
  assert(!serverInfo.m_strServerType.empty()); // you have not specified a server type. Another reason: you forgot to call Shutdown() before destructor gets called
  assert(!serverInfo.m_strServerName.empty()); // you have not specified a server name. Another reason: you forgot to call Shutdown() before destructor gets called
  assert(!serverInfo.m_strServerSerial.empty()); // you have not specified a server serial number or serial ID. Another reason: you forgot to call Shutdown() before destructor gets called

  // format the answer
  jfTarget.WriteValue(ABK_DISCOVERY_RSP_KEYNAME,ABK_DISCOVERY_RSP_KEYVALUE); // "Key": "AbkServer"
  jfTarget.WriteValue(ABK_DISCOVERY_RSP_ADDRESS,strLocalIpOfRequest); // e.g. "Address": "192.168.178.99" // 03.Oct.17, D. Burger: changed from serverInfo.m_strMyIpAddr to strLocalIpOfRequest
  jfTarget.WriteValue(ABK_DISCOVERY_RSP_PORT,serverInfo.m_nPortHttp); // e.g. "Port": 8080
  jfTarget.WriteValue(ABK_DISCOVERY_RSP_CLASS,serverInfo.m_strServerClass); // e.g. "Class": "Logger"
  jfTarget.WriteValue(ABK_DISCOVERY_RSP_TYPE,serverInfo.m_strServerType); // e.g. "Type": "MyTronicx_SuperLogger3000"
  jfTarget.WriteValue(ABK_DISCOVERY_RSP_SERIAL,serverInfo.m_strServerSerial); // e.g. "Serial": "0234"
  jfTarget.WriteValue(ABK_DISCOVERY_RSP_NAME,serverInfo.m_strServerName); // e.g. "Name": "Powertrain-Logger"
  jfTarget.WriteValue(ABK_DISCOVERY_RSP_PREFERRED,bPreference); // e.g. true
  if(!serverInfo.m_strDescUrl.empty()) // url with description of the server (optional, so we have to check for emptyness)
    jfTarget.WriteValue(ABK_DISCOVERY_RSP_DESCURL,serverInfo.m_strDescUrl); // e.g. "DescriptionUrl":"/index.html"

  // finalize
  jfTarget.Close();  
  }


//--------------------------------------------------------------------------
// Start()                 starts discovery service. must be callec explicitely, not done at consctruction
// -------
// Input: -
// Return: 

void CDiscoveryServer::Start (void)
  {
  m_evSleep.Reset(); // prevent thread from terminating immediately
  m_evDone.Reset();
  m_hThread=AbkStartThread(ThreadWrapperS,this);
  }


//--------------------------------------------------------------------------
// Stop()                 stops discovery service
// -------
// Input: -
// Return: 

void CDiscoveryServer::Stop (void)
  {
  m_evSleep.Set(); // stop from any waiting
  OnSocketShutdown(); // bring the socket out of its blocking state. If not supported, we will let the socket time-out
  m_evDone.Wait(TERMINATE_TIMEOUT_MS);  // wait for the thread to stop
  }


//--------------------------------------------------------------------------
// StopAndDeleteDeferred() stops and invokes a deferred unregistering and deletion
// -----------------------
// Input: pPoolUnregister = service pool where this unregisters itself
// Return: 

void CDiscoveryServer::StopAndDeleteDeferred (CDiscoveryServerPool *pPoolUnregister)
  {
  assert(pPoolUnregister); // you have to specify a pool where this will unregister!!
  assert(IsRunning()); // deferred un-registering a non-running service is not supported. Please do not stop the service elsewhere!!
  m_pOwningPool=pPoolUnregister;
  m_evSleep.Set(); // stop from any waiting
  OnSocketShutdown(); // bring the socket out of its blocking state. If not supported, we will let the socket time-out
  }


//--------------------------------------------------------------------------
// IsRunning()             returns true if service is running
// -----------
// Input: -
// Return: rue if service is running, false if stopped

bool CDiscoveryServer::IsRunning (void) const
  {
  return m_hThread!=ABK_INVALID_THREAD_HANDLE;  
  }








} // namespace Abk

