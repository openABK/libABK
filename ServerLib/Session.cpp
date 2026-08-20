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
// Filename:    Session.cpp
// Created:     2012-08-17 (05:13)
// Author:      D. Burger
// Description: ABK session, used as counterpart for each connected client
//------------------------------------------------------------------------------------------------


#include "stdafx.h"

#include <assert.h>
#include <map>

#include "Session.h"
#include "ValuesFromSpec.h"
#include "LoggerIf.h"


#define ABK_SESSIONLOCK_TIMEOUT 1000 // timeout for session access

using namespace std;

namespace Abk {


//--------------------------------------------------------------------------
// CSession()           Constructor of CSession
// -------------
// Input: pOwner = owning logger interface
//        uidClient = identification data of the client
// Return: -

CSession::CSession (CLoggerInterface *pOwner, const CLoggerInterface::CClientUid &uidClient)
  : m_queueEvents(ABK_RSP_SERVEREVENT_EVENTS)
  {
  assert(pOwner); // a session must be always bound to a logger interface
  m_pOwner=pOwner;
  m_pOwner->AddLog(m_pOwner->LOGSEVERITY_TRACE,"Creating session %d",uidClient.m_nSession);
  m_nRemainingS=ABK_SESSION_LIFETIME;
  m_bRequestInProgress=false;
  m_bFirstPoll = true;
  m_clientUid=uidClient;
  m_bExpThreadInProgress=true;
  AbkStartThread(MaintainExpirationThread,this);
  }



//--------------------------------------------------------------------------
// ~CSession()          Destructor of CSession
// --------------
// Input: -
// Return: 

/*virtual*/ CSession::~CSession ()
  {
  m_pOwner->AddLog(m_pOwner->LOGSEVERITY_TRACE,"Deleting the session %d",GetSessionId());
  m_eventSleep.Set(); // let the expiration maintainence thread terminate
  CLoggerInterface *pIfLogger=GetOwner();
  assert(pIfLogger);
  pIfLogger->LockSessions(ABK_SESSIONLOCK_TIMEOUT);
  bool bUnregisterSuccess;
  bUnregisterSuccess=pIfLogger->UnregisterSession(this);
  assert(bUnregisterSuccess);
  pIfLogger->UnlockSessions();
  while(m_bExpThreadInProgress) // wait for the thread to terminate
    AbkSleepMs(10);

  // delete the daqs
  std::map<std::string,CDaq *>::iterator iterDaq;
  for(iterDaq=m_mapDaq.begin();iterDaq!=m_mapDaq.end();++iterDaq)
    delete iterDaq->second;
  }



//--------------------------------------------------------------------------
// GetDaqList()            returns pointer to a order-list
// ------------
// Input: strListName = name of list to be returned
// Return: pointer to list, NULL if list with given name doesnt exist

CDaq *CSession::GetDaq (const char *pszListName) const
  {
  std::map<std::string,CDaq *>::const_iterator iterDaq;
  CAbkSingleLock lock(&m_mutex, true, ABK_SESSIONLOCK_TIMEOUT);  
  iterDaq=m_mapDaq.find(pszListName);
  if(iterDaq==m_mapDaq.end()) // if not found
    return NULL;
  return iterDaq->second;
  }


//--------------------------------------------------------------------------
// PutDaqList()          adds an empty order list to the session or return an existing
// --------------
// Input: strListName = name of the list. must not be empty
// Return: pointer to the newly created order list, or pointer to an existing with matching name
//         null if existing daq is not of type CDaqList

CDaqValues *CSession::PutDaqList (const char *pszListName, PFN_CREATE_DAQ pfn_create)
  {
  CAbkSingleLock lock(&m_mutex, true, ABK_SESSIONLOCK_TIMEOUT);  
  assert(strlen(pszListName));
  CDaqValues *pResult;
  CDaq *pExisting=GetDaq(pszListName); // get the already existing one
  if(pExisting)
    {
    pResult=dynamic_cast<CDaqValues *>(pExisting);
    if(pResult)
      return pResult; // return the existing daq list
    assert(false); // tried to insert a daq list while another object with differnt type (daq vs trend) is already in the list
    }
  pair<map<std::string,CDaq *>::iterator,bool> iterInsert; // result of the insert operation
  pResult=pfn_create();
  iterInsert=m_mapDaq.insert(pair<std::string,CDaq *>(pszListName,pResult));
  assert(iterInsert.second);
  pResult->Create(this,pszListName,ABK_DAQ_CYCLE_DEFAULT);
  return pResult;
  }




//--------------------------------------------------------------------------
// PutDaqTrend()           adds an empty daq trend to the session or returns an existing
// -------------
// Input: strListName = name of the daq trend. must not be empty
// Return: pointer to the newly created daq trend, or pointer to an existing with matching name
//         null if existing daq is not of type CDaqTrend

CDaqTrend *CSession::PutDaqTrend (const char *pszListName)
  {
  CAbkSingleLock lock(&m_mutex, true, ABK_SESSIONLOCK_TIMEOUT);  
  assert(strlen(pszListName));
  CDaqTrend *pResult;
  CDaq *pExisting=GetDaq(pszListName); // get the already existing one
  if(pExisting)
    {
    pResult=dynamic_cast<CDaqTrend *>(pExisting);
    if(pResult)
      return pResult; // return the existing daq list
    assert(false); // tried to insert a daq trend while another object with differnt type is already in the list
    }
  pair<map<std::string,CDaq *>::iterator,bool> iterInsert; // result of the insert operation
  pResult=new CDaqTrend(pszListName);
  iterInsert=m_mapDaq.insert(pair<std::string,CDaq *>(pszListName,pResult));
  assert(iterInsert.second);
  pResult->Create(this,pszListName,ABK_DAQ_CYCLE_DEFAULT);
  return pResult;
  }



//--------------------------------------------------------------------------
// DeleteDaqList()       deletes an order-list
// -----------------
// Input: strListName = name of list to be deleted
// Return: true if deleted, false if not found

bool CSession::DeleteDaqList (const char *pszListName)
  {
  m_pOwner->AddLog(m_pOwner->LOGSEVERITY_TRACE,"Deleting DAQ list \"%s\" from session %d",pszListName,GetSessionId());

  CAbkSingleLock lock(&m_mutex, true, ABK_SESSIONLOCK_TIMEOUT);
  std::map<std::string,CDaq *>::iterator iterDaq;
  iterDaq=m_mapDaq.find(pszListName);
  if(iterDaq==m_mapDaq.end()) // if not found
    {
    m_pOwner->AddLog(m_pOwner->LOGSEVERITY_ERROR,"The DAQ list \"%s\" could not be found for deleting",pszListName);
    return false;
    }
  CDaq *pDaq=iterDaq->second;  // pointer the daq list
  assert(pDaq);
  assert(!pDaq->m_strName.compare(pszListName)); // is the find() method wrong?
  m_mapDaq.erase(iterDaq); // delete in the map
  delete pDaq; // delete the daq itself
  return true;
  }


//--------------------------------------------------------------------------
// DeleteDaqLists()       deletes all daq lists
// -----------------
// Return: true

bool CSession::DeleteDaqLists ()
  {
  m_pOwner->AddLog(m_pOwner->LOGSEVERITY_TRACE,"Deleting all DAQ lists of session %d",GetSessionId());
  
  CAbkSingleLock lock(&m_mutex, true, ABK_SESSIONLOCK_TIMEOUT);  
  for(std::map<std::string,CDaq *>::iterator iterDaq=m_mapDaq.begin(); iterDaq != m_mapDaq.end(); ++iterDaq)
    {
    CDaq *pDaq=iterDaq->second;  // pointer the daq list
    m_mapDaq.erase(iterDaq); // delete in the map
    delete pDaq; // delete the daq itself
    }
  return true;
  }
  
  
//--------------------------------------------------------------------------
// Refresh()               refreshes the session timeout
// ---------
// Input: -
// Return: 

void CSession::Refresh (void)
  {
  Lock(ABK_SESSIONLOCK_TIMEOUT);
  m_nRemainingS=ABK_SESSION_LIFETIME;
  Unlock();
  }


//--------------------------------------------------------------------------
// Sleep()                 sleeps or returns if session shall terminate
// -------
// Input: nTimeMs = sleep time in ms
// Return: true if sleep succeeded, false if interface shall terminate

bool CSession::Sleep (int nTimeMs)
  {
  return !m_eventSleep.Wait(nTimeMs);
  }


//--------------------------------------------------------------------------
// MaintainExpirationThread() thread maintaining session timeouts
// --------------------------
// Input: pArg = pointer to the session object
// Return: always 0

/*static*/ unsigned int THREAD_CALLCONV CSession::MaintainExpirationThread (void *pArg)
  {
  assert(pArg);
  CSession *pThis=reinterpret_cast<CSession *>(pArg);
  for(;;)
    {
    int nRemainingS;
    if(!pThis->Lock(ABK_SESSIONLOCK_TIMEOUT))
      continue; // got no access to the session
    pThis->m_nRemainingS--;
    nRemainingS=pThis->m_nRemainingS;
    assert(nRemainingS>=0);
    pThis->Unlock();
    assert(nRemainingS>=0);
    if(!nRemainingS) // if life time expired
      {
      assert(!pThis->m_bRequestInProgress); // when a request is in progress, the session cant expire by design
      pThis->m_bExpThreadInProgress=false; // thread is about to die
      delete pThis;
      break;
      }
    if(!pThis->Sleep(1000)) // if logger session terminated from outside
      {
      pThis->m_bExpThreadInProgress=false; // thread is about to die
      break;
      }
    }
  return 0;
  }


//--------------------------------------------------------------------------
// WaitEvent()             waits for event, returns false on timeout
// -----------
// Input: nTimeoutMs = timeout in ms
// Return: true if event occured, false if timed-out

bool CSession::WaitEvent (int nTimeoutMs)
  {
  return m_event.Wait(nTimeoutMs);
  }


//--------------------------------------------------------------------------
// FireEvent()             fires the event to trigger thread waiting in WaitEvent
// -----------
// Input: -
// Return: -

void CSession::FireEvent (void)
  {
  m_event.Set();
  m_event.Reset();
  }


//--------------------------------------------------------------------------
// IsFirstPoll()          returns whether this is the first call to this function
// --------------
// Input: -
// Return: first call?

bool CSession::IsFirstPoll(void )
{
	bool retval = m_bFirstPoll;
	m_bFirstPoll = false;
	return retval;
}


//--------------------------------------------------------------------------
// FormatStatistics()      dumps proerties into json stream
// ------------------
// Input: pDump = stream to dump to
// Return: 

void CSession::FormatStatistics (CJsonStreamObject *pDump)
  {
  if(1)
    {
    CAbkSingleLock guard(&m_mutex,true,ABK_SESSIONLOCK_TIMEOUT);
    m_clientUid.FormatStatistics(*pDump);
    pDump->WriteValue("Expires",m_nRemainingS);
    }

  if(1)
    {
    CJsonStreamArray jaDataList(pDump,"DataLists");
    CAbkSingleLock guard(&m_mutex,true,ABK_SESSIONLOCK_TIMEOUT);
    std::map <std::string,CDaq *>::iterator iterDaq=m_mapDaq.begin();
    for(iterDaq=m_mapDaq.begin();iterDaq!=m_mapDaq.end();iterDaq++)
      {
      CJsonStreamObject joDaqList(&jaDataList);
      CDaq *pDaqList=iterDaq->second; // get the daq
      pDaqList->FormatStatistics(&joDaqList);
      }
    }
  }


//--------------------------------------------------------------------------
// FireEventToClient()       fires an event to the client
// -----------------
// Input: nSender = session id of sender
//        tmSent = time at the senders clock in local time
//        strEventType = type of event
//        strClientRole = actual role of the client, e.g. ABK_CLIENTROLE_PRIMARY ("primary")
//        strParam = general purpose string parameter
//        dParam1 = general purpose numeric param
//        dParam2 = general purpose numeric param
//        bForcePrimaryRole = (added on 04. July 2014 D. Burger)
//                            true if role shall be primary regardless of the
//                            state of the session
//                            false if role shall be determined by state of
//                            the session
// Return: -

void CSession::FireEventToClient (int nSender, time_t tmSent, const char *pszEventType, const std::string &strParam, double dParam1, double dParam2, bool bForcePrimaryRole/*=false*/)
  {
  CAbkSingleLock lock(&m_mutex,true,ABK_SESSIONLOCK_TIMEOUT);
  CJsonStreamObject joEvent(m_queueEvents.GetArrayForFeeding()); // format an object
  std::string strClientRole; // role of the client
  if(!bForcePrimaryRole) // rormal role assignment
    strClientRole=m_pOwner->OnGetClientRole(GetSessionId(),m_clientUid.m_strClass.c_str(),m_clientUid.m_strType.c_str(),m_clientUid.m_strSerial.c_str()); // get the role of the target client out of the system configuration database
  else // in all cases, respond primary
    strClientRole=ABK_CLIENTROLE_PRIMARY; // always respond with primary role

  // write the data
  joEvent.WriteValue(ABK_RSP_SERVEREVENT_SENDER  ,nSender);
  joEvent.WriteValue(ABK_RSP_SERVEREVENT_TIME    ,tmSent);
  joEvent.WriteValue(ABK_RSP_SERVEREVENT_TYPE    ,pszEventType);
  joEvent.WriteValue(ABK_RSP_SERVEREVENT_ROLE    ,strClientRole);
  joEvent.WriteValue(ABK_RSP_SERVEREVENT_STRPARAM,strParam);
  joEvent.WriteValue(ABK_RSP_SERVEREVENT_PARAM1  ,dParam1);
  joEvent.WriteValue(ABK_RSP_SERVEREVENT_PARAM2  ,dParam2);

  FireEvent(); // pending requests will now be answered
  }





} // namespace

