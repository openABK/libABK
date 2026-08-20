//------------------------------------------------------------------------------------------------
// Author: D. Burger, Friedberg, Germany, <www.openABK.org>, <www.embu-sys.de>, <info@openABK.org>
//
// You are not allowed to remove this heading from the source code
// You are free to use this library under the terms of the
// Code Project Open Library, see <http://www.codeproject.com/info/cpol10.aspx>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//  _____  __   __  _____  _    _        ____              
// |  ___||  \ /  ||  _  \| |  | |      / ___|  _   _  ___ 
// |  __| |   V   ||  -  /| |  | |  __  \___ \ | | | |/ __|
// | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
// |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
//                                              |___/      
// Filename:    AbkLoggerIf.cpp
// Created:     2012-05-07 (08:55)
// Author:      D. Burger
// Description: Interface between HTTP and loggers internal data
//------------------------------------------------------------------------------------------------

#include "stdafx.h"
#include "ValuesFromSpec.h"
#include "LoggerIf.h"
#include "Session.h"
#include "JsonFormatter.h"
#include "JsonParser.h"
#include <assert.h>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <time.h>



#pragma warning(disable : 4996)


#define ABK_PROTOCOL_VERSION "1.0" // protocol version this implementation realizes
#define IF_VERSION "1.0" // version of this demo code implementation


using namespace std;

namespace Abk {



#define SID_TIMEOUT 1000 // mutex timeout for session access

#define VARLIST_TIMEOUT 1000 // mutex-timeout when locking the variable list

#define SHUTDOWN_TIMEOUT 5000 // timeout to shutdown the whole logger interface

#define MIME_TYPE_TEXT "text/plain"
#define MIME_TYPE_JSON "application/json"


enum HTTP_STATUSCODE
  {
  HTTP_STATUSCODE_OK                    =200,
  HTTP_STATUSCODE_BAD_REQUEST           =400,
  HTTP_STATUSCODE_INVALID_METHOD        =405,
  HTTP_STATUSCODE_NOT_IMPLEMENTED       =501,
  HTTP_STATUSCODE_INSUFFICIENT_STORAGE  =507,
  };







int CLoggerInterface::m_nNextSessionId=ABK_SESSION_ID_FIRST; // id of the next session

// function table holding handlers for url/method pairs. Must be in alphabetical order
CLoggerInterface::RESPONSETABLE CLoggerInterface::m_tblResponse[]=
  {
    {ABK_REQUESTURL_CLIENTADDRESS    , HTTP_METHOD_GET    , &CLoggerInterface::Handle_GetClientAddress},
    {ABK_REQUESTURL_CLIENTEVENT      , HTTP_METHOD_PUT    , &CLoggerInterface::Handle_PutClientEvent},
    {ABK_REQUESTURL_DAQLISTMEAS      , HTTP_METHOD_PUT    , &CLoggerInterface::Handle_PutDaqListMeas},
    {ABK_REQUESTURL_DAQLISTMAILBOX   , HTTP_METHOD_PUT    , &CLoggerInterface::Handle_PutDaqListMailbox},
    {ABK_REQUESTURL_DAQLISTMAM       , HTTP_METHOD_PUT    , &CLoggerInterface::Handle_PutDaqListMAM},
    {ABK_REQUESTURL_DAQLISTMEAS      , HTTP_METHOD_DELETE , &CLoggerInterface::Handle_DeleteDaqList},
    {ABK_REQUESTURL_DAQLISTMAILBOX   , HTTP_METHOD_DELETE , &CLoggerInterface::Handle_DeleteDaqList},
    {ABK_REQUESTURL_DAQLISTMAM       , HTTP_METHOD_DELETE , &CLoggerInterface::Handle_DeleteDaqList},
    {ABK_REQUESTURL_DAQTREND         , HTTP_METHOD_PUT    , &CLoggerInterface::Handle_PutDaqTrend},
    {ABK_REQUESTURL_DAQTREND         , HTTP_METHOD_DELETE , &CLoggerInterface::Handle_DeleteDaqList}, // same handler as for DAQ lists
    {ABK_REQUESTURL_SERVEREVENT      , HTTP_METHOD_GET    , &CLoggerInterface::Handle_EventPolling},
    {ABK_REQUESTURL_SESSIONID        , HTTP_METHOD_POST   , &CLoggerInterface::Handle_PostSession},
    {ABK_REQUESTURL_SESSIONID        , HTTP_METHOD_DELETE , &CLoggerInterface::Handle_DeleteSession},
    {ABK_REQUESTURL_VARLIST          , HTTP_METHOD_GET    , &CLoggerInterface::Handle_GetVarListMeas},
    {ABK_REQUESTURL_MAILBOXLIST      , HTTP_METHOD_GET    , &CLoggerInterface::Handle_GetVarListMailbox},
    {ABK_REQUESTURL_VARMETA          , HTTP_METHOD_POST   , &CLoggerInterface::Handle_PostVarMetaMeas},
    {ABK_REQUESTURL_MAILBOXMETA      , HTTP_METHOD_POST   , &CLoggerInterface::Handle_PostVarMetaMailbox},
    {ABK_REQUESTURL_INTERFACESTATS   , HTTP_METHOD_GET    , &CLoggerInterface::Handle_InterfaceStatistic},
    {ABK_REQUESTURL_SERVERINFO       , HTTP_METHOD_GET    , &CLoggerInterface::Handle_GetServerInfo},
    {ABK_REQUESTURL_CURRENTTIME      , HTTP_METHOD_GET    , &CLoggerInterface::Handle_GetCurrentTime},
    {ABK_REQUESTURL_STORAGEINFO      , HTTP_METHOD_GET    , &CLoggerInterface::Handle_GetStorageInfo},
    {ABK_REQUESTURL_VARVALUE         , HTTP_METHOD_POST   , &CLoggerInterface::Handle_PostVarValueMeas},
    {ABK_REQUESTURL_VARVALUE         , HTTP_METHOD_PUT    , &CLoggerInterface::Handle_PutVarValueMeas},
    {ABK_REQUESTURL_MAM              , HTTP_METHOD_POST   , &CLoggerInterface::Handle_PostMamValues},
    {ABK_REQUESTURL_MAILBOXVALUE     , HTTP_METHOD_POST   , &CLoggerInterface::Handle_PostVarValueMailbox},
    {ABK_REQUESTURL_MAILBOXVALUE     , HTTP_METHOD_PUT    , &CLoggerInterface::Handle_PutVarValueMailbox},
    {ABK_REQUESTURL_FIRMWARE         , HTTP_METHOD_POST   , &CLoggerInterface::Handle_PostFirmwareInfo},
    {ABK_REQUESTURL_CLIENTCONFIG_INFO, HTTP_METHOD_POST   , &CLoggerInterface::Handle_PostClientConfigInfo},
    {ABK_REQUESTURL_AUDIOREC_HEADER  , HTTP_METHOD_PUT    , &CLoggerInterface::Handle_PutAudioRecHeader},
    {ABK_REQUESTURL_AUDIOREC_DATA    , HTTP_METHOD_PUT    , &CLoggerInterface::Handle_PutAudioRecData},
    {ABK_REQUESTURL_AUDIOREC_FOOTER  , HTTP_METHOD_PUT    , &CLoggerInterface::Handle_PutAudioRecFooter},

  };
// #define RESPONSETABLE_ORDERED // set the macro if the response table is ordered. Otherwise the table will be ordered in each constructor call


//--------------------------------------------------------------------------
// CLoggerInterface()      Constructor of CLoggerInterface
// ------------------
// Input: nPortHttp = port for establishing an http connection. The http
//                    server must listen to this port
//        strAdditionalHeaders = headers for the answer, null-terminated string.
//                               Each entity must end with one time \r\n,
//                               even the last entity. NULL if none specified
// Return: -

CLoggerInterface::CLoggerInterface ()
  {
  int nEntities=sizeof(m_tblResponse)/sizeof(RESPONSETABLE); // number of entities in the response function table

  #if !defined(RESPONSETABLE_ORDERED)
    // sort the response table
    int i,j;
    for(i=0;i<nEntities;i++)
      {
      for(j=i+1;j<nEntities;j++)
        {
        int nCompareResult=strcmp(m_tblResponse[i].pszUri,m_tblResponse[j].pszUri);
        if((nCompareResult>0) || ((nCompareResult==0)&&(m_tblResponse[i].nMethod>m_tblResponse[j].nMethod)))
          { // swap
          RESPONSETABLE rspTemp;
          memcpy(&rspTemp,&m_tblResponse[j],sizeof(RESPONSETABLE));
          memcpy(&m_tblResponse[j],&m_tblResponse[i],sizeof(RESPONSETABLE));
          memcpy(&m_tblResponse[i],&rspTemp,sizeof(RESPONSETABLE));
          }
        }
      }
  #endif

  #if !defined(NDEBUG)
    // check ordering of the request/response table
    RESPONSETABLE *pEntity=m_tblResponse;
    int nEntity;
    for(nEntity=0;nEntity<nEntities-1;nEntity++)
      {
      int nCompareResult=strcmp(m_tblResponse[nEntity+1].pszUri,m_tblResponse[nEntity].pszUri);
      if(nCompareResult==0) // if equal urls found, compare the method
        {
        assert(m_tblResponse[nEntity+1].nMethod>m_tblResponse[nEntity].nMethod);
        }
      else
        {
        assert(nCompareResult>0); // the order of the list is incorrect
        }
      }
  #endif

  m_bFirstRequest=true;
  }


//--------------------------------------------------------------------------
// ~CLoggerInterface()     Destructor of CLoggerInterface
// -------------------
// Input: -
// Return: 

/*virtual*/ CLoggerInterface::~CLoggerInterface ()
  {
  }


//--------------------------------------------------------------------------
// Shutdown()              shuts down the logger interface
// ----------              the shutdown is needed since threads calling virtual
//                         functions have to be terminated before the destructor
//                         gets called (v-calls not allowed from within the
//                         destructor nor threads during destruction time)
// Input: -
// Return: true if shut-down successfully
//         false if an error occured

bool CLoggerInterface::Shutdown (void)
  {
  // delete all sessions
  if(!DeleteAllSessions())
    return false; // sessions could not be deleted
  
  // delete all variables
  ClearVars(&m_lstMeasVars);
  ClearVars(&m_lstMailboxVars);
  
  // wait for completion
  bool bSuccess=false;
  const int nTimeoutPollPeriod=10;
  AddLog(LOGSEVERITY_TRACE,"Loggerif: Waiting for threads to shutdown");
  for(int nTimeoutRemaining=SHUTDOWN_TIMEOUT;nTimeoutRemaining;nTimeoutRemaining-=nTimeoutPollPeriod)
    {
    AbkSleepMs(nTimeoutPollPeriod);
    CAbkSingleLock guard(&m_mutexSessionList,true);
    if(0==m_mapSessions.size()) // if all sessions are removed
      {
      bSuccess=true;
      break;
      }
    }
  return bSuccess;
  }


//--------------------------------------------------------------------------
// ClearVars()             clears variables and empties the list
// -----------
// Input: pList = pointer to list to be cleaned-up
// Return: -

/*static*/ void CLoggerInterface::ClearVars (std::list<CVarRef *> *pList)
  {
  std::list<CVarRef *>::iterator iterVars;
  for(iterVars=pList->begin();iterVars!=pList->end();iterVars++)
    delete *iterVars;
  pList->clear();
  }



//--------------------------------------------------------------------------
// GenerateSession()       generates a session ID
// --------------
// Input: uidClient = client characteristics. the m_nSessionID member will
//                    be ignored
// Return: a newly created session ID


int CLoggerInterface::GenerateSession (const CClientUid &uidClient)
  {
  CAbkSingleLock lock(&m_mutexSessionList,true,SID_TIMEOUT);
  int nSessionId=m_nNextSessionId;

  CClientUid uidTemp(uidClient); // make copy of client characteristics..
  uidTemp.m_nSession=nSessionId; // .. and set the new session id into it
  CSession *pSessionNew=new CSession(this,uidTemp); // create a new session ..
  m_mapSessions.insert(std::pair<int,CSession *>(nSessionId,pSessionNew)); // .. and insert it into the session map
  m_nNextSessionId++; // generate new session number
  return nSessionId;
  }


//--------------------------------------------------------------------------
// DeleteSession()         deletes a session
// ---------------
// Input: nIdDelete = id of session to be deleted
// Return: true on success, false on error

bool CLoggerInterface::DeleteSession (int nIdDelete)
  {
  CAbkSingleLock lock(&m_mutexSessionList,true,SID_TIMEOUT);

  CSession *pSession=GetSession(nIdDelete);
  if(!pSession)
    return false;
  return DeleteSession(pSession);
  }


//--------------------------------------------------------------------------
// DeleteSession()         deletes a session
// ---------------
// Input: pSession = session to be deleted
// Return: true on success, false on error

bool CLoggerInterface::DeleteSession (CSession *pSession)
  {
  CAbkSingleLock lock(&m_mutexSessionList,true,SID_TIMEOUT);
  assert(pSession);
  bool bCanSaveDelete=false;
  for(int nRetries=0;nRetries<20;nRetries++)
    {
    if(!pSession->m_bRequestInProgress)
      {
      bCanSaveDelete=true;
      break;
      }
    pSession->FireEvent(); // let a potentially running long polling request terminate
    AbkSleepMs(10);
    }
  if(!bCanSaveDelete)
    return false;
  delete pSession;
  return true;
  }


//--------------------------------------------------------------------------
// DeleteAllSessions()     deletes all sessions
// -------------------
// Input: -
// Return: true on success, false on error

bool CLoggerInterface::DeleteAllSessions (void)
  {
  AddLog(LOGSEVERITY_TRACE,"Deleting all sessions");
  CAbkSingleLock lock(&m_mutexSessionList,true,SID_TIMEOUT);
  map<int, CSession *>::iterator iterSession;
  while(!m_mapSessions.empty())
    {
    iterSession=m_mapSessions.begin();
    CSession *pSession=(*iterSession).second;
    assert(pSession);
    AddLog(LOGSEVERITY_TRACE,"Deleting sessinon %d",pSession->GetSessionId());
    if(!DeleteSession(pSession))
      return false;
    }
  return true;
  }


//--------------------------------------------------------------------------
// UnregisterSession()     unregisters a session in the list
// -------------------
// Input: pSession = session to be unregistered
// Return: true on success, false on error

bool CLoggerInterface::UnregisterSession (CSession *pSession)
  {
  CAbkSingleLock lock(&m_mutexSessionList,true,SID_TIMEOUT);

  map<int, CSession *>::iterator iterSession;
  iterSession=m_mapSessions.find(pSession->GetSessionId());
  if(iterSession==m_mapSessions.end()) // not found
    return false;
  assert((*iterSession).second==pSession);
  m_mapSessions.erase(iterSession); // remove from session map
  return true;
  }


//--------------------------------------------------------------------------
// GetSession()            retrurns pointer to a session and refreshes its lifetime
// ------------
// Input: nSessionId = id of session to retrieve
//        strQuery = query string from uri
//        rRequest = request data structure
// Return: pointer to a session
//         NULL on error (not found) or if the query strind did not specifa a session
// Remark: sessions must be in a locked state

CSession *CLoggerInterface::GetSession (int nSessionId) const
  {
  assert(m_mutexSessionList.IsLocked()); // the sessions mutex must be already locked
  map<int, CSession *>::const_iterator iterSession;
  iterSession=m_mapSessions.find(nSessionId);
  if(iterSession==m_mapSessions.end()) // not found
    return NULL;
  assert((*iterSession).first==nSessionId);
  CSession *pSession=(*iterSession).second;
  assert(pSession);
  pSession->Refresh();
  return pSession;
  }

CSession *CLoggerInterface::GetSession (const std::string &strQuery) const
  {
  assert(m_mutexSessionList.IsLocked()); // the sessions mutex must be already locked
  std::string strSessionId;
  int nSessionId;
  if(!DecodeUriValue(strQuery,ABK_QRY_SESSIONID,strSessionId))
    return NULL;
  if(sscanf(strSessionId.c_str(),"%d",&nSessionId)!=1) // get the session id
    return NULL;
  return GetSession(nSessionId);
  }

CSession *CLoggerInterface::GetSession (const HTTP_REQUEST &rRequest) const
  {
  return GetSession(*rRequest.pQueryString);
  }



//--------------------------------------------------------------------------
// EnumSessions()          lists all connected clients
// --------------
// Input: pReturn = pointer to list recieving the client properties
// Return: number of clients put into the list

int CLoggerInterface::EnumSessions (std::list <CLoggerInterface::CClientUid> *pReturn) const
  {
  pReturn->clear();
  CAbkSingleLock lock(&m_mutexSessionList,true,SID_TIMEOUT);

  map<int, CSession *>::const_iterator iterSession;

  for(iterSession=m_mapSessions.begin();iterSession!=m_mapSessions.end();iterSession++)
    {
    int nSessionId=iterSession->first;
    CSession *pSession=iterSession->second;
    CClientUid uidClient=pSession->GetClientUid();
    pReturn->push_back(uidClient); // put client identification to list
    }
  return pReturn->size();
  }

//--------------------------------------------------------------------------
// IdentifyClient()        a client shall identify itself
// ----------------
// Input: strAddress = address of the client
//        strClass = class of the client
//        strType = type of the client
//        strSerial = serial number of the client
//        pUid = pointer to object containing address, class, type and serial
//        strMessage = optional message to be shown on the client
// Return: true on success (event could be sent, idependent of reciept by client)
//

bool CLoggerInterface::IdentifyClient (const CClientUid &clientUid, const char *pszMessage/*=NULL*/)
  {
  CAbkSingleLock lock(&m_mutexSessionList,true,SID_TIMEOUT);
  CSession *pSession=GetSession(clientUid.m_nSession);
  if(pSession)
    {
    std::string strParam=pszMessage;
    time_t tmSend;
    time(&tmSend); // use current local time
    pSession->FireEventToClient(0,tmSend,ABK_SVREVENT_IDENTIFY,strParam,0,0);
    return true;
    }
  return false;

  }


//--------------------------------------------------------------------------
// LockSessions()          locks session map
// --------------
// Input: nTimeout = mutex lock timeout
// Return: 

bool CLoggerInterface::LockSessions (int nTimeout) const
  {
  return m_mutexSessionList.Lock(nTimeout);
  }




//--------------------------------------------------------------------------
// UnlockSessions()        unlocks the session map
// ----------------
// Input: -
// Return: 

bool CLoggerInterface::UnlockSessions (void) const
  {
  return m_mutexSessionList.Unlock();
  }




//--------------------------------------------------------------------------
// OnGetVariableList()     called to retrieve the available variable catalogue
// -----------------
// Input: pVarList = pointer to an empty variable list where variable reference objects
//                   can be added. The objects must not be deleted elsewhere. they are
//                   deleted by the logger interface class
// Return: true on success, false on error

//virtual bool CLoggerInterface::OnGetVariableList (std::list<CVarRef *> *pVarList) const
//  {
//  return true;
//  }


//--------------------------------------------------------------------------
// FindVariable()          searches a variable by its name
// --------------
// Input: rList = [in] list to search within
//        rStrName = [in] name of variable to search for
//        pszVarName = [in] name of variable to search for
// Return: pointer to variable referenec object, NULL if not found

CVarRef *CLoggerInterface::FindVariable (const std::list<CVarRef *> &rList, const std::string &rStrVarName) const
  {
  CAbkSingleLock lock(&m_mutexVarList,true,VARLIST_TIMEOUT);
  for(list<CVarRef *>::const_iterator iterVars=rList.begin();iterVars!=rList.end();iterVars++)
    {
    if(!rStrVarName.compare((*iterVars)->GetNameString()))
      return *iterVars;
    }
  return NULL;
  }

CVarRef *CLoggerInterface::FindVariable (const std::list<CVarRef *> &rList, const char *pszVarName) const
  {
  CAbkSingleLock lock(&m_mutexVarList,true,VARLIST_TIMEOUT);
  for(list<CVarRef *>::const_iterator iterVars=rList.begin();iterVars!=rList.end();iterVars++)
    {
    if(!(*iterVars)->GetNameString().compare(pszVarName))
      return *iterVars;
    }
  return NULL;
  }



//--------------------------------------------------------------------------
// HandleHttpRequest()     handles all implemented HTTP requests
// -------------------
// Input: rRequest = request with data, url, query string and method of the request
//        rResponse = reference to response to be filled out by this handler
//                    the status code member must be set !=0 in order to mark the request as handled
// Return: -

void CLoggerInterface::HandleHttpRequest (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  RESPONSETABLE *pEntity=m_tblResponse;
  int nEntities=sizeof(m_tblResponse)/sizeof(RESPONSETABLE); // number of entities in the response function table
  int nSearchRangeMin=0; // range of indices to be searched within
  int nSearchRangeMax=nEntities;
  int nEntity=0;
  PFN_HANDLEREQUEST pfnHandler=NULL;

  // do lazy initialisation
  if(1) // provide block so CAbkSingleLock will fall out-of-scope when leaving
    {
    CAbkSingleLock guard(&m_mutexFirstRequest,true); // get exclusive access to the m_bFirstRequest
    if(m_bFirstRequest) // if the first request ever made
      {
      m_bFirstRequest=false;
      OnGetVariableList(&m_lstMeasVars); // get the variable list from the logger
      OnGetMailboxList(&m_lstMailboxVars); // get the mailbox list too
      }
    }
  
  // check for special services
  int nServiceNameLen=strlen(ABK_SERVICE_FORMS); // length of a service name
  const char *pszUrl=rRequest.pUrl->c_str();
  if(!strncmp(pszUrl,ABK_SERVICE_FORMS,nServiceNameLen)) // forms service
    {
    const char *pszFormName=pszUrl+nServiceNameLen;
    if(pszFormName[0]=='/')
      {
      pszFormName++;
      if(rRequest.nHttpMethod==HTTP_METHOD_GET)
        Handle_GetForm(rRequest,rResponse,pszFormName);
      else if(rRequest.nHttpMethod==HTTP_METHOD_PUT)
        Handle_PutForm(rRequest,rResponse,pszFormName);
      return;
      }
    }

  // binary search of uri and method pair
  for(;;)
    {
    nEntity=(nSearchRangeMin+nSearchRangeMax)/2; // mid point of range to be searched within
    if(nEntity==nSearchRangeMax)
      {
      rResponse.Set(HTTP_STATUSCODE_NOT_IMPLEMENTED,"Not implemented");
      break; // the uri with method could not be found
      }
    int nCompareResult=strcmp(pszUrl,m_tblResponse[nEntity].pszUri);
    if(nCompareResult==0) // if url found, compare the method
      nCompareResult=rRequest.nHttpMethod-m_tblResponse[nEntity].nMethod;
    if(nCompareResult==0) // if found (uri and method matches)
      {
      pfnHandler=m_tblResponse[nEntity].pfnHandler;
      break;
      }
    else if(nCompareResult>0) // if strUri larger than that at the current table position
      nSearchRangeMin=nEntity+1; // .. next search at top of the current position
    else
      nSearchRangeMax=nEntity; // .. next search at bottom of the current position
    }
  if(pfnHandler)
    (this->*pfnHandler)(rRequest,rResponse); // call the specific handler
  }



//--------------------------------------------------------------------------
// DecodeHttpMethod()      decodes method string into enumerated value
// ------------------
// Input: pszMethod = method string e.g. "GET"
// Return: one of HTTP_METHOD_xxx

/*static*/ CLoggerInterface::HTTP_METHOD CLoggerInterface::DecodeHttpMethod (const char *pszMethod)
  {
  if(!strcmp(pszMethod,"GET"))
    return HTTP_METHOD_GET;
  if(!strcmp(pszMethod,"POST"))
    return HTTP_METHOD_POST;
  if(!strcmp(pszMethod,"PUT"))
    return HTTP_METHOD_PUT;
  if(!strcmp(pszMethod,"DELETE"))
    return HTTP_METHOD_DELETE;
  return HTTP_METHOD_INVALID;
  }



//--------------------------------------------------------------------------
// DecodeUriValue()        decode a variable value from the query string or post data
// ----------------
// Input: rRequest = [in] request information
//        rResponse = [out] response the http server shall send back
//        rStrDest = reference to string recieving the result
// Return: -

/*static*/ bool CLoggerInterface::DecodeUriValue (const std::string &strQuery, const char *pszKey, std::string &rStrDest)
  {
  size_t nColKey=strQuery.find(pszKey,0);
  if(nColKey==std::string::npos)
    return false;
  int nKeyLen=strlen(pszKey);

  if((nColKey==0) || (strQuery[nColKey-1]=='?')) // if preceeded by & or at first position
    {
    if(nColKey+nKeyLen<strQuery.size() && strQuery[nColKey+nKeyLen]=='=') // if followed by "="
      {
      size_t nColValStart=nColKey+nKeyLen+1;
      size_t nColValEnd=strQuery.find('?',nColValStart);
      if(nColValEnd!=std::string::npos)
        rStrDest=strQuery.substr(nColValStart,nColValEnd-nColValStart);
      else
        rStrDest=strQuery.substr(nColValStart);
      return true;
      }
    }
  return false;
  }


//--------------------------------------------------------------------------
// Handle_PostSession()      handles the session request
// --------------------
// Input: rRequest = [in] request information
//        rResponse = [out] response the http server shall send back
// Return: -

void CLoggerInterface::Handle_PostSession (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  bool bSuccess=true;
  std::stringstream strError;
  CJsonParser parseRequest(rRequest.pPostData->c_str());
  CClientUid uidClient;
  std::string strFwVersion; // firmware version of the client, empty if not specified
  std::string strHwVersion; // hardware version of the client, empty if not specified

  // get information of the client who is requesting the session
  for(;!parseRequest.IsDone();++parseRequest) // loop for root object members
    {
    parseRequest.ExtractValue(ABK_REQ_SESSIONID_CLASS,    &uidClient.m_strClass);
    parseRequest.ExtractValue(ABK_REQ_SESSIONID_TYPE,     &uidClient.m_strType);
    parseRequest.ExtractValue(ABK_REQ_SESSIONID_SERIAL,   &uidClient.m_strSerial);
    parseRequest.ExtractValue(ABK_REQ_SESSIONID_FWVERSION,&strFwVersion);
    parseRequest.ExtractValue(ABK_REQ_SESSIONID_HWVERSION,&strHwVersion);
    }
  if(uidClient.m_strClass.empty())
    {
    strError<<"No class of the client was specified";
    bSuccess=false;
    }
  if(uidClient.m_strType.empty())
    {
    strError<<"No client type was specified";
    bSuccess=false;
    }
  if(!bSuccess)
    {
    rResponse.Set(HTTP_STATUSCODE_BAD_REQUEST,strError);
    }
  else
    {
    int nSessionId=GenerateSession(uidClient); // generate a new session
    OnClientConnected(uidClient.m_strClass.c_str(),uidClient.m_strType.c_str(),uidClient.m_strSerial.c_str(),strFwVersion.c_str(),strHwVersion.c_str(),nSessionId); // notify the server about client connection
    m_mutexSessionList.Lock(SID_TIMEOUT);
    CSession *pSession=GetSession(nSessionId); // the newly created session
    m_mutexSessionList.Unlock();
    assert(pSession);
    CJsonFormatter jfResult;
    jfResult.WriteValue(ABK_RSP_SESSIONID_ID,nSessionId);
    rResponse.Set(jfResult);
    }
  }


//--------------------------------------------------------------------------
// Handle_GetVarListMeas() responds with list of measurement variables
// -----------------------
// Input: rRequest = [in] request information
//        rResponse = [out] response the http server shall send back
// Return: -

void CLoggerInterface::Handle_GetVarListMeas (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  AddLog(LOGSEVERITY_TRACE,"A client \"%s\" requested the list of variables",rRequest.pClientAddr->c_str());
  return Handle_GetVarList(rRequest,rResponse,m_lstMeasVars);
  }


//--------------------------------------------------------------------------
// Handle_GetVarListMailbox() responds with list of mailbox variables
// --------------------------
// Input: rRequest = [in] request information
//        rResponse = [out] response the http server shall send back
// Return: -

void CLoggerInterface::Handle_GetVarListMailbox (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  AddLog(LOGSEVERITY_TRACE,"A client \"%s\" requested the list of mailboxes",rRequest.pClientAddr->c_str());
  return Handle_GetVarList(rRequest,rResponse,m_lstMailboxVars);
  }


//--------------------------------------------------------------------------
// Handle_GetVarList()     responds with list of variables
// -------------------
// Input: rRequest = [in] request information
//        rResponse = [out] response the http server shall send back
//        rListDump = [in] list (either measurement or mailbox variables) to be placed into the response body
// Return: -

void CLoggerInterface::Handle_GetVarList (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse, const std::list<CVarRef *> &rListDump)
  {
  CJsonFormatter jfResponse; // {
  CJsonStreamArray jaVarList(&jfResponse,ABK_RSP_VARLIST); // VarList:[

  CAbkSingleLock lock(&m_mutexVarList,true,VARLIST_TIMEOUT); // lock the variable list
  for(list<CVarRef *>::const_iterator iterVars=rListDump.begin();iterVars!=rListDump.end();iterVars++)
    jaVarList.WriteValue((*iterVars)->GetName());
  rResponse.Set(jfResponse);
  }




//--------------------------------------------------------------------------
// Handle_PostVarMeta()     responds with variable meta data
// -------------------
// Input: rRequest = [in] request information
//        rResponse = [out] response the http server shall send back
//        rList = [in] list (either measurement or mailbox variables) whrere to search the variable in
// Return: -

void CLoggerInterface::Handle_PostVarMeta (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse, const std::list<CVarRef *> &rListSearch)
  {
  // prepare post data decoding
  bool bSuccess=true;
  std::stringstream strError;
  CJsonParser parseRequest(rRequest.pPostData->c_str());

  // prepare meta data formatting
  CJsonFormatter jfMeta;
  CJsonStreamArray jaMeta(&jfMeta,ABK_RSP_VARMETA_METADATA);

  // decode the requested variables and format the meta data output
  for(;!parseRequest.IsDone();++parseRequest)
    {
    if(parseRequest.TestArray(ABK_REQ_VARMETA_VARLIST)) // if a var list is specified
      {
      for(++parseRequest;!parseRequest.IsDone();++parseRequest)
        {
        std::string strVarName;
        parseRequest.GetValueString(strVarName);
        CVarRef *pVar=FindVariable(rListSearch,strVarName);
        if(!pVar)
          {
          strError<<"Variable "<<strVarName<<" not found";
          bSuccess=false;
          break;
          }
        CJsonStreamObject joVarMeta(&jaMeta); // place an object for each variable
        pVar->FormatMetaAsJson(joVarMeta);
        }
      }
    if(!bSuccess)
      break;
    }

  if(!bSuccess)
    rResponse.Set(HTTP_STATUSCODE_BAD_REQUEST,strError); // error response
  else
    rResponse.Set(jfMeta);
// cout << jfMeta.GetStream ()->str ().c_str () << "\n";
  }


//--------------------------------------------------------------------------
// Handle_PostVarMetaMeas() responds with measurement variable meta data
// -----------------------
// Input: rRequest = [in] request information
//        rResponse = [out] response the http server shall send back
// Return: -

void CLoggerInterface::Handle_PostVarMetaMeas (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  return Handle_PostVarMeta(rRequest,rResponse,m_lstMeasVars);  
  }


//--------------------------------------------------------------------------
// Handle_PostVarMetaMailbox() responds with mailbox variable meta data
// --------------------------
// Input: rRequest = [in] request information
//        rResponse = [out] response the http server shall send back
// Return: -

void CLoggerInterface::Handle_PostVarMetaMailbox (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  return Handle_PostVarMeta(rRequest,rResponse,m_lstMailboxVars);  
  }







//--------------------------------------------------------------------------
// Handle_PutDaqList()   puts a DAQ list to the session
// ---------------------
// Input: rRequest = [in] request information
//        rResponse = [out] response the http server shall send back
//        rVarList = [in] variable list (measurement or mailbox) where to search for the variable
//        pfnCreate = [in] function responsible for creating the daq list
// Return: -

void CLoggerInterface::Handle_PutDaqList (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse, const std::list<CVarRef *> &rVarList, PFN_CREATE_DAQ pfnCreate)
  {
  assert(rRequest.pQueryString); // void query string not supported here
  assert(rRequest.pPostData); // void post data not supproted here
  assert(rRequest.pClientAddr); // void client sender address not supported here
  std::stringstream strError; // the potential error to be placed to the response
  bool bSuccess=false; // success flag

  do
    {
    CAbkSingleLock lock(&m_mutexSessionList,true,SID_TIMEOUT);
    CSession *pSession=GetSession(rRequest); // get the session for which the list will be defined
    if(!pSession)
      {
      AddLog(LOGSEVERITY_ERROR,"Failed to retrieve the session for the client client %s. Query string was \"%s\"",rRequest.pClientAddr->c_str(),rRequest.pQueryString->c_str());
      break;
      }
    // get list name and cycle
    std::string strDaqName; // name of the DAQ list
    int nCycleMs=-1; // update cycle in ms
    const char *pszPutC=rRequest.pPostData->c_str();
    CJsonParser parseRequest(pszPutC);
    for(;!parseRequest.IsDone();++parseRequest)
      {
      parseRequest.ExtractValue(ABK_RSP_DAQLIST_NAME,&strDaqName);
      parseRequest.ExtractValue(ABK_RSP_DAQLIST_CYCLE,&nCycleMs);
      parseRequest.SkipItem();
      }
    if(strDaqName.empty())
      {
      AddLog(LOGSEVERITY_ERROR,"The client %s did not specify a name while requesting to put a DAQ list",rRequest.pClientAddr->c_str());
      strError<<"No Name for the daq list specified";
      break;
      }
    parseRequest.Restart(pszPutC);
    AddLog(LOGSEVERITY_TRACE,"The client %s, session %d requested to put a DAQ named \"%s\"",pSession->GetClientUid().m_strAddress.c_str(),pSession->GetSessionId(),strDaqName.c_str());

    // get list variables
    CDaqValues *pList=dynamic_cast<CDaqValues *>(pSession->PutDaqList(strDaqName.c_str(),pfnCreate)); // list to be created or updated (idempotent operation required)
    if(!pList)
      {
      AddLog(LOGSEVERITY_ERROR,"Invalid list name \"%s\" or name is already used for daq trend",strDaqName.c_str());
      strError<<"Invalid list name or name is already used for daq trend";
      break;
      }
    bool bArraySuccess=true;
    for(;!parseRequest.IsDone();++parseRequest)
      {
      if(parseRequest.TestArray(ABK_RSP_DAQLIST_DAQLIST)) // if a daq list is specified
        {
        pList->ClearList(); // a new variable list is specified, so clear the old one
        for(++parseRequest;!parseRequest.IsDone();++parseRequest)
          {
          std::string strVarName;
          parseRequest.GetValueString(strVarName);
          CVarRef *pVar=FindVariable(rVarList,strVarName);
          if(!pVar)
            {
            AddLog(LOGSEVERITY_ERROR,"The client %s Variable \"%s\" not found",pSession->GetClientUid().m_strAddress.c_str(),strDaqName.c_str());
            strError<<"Variable "<<strVarName<<" not found";
            break;
            }
          if(!pList->AddVar(pVar))
            {
            strError<<"Variable "<<strVarName<<" added to list multiple times";
            bArraySuccess=false;
            break;
            }
          }
        }
      if(!bArraySuccess)
        break;
      }
    if(!bArraySuccess)
      break;
    if(nCycleMs>0) // if the cycle was specified
      pList->SetCycle(nCycleMs);
    pList->Fire(); // fire the list (when cycle changes from slow to fast, an immediate response can be seen

    //if(!parser.TestToken("}")) // at end of array, a } is expected
    //  break;
    bSuccess=bArraySuccess;
    if(bSuccess)
      AddLog(LOGSEVERITY_TRACE,"Successfully added the DAQ \"%s\" for client %s, session %d",strDaqName.c_str(),pSession->GetClientUid().m_strAddress.c_str(),pSession->GetSessionId());
    } while(false);
  if(!bSuccess)
    rResponse.Set(HTTP_STATUSCODE_BAD_REQUEST,strError);
  else
    rResponse.Set(HTTP_STATUSCODE_OK);
  }


//--------------------------------------------------------------------------
// Handle_PutDaqListMeas() puts a variable daq list to the session
// -----------------------
// Input: rRequest = [in] request information
//        rResponse = [out] response the http server shall send back
// Return: -

void CLoggerInterface::Handle_PutDaqListMeas (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  return Handle_PutDaqList(rRequest,rResponse,m_lstMeasVars,CDaqList::Construct);
  }
  

//--------------------------------------------------------------------------
// Handle_PutDaqListMAM() puts a variable min-average-max daq list to the session
// -----------------------
// Input: rRequest = [in] request information
//        rResponse = [out] response the http server shall send back
// Return: -

void CLoggerInterface::Handle_PutDaqListMAM (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  return Handle_PutDaqList(rRequest,rResponse,m_lstMeasVars,CDaqMAM::Construct);
  }


//--------------------------------------------------------------------------
// Handle_PutDaqListMailbox() puts a variable daq list to the session
// --------------------------
// Input: rRequest = [in] request information
//        rResponse = [out] response the http server shall send back
// Return: -

void CLoggerInterface::Handle_PutDaqListMailbox (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  return Handle_PutDaqList(rRequest,rResponse,m_lstMailboxVars,CDaqList::Construct);
  }



//--------------------------------------------------------------------------
// Handle_PutDaqTrend()    puts a daq trend to the session
// --------------------
// Input: rRequest = [in] request information
//        rResponse = [out] response the http server shall send back
// Return: -

void CLoggerInterface::Handle_PutDaqTrend (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  std::stringstream strError;
  bool bSuccess=false; // success flag
  do
    {
    CAbkSingleLock lock(&m_mutexSessionList,true,SID_TIMEOUT);
    CSession *pSession=GetSession(rRequest); // get the session for which the daq trend will be defined
    if(!pSession)
      break;

    // get list name and cycle
    std::string strTrenName; // name of the daq trend list
    int nCycleMs=-1; // update cycle in ms
    std::string strVarName; // name of variable to be traced
    const char *pszPutC=rRequest.pPostData->c_str();
    CJsonParser parseRequest(pszPutC);
    for(;!parseRequest.IsDone();++parseRequest)
      {
      parseRequest.ExtractValue(ABK_RSP_DAQTREND_NAME,&strTrenName);
      parseRequest.ExtractValue(ABK_RSP_DAQTREND_CYCLE,&nCycleMs);
      parseRequest.ExtractValue(ABK_RSP_DAQTREND_VARNAME,&strVarName);
      parseRequest.SkipItem();
      }
    if(strTrenName.empty())
      {
      strError<<"No Name for the daq trend specified";
      break;
      }
    if(strVarName.empty())
      {
      strError<<"No variable specified";
      break;
      }
    CVarRef *pVar=FindVariable(m_lstMeasVars,strVarName);
    if(!pVar)
      {
      strError<<"Invalid variable name specified";
      break;
      }

    // create the trend daq
    CDaqTrend *pTrend=dynamic_cast<CDaqTrend *>(pSession->PutDaqTrend(strTrenName.c_str())); // trend to be created or updated (idempotent operation required)
    if(!pTrend)
      {
      strError<<"Invalid trend name or name is already used for a daq list";
      break;
      }
    pTrend->SetVar(pVar);
    if(nCycleMs>0) // if the cycle was specified
      pTrend->SetCycle(nCycleMs);
    pTrend->Fire(); // fire the trend (when cycle changes from slow to fast, an immediate response can be seen
    bSuccess=true;
    } while(false);
  if(!bSuccess)
    rResponse.Set(HTTP_STATUSCODE_BAD_REQUEST,strError);
  else
    rResponse.Set(HTTP_STATUSCODE_OK);
  }




//--------------------------------------------------------------------------
// Handle_DeleteDaqList() deletes a variable order list from the session
// ------------------------
// Input: rRequest = [in] request information
//        rResponse = [out] response the http server shall send back
// Return: -

void CLoggerInterface::Handle_DeleteDaqList (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  std::stringstream strError;
  std::string strListName; // name of the daq list
  bool bSuccess=false; // success flag
  do
    {
    CAbkSingleLock lock(&m_mutexSessionList,true,SID_TIMEOUT);
    CSession *pSession=GetSession(rRequest); // get the session for which the order list will be defined
    if(!pSession)
      {
      strError<<"No or invalid session id specified in '"<<*rRequest.pQueryString<<"'";
      break;
      }

    CJsonParser parseRequest(rRequest.pPostData->c_str());
    std::string strListName; // name of the order list
    for(;!parseRequest.IsDone();++parseRequest)
      {
      parseRequest.ExtractValue(ABK_DEL_DAQLIST_NAME,&strListName);
      parseRequest.SkipItem(); // skip subobjects and arrays
      }
    if(strListName.empty())
      {
      strError<<"No DAQ list name specified";
      break;
      }
    if(!pSession->DeleteDaqList(strListName.c_str())) // delete the order list
      {
      strError<<"DAQ list could not be deleted";
      break;
      }
    bSuccess=true;
    } while(false);
  if(!bSuccess)
    rResponse.Set(HTTP_STATUSCODE_BAD_REQUEST,strError);
  else
    rResponse.Set(HTTP_STATUSCODE_OK);
  }



//--------------------------------------------------------------------------
// Handle_EventPolling()   handles the main event polling
// ---------------------
// Input: rRequest = [in] request information
//        rResponse = [out] response the http server shall send back
// Return: -

void CLoggerInterface::Handle_EventPolling (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  bool bSuccess=true;
  if(!m_mutexSessionList.Lock(SID_TIMEOUT))
    {
    CJsonFormatter jfEmptyResponse;
    CJsonStreamObject joDataList(&jfEmptyResponse,ABK_RSP_SERVEREVENT_DATALISTS);
    joDataList.Close();
    jfEmptyResponse.Close();
    rResponse.Set(jfEmptyResponse);
    return;
    }
  std::stringstream strError;
  CSession *pSession=GetSession(rRequest); // get the session for which the order list will be defined
  if(!pSession)
    {
    m_mutexSessionList.Unlock();
    std::stringstream strError;
    strError<<"Unknown session ID: "<<*rRequest.pQueryString;
    rResponse.Set(HTTP_STATUSCODE_BAD_REQUEST,strError);
    return;
    }
  //if(pSession)
  //  pSession->Lock(SID_TIMEOUT); // prevent the session from being deleted
  pSession->m_bRequestInProgress=true; // invoke an error if somone wants to delete the session
  m_mutexSessionList.Unlock();
  
  //if there has not been another event fired before, assume we have just established event poll mechanism of this session
  if(pSession->IsFirstPoll())
	  OnSessionEventPollEstablished(pSession);

  // here is the delay for the long polling mechanism
  pSession->WaitEvent(ABK_LONGPOLL_MAXRESPONSE_MS); // wait for an event (e.g. daq list cycle)
  
  CJsonFormatter jfResponse;
  pSession->Lock(SID_TIMEOUT);

  // put events to the response
  if(!pSession->m_queueEvents.IsEmpty())
    {
    jfResponse.WriteValue(*(pSession->m_queueEvents.CloseAndGetFormatter()));
    pSession->m_queueEvents.Flush(); // events are copied to the response formatter, so prepare a new event queue
    }

  // put measurement data
  std::map <std::string,CDaq *>::iterator iterDaq=pSession->m_mapDaq.begin();
//  CJsonStreamArray jaDataList(&jfResponse,ABK_RSP_SERVEREVENT_DATALISTS);
  CJsonStreamObject joDataList(&jfResponse,ABK_RSP_SERVEREVENT_DATALISTS);
  for(;iterDaq!=pSession->m_mapDaq.end();iterDaq++) // lopp through all daqs
    {
    CDaq *pDaq=iterDaq->second; // get a daq
    pDaq->Lock(); // prevent the logger to write data while the data is put to the response
    if(pDaq->IsTranferPending()) // had it fired after the last transfer to the client?
      {
      pDaq->FormatAllData(&joDataList); // write values
      pDaq->ClearTransferPendingStatus(); // no more data transfer to client pending
      }
    pDaq->Unlock();
    }
  
  //pSession->Unlock(); // release session, it can be deleted (e.g. timeout)
  joDataList.Close(); // 16.Feb.15: added for completeness
  jfResponse.Close();
  pSession->m_bRequestInProgress=false;
  pSession->Unlock();

  rResponse.Set(jfResponse);
  }


//--------------------------------------------------------------------------
// Handle_InterfaceStatistic() handles the statistic query
// ---------------------------
// Input: rRequest = [in] request information
//        rResponse = [out] response the http server shall send back
// Return: -

void CLoggerInterface::Handle_InterfaceStatistic (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  CJsonFormatter jfResponse;
  // session info
  m_mutexSessionList.Lock(SID_TIMEOUT);
  jfResponse.WriteValue("NextSessionId",m_nNextSessionId);
  jfResponse.WriteValue("SessionCount",(int)m_mapSessions.size());

  CJsonStreamArray jaSessions(&jfResponse,"Sessions");
  std::map<int, CSession *>::iterator iterSession=m_mapSessions.begin();
  for(;iterSession!=m_mapSessions.end();iterSession++)
    {
    CJsonStreamObject joSession(&jaSessions);
    int nSessionId=iterSession->first;
    CSession *pSession=iterSession->second;
    pSession->FormatStatistics(&joSession);
    }
  m_mutexSessionList.Unlock();
  rResponse.Set(jfResponse);
  }





//--------------------------------------------------------------------------
// Handle_DeleteSession()  handles the session deletion request
// ----------------------
// Input: pConnection = connection recieved the request
//        strQueryString = query string of uri without quotation mark ('?')
// Return: true if it was handled, false if it was not handled

void CLoggerInterface::Handle_DeleteSession (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  std::stringstream strError;
  bool bSuccess=false; // success flag
  do
    {
    CAbkSingleLock lock(&m_mutexSessionList,true,SID_TIMEOUT);
    CSession *pSession=GetSession(rRequest); // get the session for which the order list will be defined
    if(!pSession)
      {
      strError<<"Session id not valid in query string"<<*rRequest.pQueryString;
      break;
      }
    pSession->FireEvent(); // fire an event to end a current request which is in progress
    for(;;)
      {
      if(DeleteSession(pSession)) // delete the session (try as long as a request is in progress)
        break;
      }
    bSuccess=true;
    } while(false);
  if(!bSuccess)
    rResponse.Set(HTTP_STATUSCODE_BAD_REQUEST,strError);
  else
    rResponse.Set(HTTP_STATUSCODE_OK);
  }




//--------------------------------------------------------------------------
// Handle_PutClientEvent() handles events coming from a client
// -----------------------
// Input: rRequest = [in] request information
//        rResponse = [out] response the http server shall send back
// Return: -

void CLoggerInterface::Handle_PutClientEvent (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  bool bSuccess=true;
  std::stringstream strError;
  CJsonParser parsEvent(rRequest.pPostData->c_str());
  
  std::string strStringParam;
  bool bParamStringSent=false;
  double dParam1=0.;
  bool bParam1Sent=false;
  double dParam2=0.;
  bool bParam2Sent=false;
  int nSender; // sender id (session id the client has)
  bool bSenderSent=false;
  std::string strEventType;
  bool bEventTypeSent=false;
  time_t tmSent;
  bool bTimeSent=false;
  bool bPrivate=false;

  for(;!parsEvent.IsDone();++parsEvent)
    {
    CJsonParser::TYPE nItemType=parsEvent.GetType();
    bSenderSent       |=parsEvent.ExtractValue(ABK_RSP_CLIENTEVENT_SENDER  ,&nSender);
    bTimeSent         |=parsEvent.ExtractValue(ABK_RSP_CLIENTEVENT_TIME    ,&tmSent);
    bEventTypeSent    |=parsEvent.ExtractValue(ABK_RSP_CLIENTEVENT_TYPE    ,&strEventType);
    bParamStringSent  |=parsEvent.ExtractValue(ABK_RSP_CLIENTEVENT_STRPARAM,&strStringParam);
    bParam1Sent       |=parsEvent.ExtractValue(ABK_RSP_CLIENTEVENT_PARAM1  ,&dParam1);
    bParam2Sent       |=parsEvent.ExtractValue(ABK_RSP_CLIENTEVENT_PARAM2  ,&dParam2);
    parsEvent.ExtractValue(ABK_RSP_CLIENTEVENT_PRIVATE,&bPrivate); // is the event private?
    parsEvent.SkipItem(); // skip any sub arrays or objects
    }
  if(parsEvent.IsError())
    {
    bSuccess=false;
    strError<<"Syntax error near: "<<parsEvent.GetPosition();
    }
  if(!bSenderSent)
    {
    bSuccess=false;
    strError<<"Sender is missing.";
    }
  if(!bEventTypeSent)
    {
    bSuccess=false;
    strError<<"Event type is missing.";
    }
  if(!bTimeSent)
    time(&tmSent);

  if(!bSuccess)
    rResponse.Set(HTTP_STATUSCODE_BAD_REQUEST,strError);
  else
    {
    rResponse.Set(HTTP_STATUSCODE_OK);
    bool bHandled=OnClientEvent(nSender,tmSent,strEventType,strStringParam,dParam1,dParam2); // call the event handler
    if(!bHandled) // if the logger didnt handle the event, send it to all connected clients
      {
      if(!bPrivate) // if client wants the event to be reflected to all other clients
        FireEventToAllClients(nSender,tmSent,strEventType.c_str(),strStringParam,dParam1,dParam2);
      else
        {
        CAbkSingleLock lockSessions(&m_mutexSessionList,true,SID_TIMEOUT);
        CSession *pSession=GetSession(nSender);
        if(pSession)
          pSession->FireEventToClient(nSender,tmSent,strEventType.c_str(),strStringParam,dParam1,dParam2,true); // 04. July 2014 D. Burger: added true in order to respond with primary role attribute
        }
      }
    }
  }




//--------------------------------------------------------------------------
// Handle_GetClientAddress() handles ip address request from the client
// -------------------------
// Input: rRequest = [in] request information
//        rResponse = [out] response the http server shall send back
// Return: -

void CLoggerInterface::Handle_GetClientAddress (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  CJsonFormatter jfResponse;
  jfResponse.WriteValue(ABK_RSP_CLIENTADDRESS,*rRequest.pClientAddr);
  rResponse.Set(jfResponse);
  }



//--------------------------------------------------------------------------
// Handle_GetServerInfo() handles server info get request
// ----------------------
// Input: rRequest = [in] request information
//        rResponse = [out] response the http server shall send back
// Return: -

void CLoggerInterface::Handle_GetServerInfo (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  CJsonFormatter jfResponse;
  jfResponse.WriteValue(ABK_RSP_SERVERINFO_PROTOVERSION,ABK_PROTOCOL_VERSION);
  jfResponse.WriteValue(ABK_RSP_SERVERINFO_IFVERSION,IF_VERSION);
  std::string strLoggerInfo;
  
  strLoggerInfo.clear();
  if(OnGetLoggerFwVersion(strLoggerInfo))
    jfResponse.WriteValue(ABK_RSP_SERVERINFO_FWVERSION,strLoggerInfo);
  
  strLoggerInfo.clear();
  if(OnGetLoggerHwVersion(strLoggerInfo))
    jfResponse.WriteValue(ABK_RSP_SERVERINFO_HWVERSION,strLoggerInfo);

  strLoggerInfo.clear();
  if(OnGetLoggerName(strLoggerInfo))
    jfResponse.WriteValue(ABK_RSP_SERVERINFO_NAME,strLoggerInfo);

  strLoggerInfo.clear();
  if(OnGetLoggerType(strLoggerInfo))
    jfResponse.WriteValue(ABK_RSP_SERVERINFO_TYPE,strLoggerInfo);

  strLoggerInfo.clear();
  if(OnGetLoggerDescriptionUrl(strLoggerInfo))
    jfResponse.WriteValue(ABK_RSP_SERVERINFO_DESCURL,strLoggerInfo);
  
  rResponse.Set(jfResponse);
  }


//--------------------------------------------------------------------------
// Handle_GetCurrentTime() handles time get request
// -----------------------
// Input: rRequest = [in] request information
//        rResponse = [out] response the http server shall send back
// Return: -

void CLoggerInterface::Handle_GetCurrentTime (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  CJsonFormatter jfResponse;
  time_t tmNow;
  time(&tmNow);
  jfResponse.WriteValue(ABK_RSP_CURRENTTIME_TIME,tmNow);
  rResponse.Set(jfResponse);
  }



//--------------------------------------------------------------------------
// Handle_GetStorageInfo() handles storage info get request
// -----------------------
// Input: rRequest = [in] request information
//        rResponse = [out] response the http server shall send back
// Return: -

void CLoggerInterface::Handle_GetStorageInfo (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  CJsonFormatter jfResponse;
  unsigned long long ullStorageTotal=0;
  unsigned long long ullStorageFree=0;
  if(OnGetStorageInfo(ullStorageTotal,ullStorageFree)) // get the storage info from the logger
    {
    // here, transfering it as double is considered as sufficient. We avoid decoding issues with JS described at http://stackoverflow.com/questions/17320706/javascript-long-integer
    double dStorageTotal=(double)ullStorageTotal;
    double dStorageFree=(double)ullStorageFree;
    jfResponse.WriteValue(ABK_RSP_STORAGEINFO_TOTAL,dStorageTotal);
    jfResponse.WriteValue(ABK_RSP_STORAGEINFO_FREE,dStorageFree);
    rResponse.Set(jfResponse);
    return;
    }
  std::stringstream strError;
  strError<<"Storage information not supported"<<*rRequest.pQueryString;
  rResponse.Set(HTTP_STATUSCODE_NOT_IMPLEMENTED,strError);
  }



//--------------------------------------------------------------------------
// Handle_PostVarValue()    handles variable value POST request
// ---------------------
// Input: rRequest = [in] request information
//        rResponse = [out] response the http server shall send back
//        rVarList = [in] variable list (measurement or mailbox) where to search for the variable(s) to be returned
// Return: -

void CLoggerInterface::Handle_PostVarValue (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse, const std::list<CVarRef *> &rVarList)
  {
  bool bSuccess=true;
  std::stringstream strError;
  CJsonParser parseRequest(rRequest.pPostData->c_str());
  std::list<CVarRef *> lstRequest; // requested variables
  bool bListSent=false;

  for(;!parseRequest.IsDone();++parseRequest) // loop for root object members
    {
    if(parseRequest.TestArray(ABK_REQ_VARVALUE_GETLIST)) // if a daq list is specified
      {
      bListSent=true;
      for(++parseRequest;!parseRequest.IsDone();++parseRequest) // loop for array members
        {
        std::string strVarName;
        parseRequest.GetValueString(strVarName);
        CVarRef *pVar=FindVariable(rVarList,strVarName);
        if(!pVar)
          {
          strError<<"Variable "<<strVarName<<" not found.";
          bSuccess=false;
          break;
          }
        lstRequest.push_back(pVar); // append to request list
        }
      }
    if(!bSuccess)
      break;
    }
  if(!bListSent)
    {
    strError<<"No request list was sent.";
    bSuccess=false;
    }

  if(!bSuccess)
    rResponse.Set(HTTP_STATUSCODE_BAD_REQUEST,strError);
  else
    {
    // return measurement data
    CJsonFormatter jfResponse;
    CJsonStreamArray jaData(&jfResponse,ABK_RSP_VARVALUE_DATA);
    std::list<CVarRef *>::iterator iterVar=lstRequest.begin();
    int nVar=0;
    for(;iterVar!=lstRequest.end();iterVar++)
      {
      CVarRef *pVarRef=*iterVar;
      jaData.BeginMember();
      pVarRef->OnFormatValue(jaData); // let the variable format its value
      nVar++;
      }
    jfResponse.Close();
    rResponse.Set(jfResponse);
    }
  }


//--------------------------------------------------------------------------
// Handle_PostVarValueMeas() handles variable value POST request
// -------------------------
// Input: rRequest = [in] request information
//        rResponse = [out] response the http server shall send back
// Return: -

void CLoggerInterface::Handle_PostVarValueMeas (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  Handle_PostVarValue(rRequest,rResponse,m_lstMeasVars);
  }


//--------------------------------------------------------------------------
// Handle_PostVarValueMailbox() handles mailbos value POST request
// ----------------------------
// Input: rRequest = [in] request information
//        rResponse = [out] response the http server shall send back
// Return: -

void CLoggerInterface::Handle_PostVarValueMailbox (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  Handle_PostVarValue(rRequest,rResponse,m_lstMailboxVars);
  }


//--------------------------------------------------------------------------
// Handle_PostMamValues()  handles variable value POST request
// ----------------------
// Input: rRequest = [in] request information
//        rResponse = [out] response the http server shall send back
// Return: -

void CLoggerInterface::Handle_PostMamValues (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  /*
  20TH SEPTEMBER 2013, D. BURGER, EMBU-SYS: THE CODE OF THIS FUNCTION WAS NEVER TESTED
  */
  bool bSuccess=true;
  std::stringstream strError;
  CJsonParser parseRequest(rRequest.pPostData->c_str());
  std::list<CVarRef *> lstRequest; // requested variables
  bool bListSent=false;

  for(;!parseRequest.IsDone();++parseRequest) // loop for root object members
    {
    if(parseRequest.TestArray(ABK_REQ_VARVALUE_GETLIST)) // if a list is specified
      {
      bListSent=true;
      for(++parseRequest;!parseRequest.IsDone();++parseRequest) // loop for array members
        {
        std::string strVarName;
        parseRequest.GetValueString(strVarName);
        CVarRef *pVar=FindVariable(m_lstMeasVars,strVarName);
        if(!pVar)
          {
          strError<<"Variable "<<strVarName<<" not found.";
          bSuccess=false;
          break;
          }
        lstRequest.push_back(pVar); // append to request list
        }
      }
    if(!bSuccess)
      break;
    }
  if(!bListSent)
    {
    strError<<"No request list was sent.";
    bSuccess=false;
    }

  if(!bSuccess)
    rResponse.Set(HTTP_STATUSCODE_BAD_REQUEST,strError);
  else
    {
    // return measurement data
    CJsonFormatter jfResponse;
    CJsonStreamArray jaData(&jfResponse,ABK_RSP_VARVALUE_DATA);
    std::list<CVarRef *>::iterator iterVar=lstRequest.begin();
    int nVar=0;
    for(;iterVar!=lstRequest.end();iterVar++)
      {
      CVarRef *pVarRef=*iterVar;
      jaData.BeginMember();
      pVarRef->OnFormatMAM(jaData); // let the variable format its Min/Average/Max values
      nVar++;
      }
    jfResponse.Close();
    rResponse.Set(jfResponse);
    }
  }




//--------------------------------------------------------------------------
// Handle_PutVarValue() handles variable value put request
// --------------------
// Input: rRequest = [in] request information
//        rResponse = [out] response the http server shall send back
//        rVarList = [in] variable list (measurement or mailbox) where to search for the variable(s) to be changed
// Return: -

void CLoggerInterface::Handle_PutVarValue (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse, std::list<CVarRef *> &rVarList)
  {
  bool bSuccess=true;
  std::stringstream strError;
  CJsonParser parseRequest(rRequest.pPostData->c_str());
  bool bListSent=false;

  for(;!parseRequest.IsDone();++parseRequest) // loop for root object members
    {
    if(parseRequest.TestArray(ABK_REQ_VARVALUE_PUTLIST)) // if a daq list is specified
      {
      bListSent=true;
      for(++parseRequest;!parseRequest.IsDone();++parseRequest) // loop for array members
        {
        if(parseRequest.TestObject(""))
          {
          bool bNameSent=false;
          bool bValueNumericSent=false;
          bool bValueStringSent=false;
          std::string strVarName;
          std::string strName;
          std::string strValue;
          double dValue;
          for(++parseRequest/*step into object*/;!parseRequest.IsDone();++parseRequest) // loop for object members (object such {"Name":"Messwert1", "Value":12})
            {
            bNameSent        |= parseRequest.ExtractValue(ABK_REQ_VARVALUE_NAME,&strVarName);
            bValueNumericSent|= parseRequest.ExtractValue(ABK_REQ_VARVALUE_VALUE,&dValue);
            bValueStringSent |= parseRequest.ExtractValue(ABK_REQ_VARVALUE_VALUE,&strValue);
            }
          if(bNameSent&&(bValueStringSent||bValueNumericSent)) // if completely sent
            {
            CVarRef *pVar=FindVariable(rVarList,strVarName);
            if(!pVar)
              {
              strError<<"Variable "<<strVarName<<" not found.";
              bSuccess=false;
              break;
              }
            if(bValueNumericSent)
              pVar->OnSetValue(dValue);
            else if(bValueStringSent)
              pVar->OnSetValue(strValue);
            else
              assert(false); // neither string nor numeric value specified
            }
          }
        }
      }
    if(!bSuccess)
      break;
    }
  if(!bListSent)
    {
    strError<<"No list of name/value-pairs was sent.";
    bSuccess=false;
    }

  if(!bSuccess)
    rResponse.Set(HTTP_STATUSCODE_BAD_REQUEST,strError);
  else
    {
    rResponse.Set(HTTP_STATUSCODE_OK);
    }
  }


//--------------------------------------------------------------------------
// Handle_PutVarValueMeas() handles variable value put request
// ------------------------
// Input: pConnection = connection recieved the request
//        strQueryString = query string of uri without quotation mark ('?')
// Return: true if it was handled, false if it was not handled

void CLoggerInterface::Handle_PutVarValueMeas (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  return Handle_PutVarValue(rRequest,rResponse,m_lstMeasVars);    
  }


//--------------------------------------------------------------------------
// Handle_PutVarValueMailbox() handles mailbox value put request
// ---------------------------
// Input: pConnection = connection recieved the request
//        strQueryString = query string of uri without quotation mark ('?')
// Return: true if it was handled, false if it was not handled

void CLoggerInterface::Handle_PutVarValueMailbox (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  return Handle_PutVarValue(rRequest,rResponse,m_lstMailboxVars);
  }


//--------------------------------------------------------------------------
// Handle_PostFirmwareInfo() handles request about available firmware
// -------------------------
// Input: pConnection = connection recieved the request
//        strQueryString = query string of uri without quotation mark ('?')
// Return: true if it was handled, false if it was not handled

void CLoggerInterface::Handle_PostFirmwareInfo (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  bool bSuccess=true;
  std::stringstream strError;
  CJsonParser jpRequest(rRequest.pPostData->c_str());
  
  // decode the request
  std::string strClientClass; // class of the client
  std::string strClientType; // type string fo the client
  bool bClassSpecified=false;
  bool bTypeSpecified=false;
  for(;!jpRequest.IsDone();++jpRequest) // loop for root object members
    {
    bClassSpecified |=jpRequest.ExtractValue(ABK_REQ_FIRMWARE_CLASS,&strClientClass); // get the client class
    bTypeSpecified  |=jpRequest.ExtractValue(ABK_REQ_FIRMWARE_TYPE, &strClientType);  // get the client type
    jpRequest.SkipItem();
    }
  if(!bClassSpecified)
    {
    strError<<"Client class was not specified.";
    bSuccess=false;
    }
  if(!bTypeSpecified)
    {
    strError<<"Client type was not specified.";
    bSuccess=false;
    }

  std::list<CClientFirmware> lstFwInfo; // list with firmware info
  bool bSupported=OnGetClientFirmwareInfo(strClientClass.c_str(),strClientType.c_str(),lstFwInfo); // implementation specific rendering of the client firmware info
  if(!bSupported)
    {
    strError<<"Information about firmware for clients not supported.";
    rResponse.Set(HTTP_STATUSCODE_NOT_IMPLEMENTED,strError);
    return;
    }

  if(!bSuccess)
    rResponse.Set(HTTP_STATUSCODE_BAD_REQUEST,strError);
  else
    {
    // return firmware information
    CJsonFormatter jfResponse; // {
      {
      CJsonStreamArray jaImages(&jfResponse,ABK_RSP_FIRMWARE_IMAGELIST); // Images: [
      std::list<CClientFirmware>::iterator iterImage=lstFwInfo.begin();
      for(;iterImage!=lstFwInfo.end();iterImage++)
        {
        CClientFirmware *pFwInfo=&*iterImage;

        CJsonStreamObject joImage(&jaImages); // {
        assert(!pFwInfo->m_strUrl.empty()); // please intercept empty urls outside (in the OnGetClientFirmwareInfo() procedure)
        joImage.WriteValue(ABK_RSP_FIRMWARE_URL,pFwInfo->m_strUrl); // "Url": // write url where client can download its image
        if(!pFwInfo->m_strVersion.empty())
          joImage.WriteValue(ABK_RSP_FIRMWARE_VERSION,pFwInfo->m_strVersion); // "Version": // write version
        if(!pFwInfo->m_strMd5.empty())
          joImage.WriteValue(ABK_RSP_FIRMWARE_MD5,pFwInfo->m_strMd5); // "MD5": // write MD5 hash
        } // joImage falls out of scope => }
      } // jaImages falls out of scope => ]
    jfResponse.Close(); // }
    rResponse.Set(jfResponse); // send response to client
    }
  }


//--------------------------------------------------------------------------
// Handle_PostClientConfigInfo() handles request about client firmware
// -----------------------------
// Input: rRequest = 
//        rResponse = 
// Return: 

void CLoggerInterface::Handle_PostClientConfigInfo (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  bool bSuccess=true;
  std::stringstream strError;
  CJsonParser jpRequest(rRequest.pPostData->c_str());
  
  // decode the request
  std::string strClientClass; // class of the client
  std::string strClientType; // type string fo the client
  std::string strClientSerial; // serial number or ID of the client
  bool bClassSpecified=false;
  bool bTypeSpecified=false;
  bool bSerialSpecified=false;
  for(;!jpRequest.IsDone();++jpRequest) // loop for root object members
    {
    bClassSpecified|=jpRequest.ExtractValue(ABK_REQ_CLIENTCONFIG_CLASS,&strClientClass); // get the client class
    bTypeSpecified|=jpRequest.ExtractValue(ABK_REQ_CLIENTCONFIG_TYPE,&strClientType); // get the client type
    bSerialSpecified|=jpRequest.ExtractValue(ABK_REQ_CLIENTCONFIG_SERIAL,&strClientSerial); // get the client serial id
    jpRequest.SkipItem();
    }
  if(!bClassSpecified)
    {
    strError<<"Client config info request: Client class was not specified.";
    bSuccess=false;
    }
  if(!bTypeSpecified)
    {
    strError<<"Client config info request: Client type was not specified.";
    bSuccess=false;
    }
  if(!bSerialSpecified)
    {
    strError<<"Client config info request: Client serial id was not specified.";
    bSuccess=false;
    }

  CClientConfig cfgInfo; // client config info structure
  bool bSupported=OnGetClientConfigInfo(strClientClass.c_str(),strClientType.c_str(),strClientSerial.c_str(),cfgInfo); // implementation specific rendering of the client config info
  if(!bSupported)
    {
    strError<<"Information about configuration for clients not supported.";
    rResponse.Set(HTTP_STATUSCODE_NOT_IMPLEMENTED,strError);
    return;
    }

  if(!bSuccess)
    rResponse.Set(HTTP_STATUSCODE_BAD_REQUEST,strError);
  else
    {
    // return client config file information
    CJsonFormatter jfResponse; // {
      {
      CJsonStreamObject joFile(jfResponse); // {
      joFile.WriteValue(ABK_RSP_CLIENTCONFIG_URL,cfgInfo.m_strUrl); // "Url": // write url where client can download its file. Note that it can be empty if no file is supported
      if(!cfgInfo.m_strMd5.empty()) // if a valid URL was specified, output the MD5 hash
        joFile.WriteValue(ABK_RSP_CLIENTCONFIG_MD5,cfgInfo.m_strMd5); // "MD5": // write MD5 hash
      } // joFile falls out of scope => }
    jfResponse.Close(); // }
    rResponse.Set(jfResponse); // send response to client
    }
  }



//--------------------------------------------------------------------------
// Handle_GetForm()        handles the forms get method
// ----------------
// Input: strFormName = name of the form to be retrieved
//        pConnection = connection recieved the request
//        strQueryString = query string of uri without quotation mark ('?')
// Return: true if it was handled, false if it was not handled

void CLoggerInterface::Handle_GetForm (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse, const char *pszFormName)
  {
  std::stringstream strError;
  CJsonFormatter jfForm; // the form will be rendered here
  bool bSuccess=OnFormGet(pszFormName,jfForm,strError);
  if(!bSuccess)
    rResponse.Set(HTTP_STATUSCODE_BAD_REQUEST,strError);
  else
    {
    jfForm.Close(); // }
    rResponse.Set(jfForm); // send response to client
    }
  }


//--------------------------------------------------------------------------
// Handle_PutForm()        handles the forms put method
// ----------------
// Input: strFormName = name of the form whichs values are to be set
//        pConnection = connection recieved the request
//        strQueryString = query string of uri without quotation mark ('?')
// Return: true if it was handled, false if it was not handled

void CLoggerInterface::Handle_PutForm (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse, const char *pszFormName)
  {
  std::stringstream strError;
  CJsonParser jpRequest(rRequest.pPostData->c_str());
  bool bSuccess=OnFormPut(pszFormName,jpRequest,strError);
  if(!bSuccess)
    rResponse.Set(HTTP_STATUSCODE_BAD_REQUEST,strError);
  else
    rResponse.Set(HTTP_STATUSCODE_OK); // send response to client
  }




//--------------------------------------------------------------------------
// Handle_PutAudioRecHeader() audio recording header
// --------------------------
// Input: rRequest = 
//        rResponse = 
// Return: 

void CLoggerInterface::Handle_PutAudioRecHeader (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  bool bSuccess=true;
  std::stringstream strError;
  CJsonParser parsHeader(rRequest.pPostData->c_str());
  int nId=0;
  bool bIdSent=false;
  int nSampleRateHz=0;
  bool bSampleRateSent=false;
  int nChannels=1;
  bool bChannelsSent=false;
  int nBitsPerSample=8;
  bool bBitsPerSampleSent=false;

  for(;!parsHeader.IsDone();++parsHeader)
    {
    bIdSent            |= parsHeader.ExtractValue(ABK_AUDIOREC_ID            ,&nId);
    bSampleRateSent    |= parsHeader.ExtractValue(ABK_AUDIOREC_SAMPLERATE_HZ ,&nSampleRateHz);
    bChannelsSent      |= parsHeader.ExtractValue(ABK_AUDIOREC_CHANNELS      ,&nChannels);
    bBitsPerSampleSent |= parsHeader.ExtractValue(ABK_AUDIOREC_BITSPERSAMPLE ,&nBitsPerSample);
    parsHeader.SkipItem(); // skip any sub arrays or objects
    }
  if(parsHeader.IsError())
    {
    bSuccess=false;
    strError<<"Syntax error near: "<<parsHeader.GetPosition();
    }
  if(!bIdSent)
    {
    bSuccess=false;
    strError<<"ID is missing.";
    }
  if(!bSampleRateSent)
    {
    bSuccess=false;
    strError<<"Sample rate is missing.";
    }
  if(!bChannelsSent)
    {
    bSuccess=false;
    strError<<"Channel count is missing.";
    }
  if(!bBitsPerSampleSent)
    {
    bSuccess=false;
    strError<<"Bits per sample is missing.";
    }

  if(bSuccess)
    {
    if(OnAudioRecHeader(nId,nSampleRateHz,nChannels,nBitsPerSample))
      {
      rResponse.Set(HTTP_STATUSCODE_OK);
      }
    else
      {
      strError<<"Failed to create the audio storage";
      rResponse.Set(HTTP_STATUSCODE_INSUFFICIENT_STORAGE,strError);
      }
    }
  else
    {
    rResponse.Set(HTTP_STATUSCODE_BAD_REQUEST,strError);
    }
cout<<"RecHeader "<<nId; // ####
  }



//--------------------------------------------------------------------------
// Handle_PutAudioRecData() audio recording data
// ------------------------
// Input: rRequest = 
//        rResponse = 
// Return: 

void CLoggerInterface::Handle_PutAudioRecData (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  bool bSuccess=true;
  std::stringstream strError;
  CJsonParser parsData(rRequest.pPostData->c_str());
  int nId=0;
  bool bIdSent=false;
  std::vector<int> vectSampleData; // sample data get stored here

  for(;!parsData.IsDone();++parsData)
    {
    bIdSent |= parsData.ExtractValue(ABK_AUDIOREC_ID,&nId);
    if(parsData.TestArray(ABK_AUDIOREC_DATA))
      {
      vectSampleData.reserve(rRequest.pPostData->length()/2); // each sample data contains at least a digit and a comma. so allocating half string length is always sufficient
      for(++parsData;!parsData.IsDone();++parsData)
        {
        int nSampleData;
        if(parsData.ExtractValue(&nSampleData))
          vectSampleData.push_back(nSampleData);
        }
      }
    parsData.SkipItem(); // skip any sub arrays or objects
    }
  if(parsData.IsError())
    {
    bSuccess=false;
    strError<<"Syntax error near: "<<parsData.GetPosition();
    }
  if(!bIdSent)
    {
    bSuccess=false;
    strError<<"ID is missing.";
    }

  if(bSuccess)
    {
    if(int nSamples=vectSampleData.size())
      {
      if(OnAudioRecData(nId,&vectSampleData[0],vectSampleData.size()))
        {
        rResponse.Set(HTTP_STATUSCODE_OK);
        }
      else
        {
        strError<<"Failed to put data to the audio storage";
        rResponse.Set(HTTP_STATUSCODE_INSUFFICIENT_STORAGE,strError);
        }
      }
    else
      {
      rResponse.Set(HTTP_STATUSCODE_OK); // successfully put no data
      }
    }
  else
    {
    rResponse.Set(HTTP_STATUSCODE_BAD_REQUEST,strError);
    }

  }



//--------------------------------------------------------------------------
// Handle_PutAudioRecFooter() audio recording footer
// --------------------------
// Input: rRequest = 
//        rResponse = 
// Return: 

void CLoggerInterface::Handle_PutAudioRecFooter (const HTTP_REQUEST &rRequest, HTTP_RESPONSE &rResponse)
  {
  bool bSuccess=true;
  std::stringstream strError;
  CJsonParser parsData(rRequest.pPostData->c_str());
  int nId=0;
  bool bIdSent=false;

  for(;!parsData.IsDone();++parsData)
    {
    bIdSent |= parsData.ExtractValue(ABK_AUDIOREC_ID,&nId);
    parsData.SkipItem(); // skip any sub arrays or objects
    }
  if(parsData.IsError())
    {
    bSuccess=false;
    strError<<"Syntax error near: "<<parsData.GetPosition();
    }
  if(!bIdSent)
    {
    bSuccess=false;
    strError<<"ID is missing.";
    }

  if(bSuccess)
    {
    if(OnAudioRecFooter(nId))
      {
      rResponse.Set(HTTP_STATUSCODE_OK);
      }
    else
      {
      strError<<"Failed to close the audio storage";
      rResponse.Set(HTTP_STATUSCODE_INSUFFICIENT_STORAGE,strError);
      }
    }
  else
    {
    rResponse.Set(HTTP_STATUSCODE_BAD_REQUEST,strError);
    }
cout<<"  RecFooter "<< nId <<"\n"; // ####
  }





//--------------------------------------------------------------------------
// OnAudioRecHeader()      gets called when client starts an audio recording
// ------------------
// Input: nId = general purpose ID the server wanted to be reflected by the client
//        nSamplerateHz = sample rate in Hz
//        nChannels = number of channels (1=mono, 2=stereo)
//        nBitsPerSample = number of bits per sample (8 or 16)
// Return: true if e.g. file could be created successfully. false otherwise

/*virtual*/ bool CLoggerInterface::OnAudioRecHeader (int nId, int nSamplerateHz, int nChannels, int nBitsPerSample) const
  {
  // in derived classes, for example, create a file. No need to call the base class implementation
  return false;
  }



//--------------------------------------------------------------------------
// OnAudioRecData()        gets called when client has audio recording data
// ----------------
// Input: nId = general purpose ID the server wanted to be reflected by the client
// Input: pSampleData = data. Number of entities must be nSamples
//                      if multiple channels (n): first n data are the first samples of n channels and so on
//        nSamples = number of samples (of all channels)
// Return: 

/*virtual*/ bool CLoggerInterface::OnAudioRecData (int nId, const int *pSampleData, int nSamples) const
  {
  // in derived classes, for example, write data to the file in the format you got with OnAudioRecHeader(). No need to call the base class implementation
  return false;
  }



//--------------------------------------------------------------------------
// OnAudioRecFooter()      gets called when client terminates audio recording
// ------------------
// Input: nId = general purpose ID the server wanted to be reflected by the client
// Return: 

/*virtual*/ bool CLoggerInterface::OnAudioRecFooter (int nId) const
  {
  // in derived classes, close the file. No need to call the base class implementation
  return false;
  }



//--------------------------------------------------------------------------
// OnClientEvent()         called when an event from a client has been received
// ---------------         do not call the default implementation in yur derived function
// Input: strSender = sender address identification
//        tmSent = time at the senders clock in local time
//        strEventType = type of event
//        strParam = general purpose string parameter
//        dParam1 = general purpose numeric param
//        dParam2 = general purpose numeric param
// Return: true if handled. no further dispatching is performed
//         false if not handled, event will be dispatched to all clients

/*virtual*/ bool CLoggerInterface::OnClientEvent (int nSender, time_t tmSent, const std::string &strEventType, const std::string &strParam, double dParam1, double dParam2)
  {
  return false;
  }



//--------------------------------------------------------------------------
// FireEventToAllClients() fires an event to all connected clients
// -----------------------
// Input: nSender = [in] session id of the sender
//        tmSend = [in] locoal time, timestamp for event. if 0, actual time will be set
//        pszEventType = [in] type of event
//        strParam = [in] general purpose string parameter
//        dParam1 = [in] general purpose numeric param
//        dParam2 = [in] general purpose numeric param
// Return: -

void CLoggerInterface::FireEventToAllClients (int nSender, time_t tmSend, const char *pszEventType, const std::string &strParam, double dParam1, double dParam2)
  {
  CAbkSingleLock lock(&m_mutexSessionList,true,SID_TIMEOUT);
  std::map<int, CSession *>::iterator iterSession=m_mapSessions.begin();
  if(tmSend==0) // if no time is specified..
    time(&tmSend); // .. use current local time
  for(;iterSession!=m_mapSessions.end();iterSession++)
    {
    CSession *pSession=iterSession->second;
    pSession->FireEventToClient(nSender,tmSend,pszEventType,strParam,dParam1,dParam2);
    }  
  }

//--------------------------------------------------------------------------
// FireEventToAllClients() fires an event to all connected clients
// -----------------------
// Input: strEventType = type of event
//        strParam = general purpose string parameter
//        dParam1 = general purpose numeric param
//        dParam2 = general purpose numeric param
// Return: -
// the send-time and will be created automatically

void CLoggerInterface::FireEventToAllClients (const char *pszEventType, const std::string &strParam, double dParam1, double dParam2)
  {
  FireEventToAllClients(0,0,pszEventType,strParam,dParam1,dParam2);
  }


//--------------------------------------------------------------------------
// FireEventToAllClients() fires an event to all connected clients, no params
// -----------------------
// Input: strEventType = type of event
// Return: 

void CLoggerInterface::FireEventToAllClients (const char *pszEventType)
  {
  std::string strParamDummy;
  FireEventToAllClients(0,0,pszEventType,strParamDummy,0.,0.);
  }


//--------------------------------------------------------------------------
// FireEventToAllClientsMeasurementStarted() notifies clients that measurement has started
// -----------------------------------------
// Input: -
// Return: 

void CLoggerInterface::FireEventToAllClientsMeasurementStarted ()
  {
  FireEventToAllClients(ABK_SVREVENT_MEASSTARTED);
  }

//--------------------------------------------------------------------------
// FireEventToAllClientsMeasurementStopped() notifies clients that measurement has stopped
// -----------------------------------------
// Input: -
// Return: 

void CLoggerInterface::FireEventToAllClientsMeasurementStopped ()
  {
  FireEventToAllClients(ABK_SVREVENT_MEASSTOPPED);
  }
  
//--------------------------------------------------------------------------
// FireEventToAllClientsFormClose() notifies clients to close form
// -----------------------------------------
// Input: -
// Return: 

void CLoggerInterface::FireEventToAllClientsFormClose (std::string formName)
  {
  FireEventToAllClients(ABK_SVREVENT_FORMCLOSE,formName,0,0);
  
  //FireEventToAllClients(ABK_SVREVENT_FORMCLOSE);
  }
  
//--------------------------------------------------------------------------
// FireEventToAllClientsVarlistChanged() notifies clients to reload varlist
// -----------------------------------------
// Input: -
// Return: 

void CLoggerInterface::FireEventToAllClientsVarlistChanged ()
  {
  FireEventToAllClients(ABK_SVREVENT_VARLISTCHANGED);
  m_bFirstRequest = true;
  }



//--------------------------------------------------------------------------
// Alert()            sends alert to client
// -------
// Input: pszVarName = name of variable
//                     NULL if none specified. The "Name" field will be
//                     omitted and dViolatingValue and strViolatingValue will be ignored
//        pszEventClass = class name of the event, e.g. KickDown
//                        NULL if no class specified. the "Class" field will be omitted
//        nPriority = priority, severity-level of the alert
//        tmSend = time when the event occured.
//                 if 0, the time will be inserted automatically from the local clock
//        bNoClientSideSuppress = false if client shall suppress displaying further alerts
//                                of pszEventClass/nPriority, if it supports this feature
//                                true if client shall not suppress displaying further alerts
//                                of pszEventClass/nPriority. Due to compatibility reasons,
//                                the client may ignore this field and it may suppress nevertheless
//        pJfAdditionalFields = additional fields. Formatter whos fields are inserted
//                              into the event data. NULL if no additional fields
//                              shall be inserted
//        pszViolatingValue = [in] value at the moment the violation occured, formatted
//                            if NULL, the value will be queried and formatted
//                            automatically. Note: This may be not desired if
//                            the value may have been changed meanwhile
//        dViolatingValue = value of variable at the moment the violation
//                          occured
// Return: -

void CLoggerInterface::Alert (const char *pszVarName, const char *pszEventClass, unsigned int nPriority, time_t tmSend, bool bNoClientSideSuppress, const CJsonFormatter *pJfAdditionalFields, const char *pszViolatingValue/*=NULL*/)
  {
  CJsonFormatter jfAlertInfo;  // {
  
  // insert event class name
  if(pszEventClass)
    jfAlertInfo.WriteValue(ABK_SVREVENT_ALERT_CLASS,pszEventClass); // "Class": "KickDown"
  
  // insert variable-specific items
  if(pszVarName)
    {
    jfAlertInfo.WriteValue(ABK_SVREVENT_ALERT_NAME,pszVarName); // "Name":
    if(pszViolatingValue)
      jfAlertInfo.WriteValue(ABK_SVREVENT_ALERT_VALUE,pszViolatingValue); // "Value":"somevalue"
    else
      {
      CVarRef *pVar=FindVariable(m_lstMeasVars,pszVarName);
      if(pVar)
        {
        jfAlertInfo.BeginMember(ABK_SVREVENT_ALERT_VALUE); // "Value":
        pVar->OnFormatValue(jfAlertInfo);                       // 123
        }
      }
    }

  // NoClientSideSuppress
  jfAlertInfo.WriteValue(ABK_SVREVENT_ALERT_NOCLISUP,bNoClientSideSuppress); // "_NoClientSideSuppress": true

  // insert additional fields
  if(pJfAdditionalFields)
    jfAlertInfo.WriteValue(*pJfAdditionalFields); // copy the fields of pJfAdditionalFields into the alert objects

  std::string strParam(jfAlertInfo.GetStream()->str()); // }
  FireEventToAllClients(0,tmSend,ABK_SVREVENT_ALERT,strParam,nPriority,0);
  }

void CLoggerInterface::Alert (const char *pszVarName, const char *pszEventClass, unsigned int nPriority, time_t tmSend, bool bNoClientSideSuppress, const CJsonFormatter *pJfAdditionalFields, double dViolatingValue)
  {
  char cValBuf[64];
  sprintf(cValBuf,"%g",dViolatingValue);
  Alert(pszVarName,pszEventClass,nPriority,tmSend,bNoClientSideSuppress,pJfAdditionalFields,cValBuf);
  }



//--------------------------------------------------------------------------
// AlertVarLimit()      alerts a variable value limit violation
// ---------------
// Input: pszVarName = name of variable
//                     NULL if none specified. The "Name" field will be
//                     omitted and dViolatingValue and strViolatingValue will be ignored
//        pszEventClass = class name of the event, e.g. KickDown
//                        NULL if no class specified. the "Class" field will be omitted
//        nPriority = priority, severity-level of the alert
//        tmSend = time when the event occured.
//                 if 0, the time will be inserted automatically from the local clock
//        pJfAdditionalFields = additional fields. Formatter whos fields are inserted
//                              into the event data. NULL if no additional fields
//                              shall be inserted
//        pszViolatingValue = [in] value at the moment the violation occured, formatted
//                            if NULL, the value will be queried and formatted
//                            automatically. Note: This may be not desired if
//                            the value may have been changed meanwhile
//        dViolatingValue = value of variable at the moment the violation
//                          occured
// Return: -

void CLoggerInterface::AlertVarLimit (const char *pszVarName, const char *pszEventClass, unsigned int nPriority, time_t tmSend, const CJsonFormatter *pJfAdditionalFields, const char *pszViolatingValue/*=NULL*/)
  {
  // assert(false); // depricated, use Alert() with bNoClientSideSuppress=false instead

  CJsonFormatter jfAlertInfo;  // {
  
  // insert event class name
  if(pszEventClass)
    jfAlertInfo.WriteValue(ABK_SVREVENT_LIMITALERT_CLASS,pszEventClass); // "Class": "KickDown"
  
  // insert variable-specific items
  if(pszVarName)
    {
    jfAlertInfo.WriteValue(ABK_SVREVENT_LIMITALERT_NAME,pszVarName); // "Name":
    if(pszViolatingValue)
      jfAlertInfo.WriteValue(ABK_SVREVENT_LIMITALERT_VALUE,pszViolatingValue); // "Value":"somevalue"
    else
      {
      CVarRef *pVar=FindVariable(m_lstMeasVars,pszVarName);
      if(pVar)
        {
        jfAlertInfo.BeginMember(ABK_SVREVENT_LIMITALERT_VALUE); // "Value":
        pVar->OnFormatValue(jfAlertInfo);                       // 123
        }
      }
    }

  // insert additional fields
  if(pJfAdditionalFields)
    jfAlertInfo.WriteValue(*pJfAdditionalFields); // copy the fields of pJfAdditionalFields into the alert objects

  std::string strParam(jfAlertInfo.GetStream()->str()); // }
  FireEventToAllClients(0,tmSend,ABK_SVREVENT_LIMITALERT,strParam,nPriority,0);
  }

void CLoggerInterface::AlertVarLimit (const char *pszVarName, const char *pszEventClass, unsigned int nPriority, time_t tmSend, const CJsonFormatter *pJfAdditionalFields, double dViolatingValue)
  {
  char cValBuf[64];
  sprintf(cValBuf,"%g",dViolatingValue);
  AlertVarLimit(pszVarName,pszEventClass,nPriority,tmSend,pJfAdditionalFields,cValBuf);
  }


//--------------------------------------------------------------------------
// NotifyAppChanged()      notifies all clients that an app/config file has changed
// ------------------
// call this function whenever an app or config file was changed. All clients
// will reload their apps/configs
// Input: -
// Return: -

void CLoggerInterface::NotifyAppChanged (void)
  {
  FireEventToAllClients(ABK_SVREVENT_APPCHANGED);
  }



//--------------------------------------------------------------------------
// RequestOpenForm()              fires event to all client: request to open a form
// -----------------
// Input: strFormName = name of form to open
// Return: -

void CLoggerInterface::RequestOpenForm (const char *pszFormName)
  {
  std::string strParam=pszFormName;
  FireEventToAllClients(ABK_SVREVENT_FORMREQUIRED,strParam,0,0);
  }


//--------------------------------------------------------------------------
// RequestCloseForm()      fires event to all client: request to close a form with a certain name
// ------------------
// Input: strFormName = name of form to be closed
// Return: -

void CLoggerInterface::RequestCloseForm (const char *pszFormName)
  {
  std::string strParam=pszFormName;
  FireEventToAllClients(ABK_SVREVENT_FORMCLOSE,strParam,0,0);
  }




//--------------------------------------------------------------------------
// MsgBox()                invokes message box
// --------
// Input: pszCaption = caption string
//        pszText = main text for the message box
//        nPriority = severity level
//        nID = general purpose ID, used to match confirmation events
//        nButtons = button flags, one or more of 
// Return: -

void CLoggerInterface::MsgBox (const char *pszCaption, const char *pszText, unsigned int nPriority, int nID, MSGBOX_BUTTON nButtons)
  {
  assert(pszCaption); // you have to specify a caption. at least, specify an empty string
  assert(pszText); // please specify a text for the message box
  assert(nButtons); // you have to specify at least one button, giving the user the chance to confirm

  static std::map<MSGBOX_BUTTON,const char *> s_mapButtonNames;
  if(s_mapButtonNames.size()==0)
    {
    s_mapButtonNames[MSGBOX_BUTTON_OK]          =ABK_SVREVENT_MSGBOX_BTN_OK;
    s_mapButtonNames[MSGBOX_BUTTON_OKPERMANENT] =ABK_SVREVENT_MSGBOX_BTN_OKPERMA;
    s_mapButtonNames[MSGBOX_BUTTON_YES]         =ABK_SVREVENT_MSGBOX_BTN_YES;
    s_mapButtonNames[MSGBOX_BUTTON_NO]          =ABK_SVREVENT_MSGBOX_BTN_NO;
    s_mapButtonNames[MSGBOX_BUTTON_RETRY]       =ABK_SVREVENT_MSGBOX_BTN_RETRY;
    s_mapButtonNames[MSGBOX_BUTTON_CANCEL]      =ABK_SVREVENT_MSGBOX_BTN_CANCEL;
    s_mapButtonNames[MSGBOX_BUTTON_IGNORE]      =ABK_SVREVENT_MSGBOX_BTN_IGNORE;
    s_mapButtonNames[MSGBOX_BUTTON_ABORT]       =ABK_SVREVENT_MSGBOX_BTN_ABORT;
    }

  CJsonFormatter jfInfo;  // {
  
  jfInfo.WriteValue(ABK_SVREVENT_MSGBOX_CAPTION,pszCaption); // "Caption": "A"
  jfInfo.WriteValue(ABK_SVREVENT_MSGBOX_TEXT,pszText); // "Text": "B"
  jfInfo.WriteValue(ABK_SVREVENT_MSGBOX_ID,nID); // "ID": "123"
  
  // insert buttons
  CJsonStreamArray jaButtons(&jfInfo,ABK_SVREVENT_MSGBOX_BUTTONS);
  for(std::map<MSGBOX_BUTTON,const char *>::const_iterator itButton=s_mapButtonNames.begin();itButton!=s_mapButtonNames.end();++itButton)
    {
    if(itButton->first&nButtons)
      jaButtons.WriteValue(itButton->second);
    }
  jaButtons.Close();

  std::string strParam(jfInfo.GetStream()->str()); // }
  FireEventToAllClients(ABK_SVREVENT_ALERT,strParam,nPriority,0);
  }



//--------------------------------------------------------------------------
// RequestAudioRec()       fires event for audio recording request display, fire to all clients
// -----------------
// Input: nId = general purpose ID which will be reflected in audio data from client
//        dMaxRecTime = maximum record time in s. Client shall terminate when this time is elapsed. 0 means no limit
// Return: -

void CLoggerInterface::RequestAudioRec (int nId, double dMaxRecTime)
  {
  FireEventToAllClients(ABK_SVREVENT_AUDIOREC_REQ,"",(double)nId,dMaxRecTime);
  }


//--------------------------------------------------------------------------
// StopAudioRec()       fires event to stop audio recording, fire to all clients
// --------------
// Input: nId = general purpose ID to be matched with the ID sent with RequestAudioRec
// Return: -

void CLoggerInterface::StopAudioRec (int nId)
  {
  FireEventToAllClients(ABK_SVREVENT_AUDIOREC_STOP,"",(double)nId,0);
  }


//--------------------------------------------------------------------------
// OnGetClientRole()       returns the role of a client
// -----------------
// this function shall be overwritten by specific the implementation to
// retrieve the role of a client by given class, type and serial number.
// Especially used in multi-display configurations.
// Input: strClass = device class e.g. "display"
//        strType = type, e.g. "mytronics_superdisplay3000"
//        strSerial = serial number of client
// Return: role string

/*virtual*/ std::string CLoggerInterface::OnGetClientRole (int nSession, const char *pszClass, const char *pszType, const char *pszSerial)
  {
  std::map<int, CSession *>::iterator iterSession=m_mapSessions.begin();
  for(;iterSession!=m_mapSessions.end();iterSession++)
    {
    CSession *pSession=iterSession->second;
    if(!pSession->m_clientUid.m_strClass.compare(pszClass)) // if session found for that class
      {
      if(iterSession->first==nSession) // if the client is the first of given class in the list of sessions
        return ABK_CLIENTROLE_PRIMARY; // it is the primary
      return ABK_CLIENTROLE_NOSPECIAL; // another session has this client class => the requested one is not the primary
      }
    }  
  return ABK_CLIENTROLE_NOSPECIAL;
  }



//--------------------------------------------------------------------------
// OnGetConnectionPreference() returns preference of a connection
// ---------------------------
// Input: strClass = device class e.g. "display"
//        strType = type, e.g. "mytronics_superdisplay3000"
//        strSerial = serial number of client, may also be MAC address
// Return: true if a connection to such a client is preferred, e.g. if 
//              it is stored in a configuration data base
//         false if connection is not preferred, e.g. not in the
//               configuration data base

/*virtual*/ bool CLoggerInterface::OnGetConnectionPreference (const char *pszClass, const char *pszType, const char *pszSerial) const
  {
  return false; // default implementaion does not support connection configuration database
  }




/** Returns list of available firmware fot a specific client
@note This virtual method gets called when the available client firmware information shall be gathered.
 In derived classed, there is no need to call the base class implementation.
@param pszClientClass class name of client requesting the firmware info
@param pszClientType type of client, typ. manufacturer and type merged string
@param lstGet list to return all available firmware images.
There is no need to empty the container since When this method is called, the list is guaranteed to be empty
@return true if handled, false if not handled since there is generally no client firmware support
*/
/*virtual*/ bool CLoggerInterface::OnGetClientFirmwareInfo (const char *pszClientClass, const char *pszClientType, std::list<CClientFirmware> &lstGet) const
{
  return false; // default implementation does not provide client firmware information
}




//--------------------------------------------------------------------------
// OnGetClientConfigInfo() returns client config file info
// -----------------------
// Input: strClientClass = class of client requesting the firmware info
//        strClientType = type of client, typ. manifacturer and type merged string
//        strClientSerial = serial number the client specified
//        cfgGet = [out] ref to return client configuration file info
//                 if no config file available, set the m_strUrl member to an
//                 empty string
// Return: true if sucessfully handled (even if no config file available and
//              the m_strUrl member was emptied)
//         false otherwise (on error or if feature is not supported)

/*virtual*/ bool CLoggerInterface::OnGetClientConfigInfo (const char *pszClientClass, const char *pszClientType, const char *pszClientSerial, CClientConfig &cfgGet) const
  {
  return false; // default implementation does not provide client config file information
  }




//--------------------------------------------------------------------------
// FormWriteInput()        writes an input form field
// ----------------
// Input: jaDest = json array formatter where the control will be formatted to
//        strName = name of the contorl
//        strCaption = caption of the control
//        strInitialValue = initial value of the control
//        nMaxLen = maximum input length, -1 if no limitation
//        bReadOnly = if true, no changes can be made
//        bPassword = if true, no readable display of the control
//        bUpdateable = if true, control can be updated by re-loading the form
// Return: -

/*static*/ void CLoggerInterface::FormWriteInput (CJsonStreamArray &jaDest, const char *pszName, const char *pszCaption, const char *pszInitialValue, int nMaxLen/*=-1*/, bool bReadOnly/*=false*/, bool bPassword/*=false*/, bool bUpdateable/*=false*/)
  {
  CJsonStreamObject joControl(&jaDest); // {
  joControl.WriteValue(ABK_RSP_FORMS_CONTROLTYPE,ABK_RSP_FORMS_CONTROLTYPE_INPUT);
  joControl.WriteValue(ABK_RSP_FORMS_CONTROL_NAME,pszName);
  joControl.WriteValue(ABK_RSP_FORMS_CONTROL_CAPTION,pszCaption);
  joControl.WriteValue(ABK_RSP_FORMS_CONTROL_INITIALVALUE,pszInitialValue);
  if(nMaxLen>=0)
    joControl.WriteValue(ABK_RSP_FORMS_CONTROL_MAXLEN,nMaxLen);
  joControl.WriteValue(ABK_RSP_FORMS_CONTROL_READONLY,bReadOnly);
  joControl.WriteValue(ABK_RSP_FORMS_CONTROL_PASSWORD,bPassword);
  joControl.WriteValue(ABK_RSP_FORMS_CONTROL_UPDATEABLE,bUpdateable); // write the updateable attribute
  } // }


//--------------------------------------------------------------------------
// FormWriteCheckbox()     Constructor of FormWriteCheckbox
// -------------------
// Input: jaDest = json array formatter where the control will be formatted to
//        strName = name of the contorl
//        strCaption = caption of the control
//        strInitialValue = initial value of the control
// Return: -

/*static*/ void CLoggerInterface::FormWriteCheckbox (CJsonStreamArray &jaDest, const char *pszName, const char *pszCaption, bool bInitialValue, bool bUpdateable/*=false*/)
  {
  CJsonStreamObject joControl(&jaDest); // {
  joControl.WriteValue(ABK_RSP_FORMS_CONTROLTYPE,ABK_RSP_FORMS_CONTROLTYPE_CHECKBOX);
  joControl.WriteValue(ABK_RSP_FORMS_CONTROL_NAME,pszName);
  joControl.WriteValue(ABK_RSP_FORMS_CONTROL_CAPTION,pszCaption);
  joControl.WriteValue(ABK_RSP_FORMS_CONTROL_INITIALVALUE,bInitialValue);
  joControl.WriteValue(ABK_RSP_FORMS_CONTROL_UPDATEABLE,bUpdateable); // write the updateable attribute
  } // }


//--------------------------------------------------------------------------
// FormWriteList()         writes a list form field
// ---------------
// Input: jaDest = json array formatter where the control will be formatted to
//        strName = name of the contorl
//        strCaption = caption of the control
//        strInitialValue = initial value of the control
//        lstOptions = list of options for the list box
// Return: -

/*static*/ void CLoggerInterface::FormWriteList (CJsonStreamArray &jaDest, const char *pszName, const char *pszCaption, int nInitialValue, std::list<std::string> &lstOptions, bool bUpdateable/*=false*/)
  {
  CJsonStreamObject joControl(&jaDest); // {
  joControl.WriteValue(ABK_RSP_FORMS_CONTROLTYPE,ABK_RSP_FORMS_CONTROLTYPE_COMBO);
  joControl.WriteValue(ABK_RSP_FORMS_CONTROL_NAME,pszName);
  joControl.WriteValue(ABK_RSP_FORMS_CONTROL_CAPTION,pszCaption);
  joControl.WriteValue(ABK_RSP_FORMS_CONTROL_INITIALVALUE,nInitialValue);
  joControl.WriteValue(ABK_RSP_FORMS_CONTROL_UPDATEABLE,bUpdateable); // write the updateable attribute
  if(1)
    {
    CJsonStreamArray jaOptions(&joControl,ABK_RSP_FORMS_CONTROL_OPTIONS); // [
    std::list<std::string>::iterator iterOptions;
    for(iterOptions=lstOptions.begin();iterOptions!=lstOptions.end();iterOptions++)
      jaOptions.WriteValue(*iterOptions); // write each option
    } // jaOptions falls out of scope => ]
  } // }


//--------------------------------------------------------------------------
// FormWriteButton()       writes a button form field
// -----------------
// Input: jaDest = json array formatter where the control will be formatted to
//        strName = name of the contorl
//        strCaption = caption of the control
//        bSubmit = if true, the button is a submit button
//        bCancel = if true, the button is a cancel button
// Return: -

/*static*/ void CLoggerInterface::FormWriteButton (CJsonStreamArray &jaDest, const char *pszName, const char *pszCaption, bool bSubmit/*=false*/, bool bCancel/*=false*/, bool bUpdateable/*=false*/)
  {
  CJsonStreamObject joControl(&jaDest); // {
  joControl.WriteValue(ABK_RSP_FORMS_CONTROLTYPE,ABK_RSP_FORMS_CONTROLTYPE_BUTTON);
  joControl.WriteValue(ABK_RSP_FORMS_CONTROL_NAME,pszName);
  joControl.WriteValue(ABK_RSP_FORMS_CONTROL_CAPTION,pszCaption);
  joControl.WriteValue(ABK_RSP_FORMS_CONTROL_SUBMIT,bSubmit);
  joControl.WriteValue(ABK_RSP_FORMS_CONTROL_CANCEL,bCancel);
  joControl.WriteValue(ABK_RSP_FORMS_CONTROL_UPDATEABLE,bUpdateable); // write the updateable attribute
  } // }







//--------------------------------------------------------------------------
// FormatStatistics()      formats characteristics into an json object
// ------------------
// Input: joDump = 
// Return: 

void CLoggerInterface::CClientUid::FormatStatistics (CJsonStreamObject &joDump) const
  {
  joDump.WriteValue("Id",m_nSession);
  joDump.WriteValue("ClientAddr",m_strAddress);
  joDump.WriteValue("Class",m_strClass);
  joDump.WriteValue("Type",m_strType);
  joDump.WriteValue("Serial",m_strSerial);
  }



//--------------------------------------------------------------------------
// OnLogAdded()          notifies that the log queue has got new entities. may be called in any thread context!
// ---------             Overwrite this function to pick the log messages with PopLog()
//                       Note that this function can be in any thread context
// Input: -
// Return: -

/*virtual*/ void CLoggerInterface::OnLogAdded (void)
  {
  // in derived classes, do not call the base class implementation

  LOGSEVERITY nSeverity;
  std::string strError;
  for(;;)
    {
    BOOL bPopSuccess=PopLog(nSeverity,strError); // get one log message
    if(!bPopSuccess) // if the log was empty, ready with popping messages
      break;

    ; // discard it. On derived classes, put the message to somwhere else
    }
  }



//--------------------------------------------------------------------------
// AddLog(), AddLogV            writes one line to error log
// ----------
// Input: nSeverity = [in] severity, one of LOGSEVERITY_xxx
//        pszMessage = [in] message format string, like printf
//        args = [in] optional arguments
// Return: -

void CLoggerInterface::AddLog (LOGSEVERITY nSeverity, const char *pszMessage, ...)
  {
  va_list args;
  va_start(args,pszMessage);
  AddLogV(nSeverity,pszMessage,args);
  va_end(args);
  }


void CLoggerInterface::AddLogV (LOGSEVERITY nSeverity, const char *pszMessage, va_list args)
  {
  assert(this); // called with a NULL instance pointer??
  assert(pszMessage); // you have to specify a message
  m_queueLog.AddV((CLogQueue<char>::LOGSEVERITY)nSeverity,pszMessage,args);
  OnLogAdded(); // notify about changes in error log
  }


//--------------------------------------------------------------------------
// PopLog()                pops one entity from the error log. can be called from any thread context
// --------
// Input: nSeverityGet = [out] returns severity
//        strMessageGet = [out] copies message string to this string
// Return: TRUE if message could be popped, FALSE if queue is empty

BOOL CLoggerInterface::PopLog (LOGSEVERITY &nSeverityGet, std::string &strMessageGet)
  {
  BOOL bSuccess=m_queueLog.Pop((CLogQueue<char>::LOGSEVERITY &)nSeverityGet,strMessageGet);
  return bSuccess;
  }



} // namespace


