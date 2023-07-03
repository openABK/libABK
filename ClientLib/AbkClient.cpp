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
// |  __| |   ´   ||  -  /| |  | |  __  \___ \ | | | |/ __|
// | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
// |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
//                                              |___/      
// Filename:    AbkClient.cpp
// Created:     2012-08-02 (07:48)
// Author:      D. Burger
// Description: client provinding api to an ABK server
//------------------------------------------------------------------------------------------------



#include "stdafx.h"

#include <assert.h>
#include <list>
#include <limits>
#include "AbkClient.h"
#include "AbkClientVar.h"
#include "AbkClientDaq.h"
#include "AbkServerEvent.h"

#include "JsonParserAtl.h"
#include "JsonFormatter.h"
#include "ValuesFromSpec.h"
#include <WinInet.h> // gives the HTTP status codes
#include <afxmt.h>

#include "Stopwatch.h"

#ifdef WINCE
#include <WceExtensions.h>
#endif

#define MIME_TYPE_TEXT "text/plain"
#define MIME_TYPE_JSON "application/json"

#define DAQ_TIMEOUT 10000 // mutex timeout in ms
#define ERRORLOG_TIMEOUT 1000 // mutex timeout for error log access

#define LONGPOLL_FAILURE_TOLERANCE_MS 2000 // maximum time span in ms where errors are tolerated
#define LONGPOLL_FAILURE_RECOVERY_MS 100 // 500 // time the longpoll thread is stalled after an http error occured. Used to reduce error log entry rate



#ifdef WINCE
  #define HTTP_PIPELINING
#else
  // #define HTTP_PIPELINING
#endif
#ifdef HTTP_PIPELINING
  #define LOCKED_SECTION_NAV(intention) // no locking, multiple threads can send requests interleaved
#else
  #define LOCKED_SECTION_NAV(intention) CLockMyCriticalSection lockNav(m_csNavigate,_T("")); // allow only one thread to access either aux or longpoll section
#endif

using namespace std;
using namespace ATL;




namespace Abk
  {



  /** Thread which requests in order to generate a little traffic  
  @param vpThis Pointer to the temporary connection object
  @return always 0
  */
  /*static*/ DWORD WINAPI CAbkClient::CTempConnection::RequestThreadS(void* vpThis)
  {
    CTempConnection* pThis = static_cast<CTempConnection*>(vpThis);
    for (size_t nRequest = 0; nRequest < REQUEST_COUNT; ++nRequest)
    {
      pThis->m_pClient->NavigateGet(_T(ABK_REQUESTURL_CURRENTTIME), -1);
    }
    pThis->m_hThread = NULL;
    return 0;
  }

  /** Constructor  
  @param pClient http client used to send the requests
  */
  CAbkClient::CTempConnection::CTempConnection(CBaseAbstraction* pClient)
    : m_hThread(NULL)
    , m_pClient(pClient)
  {
    m_hThread = CreateThread(NULL, 0, RequestThreadS, this, 0, NULL);
  }

  /** dtor
  */
  CAbkClient::CTempConnection::~CTempConnection()
  {
    while (m_hThread)
      Sleep(1);
  }





//--------------------------------------------------------------------------
// Delete()                deletes the client
// --------
// Input: -
// Return: TRUE on success of if no object was created before,
//         FALSE if wait until no more usage of the client timed-out

BOOL CAbkClient::CClientPtr::Delete (void)
  {
  if(!m_pClient)
    return TRUE; // successfully deleted nothing
  BOOL bSuccess=FALSE;
  for(int nRetry=0;nRetry<300;nRetry++)
    {
    CLockMyCriticalSection lock(m_csUsage,_T("Abk::CAbkClient::CClientPtr::Delete()"));
    if(m_nUsage==0)
      {
      delete m_pClient;
      m_pClient=NULL;
      bSuccess=TRUE;
      break;
      }
    Sleep(10);
    }
  ASSERT(m_nUsage==0);
  return bSuccess;
  }










/** Constructor of CAbkClient
@param bSuppressLeading If true, http requests wont emit log file entries
@param bTextTranslationByServer true requests the server to translate values in text, if applicable. false instructs the server to send non-translated values
*/
CAbkClient::CAbkClient (bool bSuppressLog /*= false*/, bool bTextTranslationByServer /*=true*/)
: m_evLongPollEnable(FALSE,TRUE) // use as manual-reset event
, m_bTextTranslationByServer (bTextTranslationByServer)
{
  //m_pClientAux=NULL;
  //m_pClientEvent=NULL;
  m_bSuppressLog = bSuppressLog;
  m_hLongPollThread = NULL;
  m_bTerminateLongPoll = false;
  m_pNextEventData = NULL; // default: no event buffer
  m_nSessionId = -1;
}




//--------------------------------------------------------------------------
// ~CAbkClient()           Destructor of CAbkClient
// -------------
// Input: -
// Return: 

/*virtual*/ CAbkClient::~CAbkClient ()
  {
  TidyUp(false);
  }




/** creates the client and initializes
@param pszServerAddress server IP address. This string can be volatile since it will be stored internally
@param nPort port to connect to
@param pEventRxBuffer pointer to event buffer for next event reception.
 This can be either a buffer or the first element of an event queue.
 If NULL, no long polling thread will be created
@param pszClientClass class name of the client. If NULL or an empty string, no session will be obtained
@param pszClientType type name of the client. If NULL or an empty string, no session will be obtained
@param pszClientSerial serial number or id of the client
@param pszClientFwRev firmware revision string. NULL if not known
@param pszClientHwRev hardware revision string. NULL if not known
@return true on success, false on error
*/
bool CAbkClient::Create (LPCTSTR pszServerAddress, int nPort, CAbkServerEvent *pEventRxBuffer, LPCTSTR pszClientClass, LPCTSTR pszClientType, LPCTSTR pszClientSerial, LPCTSTR pszClientFwRev /*=NULL*/, LPCTSTR pszClientHwRev /*=NULL*/)
{
  ASSERT (pszClientSerial && pszClientSerial[0]); // the serial number is mandatory, since this device is identified when saving or loading client states!!
  bool bSuccess = false;
  TidyUp (false);
  m_strServerAddress = pszServerAddress;
  m_nPort = nPort;
  if (pszClientClass)
    m_strClientClass = pszClientClass;
  else
    m_strClientClass.Empty ();
  if (pszClientType)
    m_strClientType = pszClientType; // regular client type, except when querying firmware info
  else
    m_strClientType.Empty ();
  if (pszClientSerial)
    m_strClientSerial = pszClientSerial;
  else
    m_strClientSerial.Empty ();
  if (pszClientFwRev)
    m_strClientFwRev = pszClientFwRev;
  else
    m_strClientFwRev.Empty ();
  if (pszClientHwRev)
    m_strClientHwRev = pszClientHwRev;
  else
    m_strClientHwRev.Empty ();

  m_pNextEventData = pEventRxBuffer; // events will be stored here. After TidyUp() the long-polling thread is stopped and it is safe to change this pointer

  if (m_nPort > 0)
  {
    // create clients
    m_pClientAux = new CBaseAbstraction (this);
    m_pClientEvent = new CBaseAbstraction (this);
    CClientPtrRef pClientAux (m_pClientAux);
    CClientPtrRef pClientEvent (m_pClientEvent);
    pClientAux->SetServerAddr (m_strServerAddress, nPort);
    pClientEvent->SetServerAddr (m_strServerAddress, nPort);
    pClientAux->SetTimeout (ABK_AUX_MAXRESPONSE_MS);
    pClientEvent->SetTimeout (ABK_LONGPOLL_MAXRESPONSE_MS + (ABK_LONGPOLL_MAXRESPONSE_MS / 2)); // + ABK_LONGPOLL_MAXRESPONSE_MS / 2 as reserve

    // get a session id
    if (!m_strClientClass.IsEmpty () && !m_strClientType.IsEmpty ())
    {
      // tentative
      if(1)
      {
        for (size_t nTemp = 0; nTemp < 2; ++nTemp)
        {
          CBaseAbstraction *pClient = m_pClientAux.GetPtr();
          CTempConnection connTemp[3] = { CTempConnection(pClient), CTempConnection(pClient), CTempConnection(pClient) };
        }
      }

      m_nSessionId = pClientAux->ObtainSessionId (m_strClientClass, m_strClientType, m_strClientSerial, m_strClientFwRev, m_strClientHwRev);
      if (m_nSessionId >= 0)
      {
        // start the long-polling thread
#ifndef STRIPDOWN_LONGPOLL_THREAD
        if (m_pNextEventData)
        {
          m_evLongPollEnable.ResetEvent (); // do not initially stall long polling
          m_evLongPollDone.ResetEvent ();
          m_bTerminateLongPoll = false;
          m_hLongPollThread = CreateThread (NULL, 0, LongPollThreadS, this, 0, NULL);
        }
#endif
        bSuccess = true;
      }
    }
    else
    {
      m_nSessionId = -1;
      bSuccess = true;
    }
  }

  return bSuccess;
}




/** returns true if connected to a server
@return true if connected to a server
*/
bool CAbkClient::IsConnected (void) const
{
  bool bConnected = false;
  if ((m_nPort > 0) && (m_nSessionId > 0))
  {
    CClientPtrRefConst a (m_pClientAux);
    CClientPtrRefConst e (m_pClientEvent);
    bConnected = a.IsValid () && e.IsValid () && (a->GetPort () > 0) && (e->GetPort () > 0);
  }
  return bConnected;
}




//--------------------------------------------------------------------------
// TidyUp()                cleans object
// --------
// Input: bLostConnection: true, if no more connection is available and remote objects
//                              shall not be tidied-up
//                         false, if connection is assumed to be available
//                                and remote objects shall be tidied-up via http
// Return: -

void CAbkClient::TidyUp (bool bLostConnection)
  {
  bool bLongPollSelfTerminated=m_bTerminateLongPoll; // if true, indicates that the longpoll thread terminated itself due to an error

  // stop the long-polling thread
  if(1)
    {
    CClientPtrRef pClientAux(m_pClientAux);
    CClientPtrRef pClientEvent(m_pClientEvent);
    if(pClientEvent.IsValid())
      {
      m_bTerminateLongPoll=true;
#ifndef WINCE      
      SOCKET s=pClientEvent->GetSocket(); // 12.Jan.15 added for EMBU-Sketch preventing in asserting in pClientEvent->Close()
      closesocket(s);
#endif
Sleep(20);
      m_evLongPollEnable.SetEvent(); // in case the long-poll-thread is stalled, wake it up so it can terminate
      if (m_hLongPollThread)
        WaitForSingleObject (m_evLongPollDone.m_hObject, LONGPOLL_FAILURE_TOLERANCE_MS * 2); // wait until terminated
      pClientEvent->Close(); // close connection so request of long-polling gets interrrupted
      m_bTerminateLongPoll=false;
      }

    if(bLostConnection)
      {
      if(pClientAux.IsValid())
        pClientAux->SetServerAddr(_T(""),0); // inhibit further requests since connection is dead
      if(pClientEvent.IsValid())
        pClientEvent->SetServerAddr(_T(""),0);
      }

    // tidy-up aux items
    if(pClientAux.IsValid())
      {
      // delete the daqs
      std::map<std::string,CAbkClientDaq *>::iterator iterDaq;
      for(iterDaq=m_mapDaq.begin();iterDaq!=m_mapDaq.end();++iterDaq)
        {
        if(bLongPollSelfTerminated) // if it is likely that the server is inresponsive..
          iterDaq->second->m_pOwner=NULL; // prevent the daq from deleting at the server
        delete iterDaq->second;
        }
      m_mapDaq.clear();

      // delete session
      if(IsConnected() && !bLongPollSelfTerminated)
        pClientAux->DeleteSession(m_nSessionId);

      pClientAux->Close();
      }

    }

  // finally delete the clients
  m_pClientEvent.Delete();
  m_pClientAux.Delete();

  m_strServerAddress="";
  m_nPort=0;
  m_nSessionId=-1;
  }


//--------------------------------------------------------------------------
// SetServerAddr()         re-assigns the server address and port
// ---------------
// Input: strServerAddress = server address, e.g. "192.168.178.22". may be volatile buffer
//        nPort = port for connection with server, e.g. 8080
// Return: -

void CAbkClient::SetServerAddr (LPCTSTR pszServerAddress, int nPort)
  {
  if((m_nPort!=nPort)||(m_strServerAddress.Compare(pszServerAddress))) // if changes in address or port
    {
    Create (pszServerAddress, nPort, m_pNextEventData, m_strClientClass, m_strClientType, m_strClientSerial, m_strClientFwRev, m_strClientHwRev);
    }
  }


//--------------------------------------------------------------------------
// GetPort()               returns port of server connection
// ---------
// Input: -
// Return: port of connection

int CAbkClient::GetServerPort (void) const
  {
  //CClientPtrRefConst pClientAux(m_pClientAux);
  //if(pClientAux.IsValid())
    return m_nPort; // return const_cast<CAtlNavigateData &>(pClientAux->m_nav).GetPort();
  //return 0;
  }


//--------------------------------------------------------------------------
// GetCurrentServerTime()        retrieves the current time of the server as local time
// ----------------------
// Input: pGet = pointer to return the server time, returned in client local time
// Return: true on success, false on error

bool CAbkClient::GetCurrentServerTime (time_t *pGet)
  {
  bool bSuccess=FALSE;
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(pClientAux.IsValid())
    {
    if(const char *pReturn=pClientAux->NavigateGet(_T(ABK_REQUESTURL_CURRENTTIME),-1))
      {
      CJsonParser parsResponse(pReturn);
      for(;!parsResponse.IsDone();++parsResponse)
        {
        bSuccess=parsResponse.ExtractValue(ABK_RSP_CURRENTTIME_TIME,pGet);
        if(bSuccess)
          break;
        }
      if(!bSuccess)
        {
        AddLog(LOGSEVERITY_ERROR,_T("Error: No date included in the answer of %s"),_T(ABK_REQUESTURL_CURRENTTIME));
        }
      }
    }
  return bSuccess;
  }



//--------------------------------------------------------------------------
// GetStorageInfo()        requests storage info of the server (storage for logging data)
// ----------------
// Input: rTotal = [out] ref to return the total capacity in bytes
//        rFree = [out] ref to return the free capacity in bytes
// Return: true on success, false on error

bool CAbkClient::GetStorageInfo (unsigned long long &rTotal, unsigned long long &rFree)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  const char *pReturn=pClientAux->NavigateGet(_T(ABK_REQUESTURL_STORAGEINFO),-1);
  if(!pReturn)
    return false;
  CJsonParser parsResponse(pReturn);
  double dTotal=0.;
  double dFree=0.;
  bool bSuccessTotal=false;
  bool bSuccessFree=false;
  for(;!parsResponse.IsDone();++parsResponse)
    {
    if(!bSuccessTotal)
      bSuccessTotal=parsResponse.ExtractValue(ABK_RSP_STORAGEINFO_TOTAL,&dTotal);
    if(!bSuccessFree)
      bSuccessFree=parsResponse.ExtractValue(ABK_RSP_STORAGEINFO_FREE,&dFree);
    if(bSuccessFree && bSuccessTotal) // if collected all neccessary information..
      {
      rTotal=(unsigned long long)dTotal;
      rFree=(unsigned long long)dFree;
      return true; // .. done
      }
    }
  CString strError;
  strError.Format(_T("Error: in the answer of %s."),_T(ABK_REQUESTURL_CURRENTTIME));
  if(!bSuccessTotal)
    strError.AppendFormat(_T(" Field %s is missing."),_T(ABK_RSP_STORAGEINFO_TOTAL));
  if(!bSuccessFree)
    strError.AppendFormat(_T(" Field %s is missing."),_T(ABK_RSP_STORAGEINFO_FREE));
  AddLog(LOGSEVERITY_ERROR,strError);
  return false;
  }


//--------------------------------------------------------------------------
// GetLastModified()       returns the last modified date of a file by given url
// -----------------
// Input: strUrl = url of file to be examined
//        pGet = pointer to return the last modified date
// Return: true on success, false on error

bool CAbkClient::GetLastModified (LPCTSTR pszUrl, CTime *pGet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  return pClientAux->GetLastModified(pszUrl,pGet);
  }





//--------------------------------------------------------------------------
// SendEvent()             sends a client event to the server
// -----------
// Input: strEventType = envent type string
//        strStringParam = string parameter
//        jfString = formatted object to be sent as string parameter
//        dParam1 = numeric parameter 1
//        dParam2 = numeric parameter 2
//        bPrivate: if true, client intends to process the reflected event
//                  by itself and the server shall not reflect the event
//                  to other clients
// Return: true on success, false on error

bool CAbkClient::SendEvent (const char *pszEventType, LPCTSTR pszStringParam, double dParam1, double dParam2, bool bPrivate)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  CJsonFormatter jfEvent; // whole event formatted in json
  jfEvent.WriteValue(ABK_RSP_CLIENTEVENT_SENDER,m_nSessionId);
#ifdef WINCE
  time_t tmNow=time(NULL); // get actual time
#else
  time_t tmNow;
  time(&tmNow); // get actual time
#endif
  jfEvent.WriteValue(ABK_RSP_CLIENTEVENT_TIME,tmNow);
  jfEvent.WriteValue(ABK_RSP_CLIENTEVENT_TYPE,pszEventType);
  jfEvent.WriteValue(ABK_RSP_SERVEREVENT_STRPARAM,CT2A(pszStringParam,CP_UTF8));
  jfEvent.WriteValue(ABK_RSP_SERVEREVENT_PARAM1,dParam1);
  jfEvent.WriteValue(ABK_RSP_SERVEREVENT_PARAM2,dParam2);
  jfEvent.WriteValue(ABK_RSP_CLIENTEVENT_PRIVATE,bPrivate);
  jfEvent.Close();
CStopwatch watch;
watch.Start();
  bool bSuccess=NULL!=pClientAux->NavigatePut(_T(ABK_REQUESTURL_CLIENTEVENT),-1,&jfEvent);
watch.Stop();
watch.OutputDebugTimeMs(_T("SendEvent"));
  return bSuccess;
  }

bool CAbkClient::SendEvent (const char *pszEventType, CJsonFormatter &jfString, double dParam1, double dParam2, bool bPrivate)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  CJsonFormatter jfEvent; // whole event formatted in json
  jfEvent.WriteValue(ABK_RSP_CLIENTEVENT_SENDER,m_nSessionId);
#ifdef WINCE
  time_t tmNow=time(NULL); // get actual time
#else
  time_t tmNow;
  time(&tmNow); // get actual time
#endif
  jfEvent.WriteValue(ABK_RSP_CLIENTEVENT_TIME,tmNow);
  jfEvent.WriteValue(ABK_RSP_CLIENTEVENT_TYPE,pszEventType);
  jfEvent.WriteValue(ABK_RSP_SERVEREVENT_STRPARAM,jfString.GetStream()->str().c_str());
  jfEvent.WriteValue(ABK_RSP_SERVEREVENT_PARAM1,dParam1);
  jfEvent.WriteValue(ABK_RSP_SERVEREVENT_PARAM2,dParam2);
  jfEvent.WriteValue(ABK_RSP_CLIENTEVENT_PRIVATE,bPrivate);
  jfEvent.Close();
  return NULL!=pClientAux->NavigatePut(_T(ABK_REQUESTURL_CLIENTEVENT),-1,&jfEvent);
  }



//--------------------------------------------------------------------------
// SendButtonEvent()           sends a button press/release event to the server
// -----------------
// Input: strButtonName = name of the button
//        bPressedState = TRUE if button is pressed, FALSE if released
//        nTime = time value of key event.
//                positive values indicate the time since key was pressed in ms
//                negative values indicate the time since key was released in ms
//        bPrivate: if true, client intends to process the reflected event
//                  by itself and the server shall not reflect the event
//                  to other clients
// Return: true on success, false on error

bool CAbkClient::SendButtonEvent (LPCTSTR pszButtonName, bool bPressedState, int nTime, bool bPrivate)
  {
  return SendEvent(ABK_CLIENTEVENT_BUTTON,pszButtonName,(double)(bPressedState!=0),(double)nTime,bPrivate);
  }



//--------------------------------------------------------------------------
// SendAlertConfirmEvent() sends confirmation event to server: user has confirmed an event
// -----------------------
// Input: strAlertClassName = class name of the alert, e.g. KickDown
//        nSeverity = severity level of the alert
//        nMerged = number of merged (into one widget) alerts the user confirms
//        bPermanent = TRUE if user whishes to confirm permanently
//        bSuppressed = true if alert was suppressed to user and confirmation
//                      was generated automatically
//        bTimeout = TRUE if widget closed automatically after timeout
//                   FALSE if user explicitely confirmed
// Return: true on success, false on error

bool CAbkClient::SendAlertConfirmEvent (LPCTSTR pszAlertClassName, int nSeverity, int nMerged, bool bPermanent, bool bSuppressed, bool bTimeout)
  {
  assert(this);
  assert((!bSuppressed) || (bSuppressed && !bTimeout)); // if suppressed, timeout must not be set! Please check how you call the function
  CJsonFormatter jfSend;
  std::string strClassA=CT2A(pszAlertClassName,CP_UTF8);
  jfSend.WriteValue(ABK_ALERTCONFIRM_CLASS,strClassA.c_str()); // "Class": "KickDown"
  jfSend.WriteValue(ABK_ALERTCONFIRM_SEVERITY,nSeverity); // "Severity": 3
  jfSend.WriteValue(ABK_ALERTCONFIRM_COUNT,nMerged); // "Merged": 5
  jfSend.WriteValue(ABK_ALERTCONFIRM_SUPPRESSED,bSuppressed); // "Suppressed": false
  jfSend.WriteValue(ABK_ALERTCONFIRM_TIMEOUT,bTimeout); // "Timeout": false
  jfSend.WriteValue(ABK_ALERTCONFIRM_PERMASUPPRBYUSER,bPermanent); // "PermanentSuppressedByUser": false
  jfSend.Close();
  return SendEvent(ABK_CLIENTEVENT_ALERT_CONFIRM,jfSend,0,0,false);
  }


//--------------------------------------------------------------------------
// GetVarValue()           queries a variable value
// -------------
// Input: pszVarName = name of variable
//        pGet = pointer to return the variable value
// Return: true on success, false on error

bool CAbkClient::GetVarValue (LPCTSTR pszVarName, CString *pGet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  std::string strVarValue;  
  bool bSuccess=pClientAux->GetVarOrMailboxValue(_T(ABK_REQUESTURL_VARVALUE),CT2A(pszVarName,CP_UTF8),&strVarValue);
  *pGet=CA2T(strVarValue.c_str(),CP_UTF8);
  return bSuccess;
  }

bool CAbkClient::GetVarValue (LPCTSTR pszVarName, std::string *pGet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  return pClientAux->GetVarOrMailboxValue(_T(ABK_REQUESTURL_VARVALUE),CT2A(pszVarName,CP_UTF8),pGet);  
  }

bool CAbkClient::GetVarValue (LPCTSTR pszVarName, double *pGet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  return pClientAux->GetVarOrMailboxValue(_T(ABK_REQUESTURL_VARVALUE),CT2A(pszVarName,CP_UTF8),pGet);  
  }

bool CAbkClient::GetVarValue (LPCTSTR pszVarName, int *pGet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  return pClientAux->GetVarOrMailboxValue(_T(ABK_REQUESTURL_VARVALUE),CT2A(pszVarName,CP_UTF8),pGet);  
  }

bool CAbkClient::GetVarValue (LPCTSTR pszVarName, bool *pGet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  return pClientAux->GetVarOrMailboxValue(_T(ABK_REQUESTURL_VARVALUE),CT2A(pszVarName,CP_UTF8),pGet);  
  }

bool CAbkClient::GetVarValue (LPCTSTR pszVarName, CTime *pGet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  time_t tmGet;
  return pClientAux->GetVarOrMailboxValue(_T(ABK_REQUESTURL_VARVALUE),CT2A(pszVarName,CP_UTF8),&tmGet);
  *pGet=tmGet;
  }



//--------------------------------------------------------------------------
// GetMailboxValue()       queries a mailbox value
// -----------------
// Input: pszMailboxName = name of mailbox
//        pGet = pointer to return the mailbox value
// Return: 

bool CAbkClient::GetMailboxValue (LPCTSTR pszMailboxName, CString *pGet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  std::string strMailboxValue;  
  bool bSuccess=pClientAux->GetVarOrMailboxValue(_T(ABK_REQUESTURL_MAILBOXVALUE),CT2A(pszMailboxName,CP_UTF8),&strMailboxValue);
  *pGet=strMailboxValue.c_str();
  return bSuccess;
  }

bool CAbkClient::GetMailboxValue (LPCTSTR pszMailboxName, std::string *pGet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  return pClientAux->GetVarOrMailboxValue(_T(ABK_REQUESTURL_MAILBOXVALUE),CT2A(pszMailboxName,CP_UTF8),pGet);  
  }

bool CAbkClient::GetMailboxValue (LPCTSTR pszMailboxName, double *pGet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  return pClientAux->GetVarOrMailboxValue(_T(ABK_REQUESTURL_MAILBOXVALUE),CT2A(pszMailboxName,CP_UTF8),pGet);  
  }

bool CAbkClient::GetMailboxValue (LPCTSTR pszMailboxName, int *pGet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  return pClientAux->GetVarOrMailboxValue(_T(ABK_REQUESTURL_MAILBOXVALUE),CT2A(pszMailboxName,CP_UTF8),pGet);  
  }

bool CAbkClient::GetMailboxValue (LPCTSTR pszMailboxName, bool *pGet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  return pClientAux->GetVarOrMailboxValue(_T(ABK_REQUESTURL_MAILBOXVALUE),CT2A(pszMailboxName,CP_UTF8),pGet);  
  }

bool CAbkClient::GetMailboxValue (LPCTSTR pszMailboxName, CTime *pGet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  time_t tmGet;
  return pClientAux->GetVarOrMailboxValue(_T(ABK_REQUESTURL_MAILBOXVALUE),CT2A(pszMailboxName,CP_UTF8),&tmGet);
  *pGet=tmGet;
  }



//--------------------------------------------------------------------------
// SetVarValue()           sets a variable value
// -------------
// Input: pszVarName = name of variable to be set
//        pSet = pointer to new value
// Return: 

bool CAbkClient::SetVarValue (LPCTSTR pszVarName, const CString &strSet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  std::string strValue=CT2A(strSet);
  return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_VARVALUE),CT2A(pszVarName,CP_UTF8),&strValue);
  }

bool CAbkClient::SetVarValue (LPCTSTR pszVarName, const std::string &strSet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_VARVALUE),CT2A(pszVarName,CP_UTF8),&strSet);
  }

bool CAbkClient::SetVarValue (LPCTSTR pszVarName, const double dSet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_VARVALUE),CT2A(pszVarName,CP_UTF8),&dSet);
  }

bool CAbkClient::SetVarValue (LPCTSTR pszVarName, const int nSet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_VARVALUE),CT2A(pszVarName,CP_UTF8),&nSet);
  }

bool CAbkClient::SetVarValue (LPCTSTR pszVarName, const bool bSet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_VARVALUE),CT2A(pszVarName,CP_UTF8),&bSet);
  }

bool CAbkClient::SetVarValue (LPCTSTR pszVarName, const CTime &tmSet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  struct tm tmLocal;
  tmSet.GetLocalTm(&tmLocal);
  time_t tmtSet=mktime(&tmLocal);
  return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_VARVALUE),CT2A(pszVarName,CP_UTF8),&tmtSet);
  }



//--------------------------------------------------------------------------
// SetMailboxValue()       sets a mailbox value
// -----------------
// Input: pszMailboxName = name of the mailbox to be set
//        pSet = pointer to new value
// Return: 

bool CAbkClient::SetMailboxValue (LPCTSTR pszMailboxName, const CString &strSet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  std::string strValue=CT2A(strSet);
  return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_MAILBOXVALUE),CT2A(pszMailboxName,CP_UTF8),&strValue);
  }

bool CAbkClient::SetMailboxValue (LPCTSTR pszMailboxName, const std::string &strSet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_MAILBOXVALUE),CT2A(pszMailboxName,CP_UTF8),&strSet);
  }

bool CAbkClient::SetMailboxValue (LPCTSTR pszMailboxName, const double dSet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_MAILBOXVALUE),CT2A(pszMailboxName,CP_UTF8),&dSet);
  }

bool CAbkClient::SetMailboxValue (LPCTSTR pszMailboxName, const int nSet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_MAILBOXVALUE),CT2A(pszMailboxName,CP_UTF8),&nSet);
  }

bool CAbkClient::SetMailboxValue (LPCTSTR pszMailboxName, const bool bSet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_MAILBOXVALUE),CT2A(pszMailboxName,CP_UTF8),&bSet);
  }

bool CAbkClient::SetMailboxValue (LPCTSTR pszMailboxName, const CTime &tmSet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  struct tm tmLocal;
  tmSet.GetLocalTm(&tmLocal);
  time_t tmtSet=mktime(&tmLocal);
  return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_MAILBOXVALUE),CT2A(pszMailboxName,CP_UTF8),&tmtSet);
  }




/** requests meta data of variables
@param vectVarNames list with variable names
@param pGet[out] pointer to return the meta data.Data will be appended rather than overwritten
@return true on success, false on error
*/
bool CAbkClient::GetVarMeta (const std::vector<LPCTSTR> &vectVarNames, std::vector<CAbkClientMeta> *pGet)
  {
  bool bSuccess=false;
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(pClientAux.IsValid())
    bSuccess=pClientAux->GetVarOrMailboxMeta(vectVarNames,pGet,false);
  return bSuccess;
  }



//--------------------------------------------------------------------------
// GetVarMeta()            requests meta data of a variable
// ------------
// Input: pszVarName = name of variable to be queried
//        pGet = pointer to return the meta data
// Return: true on success, false on error

bool CAbkClient::GetVarMeta (LPCTSTR pszVarName, CAbkClientMeta *pGet)
  {
  std::vector<LPCTSTR> vectVarNames;
  std::vector<CAbkClientMeta> vectMeta;
  vectVarNames.push_back(pszVarName); // compose a list with one entity
  bool bSuccess=GetVarMeta(vectVarNames,&vectMeta); // request the meta data
  assert(vectMeta.size()==1);
  if(bSuccess)
    *pGet=*vectMeta.begin();
  return bSuccess;
  }




/** requests meta data of mailboxes
@param vectVarNames list with variable names
@param pGet[out] pointer to return the meta data.Data will be appended rather than overwritten
@return true on success, false on error
*/
bool CAbkClient::GetMailboxMeta (const std::vector<LPCTSTR> &vectMailboxNames, std::vector<CAbkClientMeta> *pGet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  return pClientAux->GetVarOrMailboxMeta(vectMailboxNames,pGet,true);
  }


//--------------------------------------------------------------------------
// GetMailboxMeta()        requests meta data of a mailbox
// ----------------
// Input: pszMailboxName = name of mailbox to be queried
//        pGet = pointer to return the meta data
// Return: true on success, false on error

bool CAbkClient::GetMailboxMeta (LPCTSTR pszMailboxName, CAbkClientMeta *pGet)
  {
  std::vector<LPCTSTR> vectVarNames;
  std::vector<CAbkClientMeta> vectMeta;
  vectVarNames.push_back(pszMailboxName); // compose a list with one entity
  bool bSuccess=GetMailboxMeta(vectVarNames,&vectMeta); // request the meta data
  assert(vectMeta.size()==1);
  if(bSuccess)
    *pGet=*vectMeta.begin();
  return bSuccess;
  }


//--------------------------------------------------------------------------
// GetVarList()            requests list of variables
// ------------
// Input: pGet = pointer to list recieving the variable names
// Return: true on success, false on error

bool CAbkClient::GetVarList (std::vector<CString> *pGet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  return pClientAux->GetVarOrMailboxList(_T(ABK_REQUESTURL_VARLIST),pGet);
  }


//--------------------------------------------------------------------------
// GetMailboxList()        requests list of mailboxes
// ----------------
// Input: pGet = list recieving the mailbox names
// Return: 

bool CAbkClient::GetMailboxList (std::vector<CString> *pGet)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  return pClientAux->GetVarOrMailboxList(_T(ABK_REQUESTURL_MAILBOXLIST),pGet);
  }



//--------------------------------------------------------------------------
// GetForm()               requests a form
// ---------
// Input: pszFormName = name of the form to be requested
//        vectGet = list of form elements
//        strCaptionGet = string reference to get the caption of the form
//        nPersitenceMs = ref to return the desired form persistence
//                        time in ms. 0 means infinite
// Return: true on success, false on error

bool CAbkClient::GetForm (LPCTSTR pszFormName, std::vector<CFormElement> &vectGet, CString &strCaptionGet, int &nPersitenceMs)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;
  CString strUrl;
  strUrl.Format(_T("%s/%s"),_T(ABK_SERVICE_FORMS),pszFormName);
  const char *pszResponse=pClientAux->NavigateGet(strUrl,-1);
  if(!pszResponse)
    return false;
  CJsonParserAtl jpForm(pszResponse);
  bool bCaptionDecoded=false;
  nPersitenceMs=0; // default: infinite display time
  for(;!jpForm.IsDone();++jpForm)
    {
    bCaptionDecoded|=jpForm.ExtractValueAtl(ABK_RSP_FORMS_CAPTION,strCaptionGet); // get caption of the form
    jpForm.ExtractValue(ABK_RSP_FORMS_PERSISTENCE,&nPersitenceMs); // get persistence time
    if(jpForm.TestArray(ABK_RSP_FORMS_CONTROLS)) // is there "Controls":[
      {
      for(++jpForm;!jpForm.IsDone();++jpForm) // each element
        {
        CFormElement elGet;
        if(!elGet.DecodeJson(jpForm)) // decode the element
          return false; // error in element
        vectGet.push_back(elGet);
        }
      }
    jpForm.SkipItem();
    }
  if(!bCaptionDecoded)
    {
    AddLog(LOGSEVERITY_ERROR,_T("Caption is missing in form \"%s\""),pszFormName);
    return false;
    }
  return true;
  }


//--------------------------------------------------------------------------
// SendForm()              sends a form
// ----------
// Input: pszFormName = name of form to be sent
//        lstSend = list of elements to be sent. only the m_varValue with
//                  the corresponding names are sent
// Return: 

bool CAbkClient::SendForm (LPCTSTR pszFormName, const std::vector<CFormElement> &vectSend)
  {
  ASSERT(pszFormName);
  // assert(m_pClientAux);

  CClientPtrRef pClientAux(m_pClientAux);
  if(!pClientAux.IsValid())
    {
    AddLog(LOGSEVERITY_ERROR,_T("Tried to send the filled form \"%s\" but connection to server was lost in the meanwhile."),pszFormName);
    return false;
    }
  bool bSuccess=true;
  CJsonFormatter jfForm;  // {
  for(std::vector<CFormElement>::const_iterator itElement=vectSend.begin();itElement!=vectSend.end();++itElement)
    {
    const CFormElement *pElement=&*itElement;
    switch(pElement->m_varValue.vt)
      {
      case VT_I2:
        jfForm.WriteValue(CT2A(pElement->m_strName,CP_UTF8),(int)pElement->m_varValue.iVal); // "Elementname":123
        break;
      case VT_I4:
        jfForm.WriteValue(CT2A(pElement->m_strName,CP_UTF8),(int)pElement->m_varValue.lVal); // "Elementname":123
        break;
      case VT_INT:
        jfForm.WriteValue(CT2A(pElement->m_strName,CP_UTF8),(int)pElement->m_varValue.intVal); // "Elementname":123
        break;
      case VT_R8:
        jfForm.WriteValue(CT2A(pElement->m_strName,CP_UTF8),pElement->m_varValue.dblVal); // "Elementname":1.23
        break;
      case VT_BSTR:
        jfForm.WriteValue(CT2A(pElement->m_strName,CP_UTF8),(char *)(CW2A(pElement->m_varValue.bstrVal,CP_UTF8))); // "Elementname":"string"
        break;
      case VT_BOOL:
        jfForm.WriteValue(CT2A(pElement->m_strName,CP_UTF8),(bool)(pElement->m_varValue.boolVal!=0)); // "Elementname":true
        break;
      case VT_EMPTY:
        jfForm.WriteValue(CT2A(pElement->m_strName,CP_UTF8),numeric_limits<double>::quiet_NaN()); // null
        break;
      default:
        assert(false); // encountered an unimplemented variant type
        bSuccess=false;
      }
    }
  jfForm.Close(); // }
  if(!bSuccess)
    return false;
  
  // send form
  CString strUrl;
  strUrl.Format(_T("%s/%s"),_T(ABK_SERVICE_FORMS),pszFormName);
  if(!pClientAux->NavigatePut(strUrl,-1,&jfForm))
    return false;
  return true;
  }


//--------------------------------------------------------------------------
// GetClientState()       reads client configuration from server
// -----------------
// Input: strFileExtension = file extension of data set to be requested, with dot delimiter
//                           e.g. ".ini" or ".reg" or ".xml"
// Return: pointer to null-terminated configuration string
//         NULL on fatal error or server responds not with 200

const char *CAbkClient::GetClientState (LPCTSTR pszFileExtension)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  //assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return NULL;
  CString strUrl;
  assert(pszFileExtension);
  assert(pszFileExtension[0]!='\0'); // please no empty extension
  assert(pszFileExtension[0]=='.'); // extension must start with delimiter dot
  strUrl.Format(_T("%s/%s_%s_%s%s"),_T(ABK_SERVICE_CLIENTSTATES),m_strClientClass,m_strClientType,m_strClientSerial,pszFileExtension);
  const char *pszResponse=pClientAux->NavigateGet(strUrl,-1); // read data
  int nStatus=pClientAux->GetStatus();
  if(nStatus!=HTTP_STATUS_OK) // if not responded with OK (200)..
    pszResponse=NULL; // .. devalidate the result (which is typically empty)
  return pszResponse;
  }


//--------------------------------------------------------------------------
// SetClientState()       writes client configuration to server
// -----------------
// Input: pConfigString = configuration string with configuration settings, null-terminted
//        strFileExtension = file extension the data shall be stored, with dot delimiter
//                           e.g. ".ini" or ".reg" or ".xml"
// Return: true on success (all went OK and the server responded with a 2xx code
//         false on fatal error or if server responed with a non 2xx code

bool CAbkClient::SetClientState (const char *pConfigString, LPCTSTR pszFileExtension)
  {
  bool bSuccess=false;
  CClientPtrRef pClientAux(m_pClientAux);
  if(pClientAux.IsValid() && IsConnected())
    {
    CString strUrl;
    assert(pszFileExtension);
    assert(pszFileExtension[0]!='\0'); // please no empty extension
    assert(pszFileExtension[0]=='.'); // extension must start with delimiter dot
    strUrl.Format(_T("%s/%s_%s_%s%s"),_T(ABK_SERVICE_CLIENTSTATES),m_strClientClass,m_strClientType,m_strClientSerial,pszFileExtension);
    bool bSuccess=NULL!=pClientAux->NavigatePut(strUrl,-1,pConfigString,(int)strlen(pConfigString),_T(MIME_TYPE_TEXT));
    if(bSuccess)
      {
      int nStatus=pClientAux->GetStatus();
      if((nStatus<200) || (nStatus>=300)) // if not responded with an OK-code (2xx)..
        bSuccess=false; // .. error in writing at the server
      }
    }
  return bSuccess;
  }



//--------------------------------------------------------------------------
// GetClientConfigInfo()      queries the client configuration/app information
// ---------------------
// Input: strUrl = [out] URL where to download the configuration file. if no file
//                 is provided, this string will get empty
//        strMd5 = [out] MD5 of the file, only valid if strUrl is not empty
//        pszClientType=NULL = [in, optional] client type name when querying
//                             the info. If NULL, the standard client type which
//                             was specified in Create() will be used
// Return: true on success, even if no config file is available and the strUrl was
//              emptied
//         false on error or if server does not support client config hosting

bool CAbkClient::GetClientConfigInfo (CString &strUrl, CString &strMd5, LPCTSTR pszClientType/*=NULL*/)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  //assert(pClientAux.IsValid());
  if(!pClientAux.IsValid())
    return false;

  // compose and send request
  if(!pszClientType) // if no client type name specified, use the stored one
    pszClientType=(LPCTSTR)m_strClientType;
  CJsonFormatter jfReq;
  jfReq.WriteValue(ABK_REQ_CLIENTCONFIG_CLASS,CT2A(m_strClientClass,CP_UTF8));
  jfReq.WriteValue(ABK_REQ_CLIENTCONFIG_TYPE,CT2A(pszClientType,CP_UTF8));
  jfReq.WriteValue(ABK_REQ_CLIENTCONFIG_SERIAL,CT2A(m_strClientSerial,CP_UTF8));
  const char *pszResponse=pClientAux->NavigatePost(_T(ABK_REQUESTURL_CLIENTCONFIG_INFO),-1,&jfReq); // send own info and get config file info
  if(pszResponse==NULL)
    return false;

  // decode response
  CJsonParserAtl jpResp(pszResponse);
  bool bUrlDecoded=false;
  bool bMd5Decoded=false;
  for(;!jpResp.IsDone();++jpResp)
    {
    bUrlDecoded|=jpResp.ExtractValueAtl(ABK_RSP_CLIENTCONFIG_URL,strUrl); // get URL
    bMd5Decoded|=jpResp.ExtractValueAtl(ABK_RSP_CLIENTCONFIG_MD5,strMd5); // get MD5 hash
    }
  if(!bUrlDecoded)
    {
    AddLog(LOGSEVERITY_ERROR,_T("ClientConfig info response: URL field is missing"));
    return false;
    }
  if(!strUrl.IsEmpty() && (!bMd5Decoded || strMd5.IsEmpty())) // if there was an URL returned, a valid MD5 must be there too
    {
    AddLog(LOGSEVERITY_ERROR,_T("ClientConfig info response: MD5 field is missing"));
    return false;
    }
  return true;
  }


//--------------------------------------------------------------------------
// GetClientFirmwareInfo() queries the available client firmware information
// -----------------------
// Input: vectGet = list to return info to all firmware files
//        pszClientType=NULL = [in, optional] client type name when querying
//                             the info. If NULL, the standard client type which
//                             was specified in Create() will be used
// Return: true on success (even if emtpy list returned), FALSE on error

bool CAbkClient::GetClientFirmwareInfo (std::vector<CFirmwareInfo> &vectGet, LPCTSTR pszClientType/*=NULL*/)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  if(!pClientAux.IsValid())
    return false;
  bool bSuccess=true;
  vectGet.clear();

  // compose and send request
  if(!pszClientType) // if no client type name specified, use the stored one
    pszClientType=(LPCTSTR)m_strClientType;
  CJsonFormatter jfReq;
  jfReq.WriteValue(ABK_REQ_FIRMWARE_CLASS,CT2A(m_strClientClass,CP_UTF8));
  jfReq.WriteValue(ABK_REQ_FIRMWARE_TYPE,CT2A(pszClientType,CP_UTF8));
  jfReq.WriteValue("Serial",CT2A(m_strClientSerial,CP_UTF8)); // send serial unsolicitedly
  const char *pszResponse=pClientAux->NavigatePost(_T(ABK_REQUESTURL_FIRMWARE),-1,&jfReq); // send own info and get list of firmware files file info
  if(pszResponse==NULL)
    return false;

  // decode response
  CJsonParserAtl jpResp(pszResponse);
  bool bAnyVersionOmitted=false; // if we found at least one entity without version info
  for(;!jpResp.IsDone();++jpResp)
    {
    if(jpResp.TestArray(ABK_RSP_FIRMWARE_IMAGELIST)) // is there "Images":[
      {
      CFirmwareInfo fwi;
      bool bUrlDecoded=false;
      bool bMd5Decoded=false;
      bool bVersionDecoded=false;
      for(;!jpResp.IsDone();++jpResp)
        {
        bUrlDecoded     |=jpResp.ExtractValueAtl(ABK_RSP_FIRMWARE_URL,fwi.m_strUrl); // get URL
        bMd5Decoded     |=jpResp.ExtractValueAtl(ABK_RSP_FIRMWARE_MD5,fwi.m_strMd5); // get MD5 hash
        bVersionDecoded |=jpResp.ExtractValueAtl(ABK_RSP_FIRMWARE_VERSION,fwi.m_strVersion); // get version info
        }
      if(!bVersionDecoded)
        bAnyVersionOmitted=true;
      if(bUrlDecoded && bMd5Decoded) // at least the server has to fill in these fields
        {
        vectGet.push_back(fwi);
        }
      else
        {
        if(!bUrlDecoded)
          AddLog(LOGSEVERITY_ERROR,_T("GetClientFwInfo: Firmware info response: URL field is missing"));
        if(!bMd5Decoded)
          AddLog(LOGSEVERITY_ERROR,_T("GetClientFwInfo: Firmware info response: MD5 field is missing"));
        bSuccess=FALSE;
        }
      jpResp.SkipItem();
      }
    }
  if(bAnyVersionOmitted && vectGet.size()>1) // if more than one entity returned and a version field was omitted
    {
    AddLog(LOGSEVERITY_ERROR,_T("GetClientFwInfo: Firmware info response: Version field is missing while returning multiple entities"));
    bSuccess=FALSE;
    }
  if(!bSuccess)
    vectGet.clear(); // discard decoded content if an error occured
  return bSuccess;
  }


//--------------------------------------------------------------------------
// DownloadFile()          downloads a file, used to load client config or firmware
// --------------
// Input: pszUrl = url on the server where do load from
//        pszStorePath = absolute local file path where to store to
//        pfnReadCallback = callback called for status updates during download
//        dwCookie = callback data
// Return: true on success, false if error occured

bool CAbkClient::DownloadFile (LPCTSTR pszUrl, LPCTSTR pszStorePath, PFNATLSTATUSCALLBACK pfnReadCallback/*=NULL*/, DWORD_PTR dwCookie/*=0*/)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  if(!pClientAux.IsValid())
    return false;
  bool bSuccess=false;
  bSuccess=pClientAux->NavigateGet(pszUrl,-1,pszStorePath,pfnReadCallback,dwCookie);
  if (!bSuccess)
    DeleteFile (pszStorePath);
  return bSuccess;
  }


//--------------------------------------------------------------------------
// DownloadFile()          downloads a file
// --------------
// Input: pszUrl = url on the server where do load from
//        fileStore = already opened file where to store the data to. The file
//                    is not flushed, so DownloadFile may be used to append to
//                    the file
//        pfnReadCallback = callback called for status updates during download
//        dwCookie = callback data
// Return: true on success, false if error occured

bool CAbkClient::DownloadFile (LPCTSTR pszUrl, CFile &fileStore, PFNATLSTATUSCALLBACK pfnReadCallback/*=NULL*/, DWORD_PTR dwCookie/*=0*/)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  if(!pClientAux.IsValid())
    return false;
  bool bSuccess=false;
  bSuccess=pClientAux->NavigateGet(pszUrl,-1,&fileStore,pfnReadCallback,dwCookie);
  return bSuccess;
  }



//--------------------------------------------------------------------------
// GetServerInfo()         returns server information as json formatted string
// ---------------
// Input: -
// Return: pointer to server information in json format
//         NULL on error
//         the pointer is valid until the next aux-request to the server
//         it shall not be freed nor deleted!

const char *CAbkClient::GetServerInfo (void)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid()); // no connection with the aux http client established
  if(!pClientAux.IsValid())
    return NULL;
  //if(!IsConnected()) // commented-out since it needs no connection since no session neccessary
  //  return NULL;
  return pClientAux->NavigateGet(_T(ABK_REQUESTURL_SERVERINFO),-1);
  }


//--------------------------------------------------------------------------
// GetServerInfo()         retrieves information from server
// ---------------
// Input: strProtocolVersion = 
//        strInterfaceVersion = 
//        strFwVersion = 
//        strHwVersion = 
//        strServerName = 
//        strServerType = 
//        strDescUrl = 
// Return: 

bool CAbkClient::GetServerInfo (CString &strProtocolVersion, CString &strInterfaceVersion, CString &strFwVersion, CString &strHwVersion, CString &strServerName, CString &strServerType, CString &strDescUrl)
  {
  const char *pszServerInfoJson; // json formatted server information
  pszServerInfoJson=GetServerInfo();
  if(!pszServerInfoJson)
    return FALSE;
  CJsonParserAtl jp(pszServerInfoJson);

  // empty all return data
  strProtocolVersion.Empty();
  strInterfaceVersion.Empty();
  strFwVersion.Empty();
  strHwVersion.Empty();
  strServerName.Empty();
  strServerType.Empty();
  strDescUrl.Empty();
  
  bool bSuccess=true;
  for(;!jp.IsDone();++jp)
    {
    jp.ExtractValueAtl(ABK_RSP_SERVERINFO_PROTOVERSION,strProtocolVersion);
    jp.ExtractValueAtl(ABK_RSP_SERVERINFO_IFVERSION,strInterfaceVersion);
    jp.ExtractValueAtl(ABK_RSP_SERVERINFO_FWVERSION,strFwVersion);
    jp.ExtractValueAtl(ABK_RSP_SERVERINFO_HWVERSION,strHwVersion);
    jp.ExtractValueAtl(ABK_RSP_SERVERINFO_NAME,strServerName);
    jp.ExtractValueAtl(ABK_RSP_SERVERINFO_TYPE,strServerType);
    jp.ExtractValueAtl(ABK_RSP_SERVERINFO_DESCURL,strDescUrl);
    }
  return bSuccess;
  }





//--------------------------------------------------------------------------
// SendAudioRecHeader()    sends an audio header
// --------------------
// Input: nId = general purpose id the server wants to be reflected when the server initiated the recording operation
//        nSampleRateHz = sample rate in Hz
//        nBitsPerSample = bits per sample, 8 and 16 allowed
//        nChannels = number of channels. Allowed is 1 (mono) and 2 (stereo)
// Return: true on success, false on error

bool CAbkClient::SendAudioRecHeader (int nId, int nSampleRateHz, int nBitsPerSample, int nChannels)
  {
  bool bSuccess=false;
  assert(nBitsPerSample==8 || nBitsPerSample==16); // invalid bits per sample??
  assert(nChannels==1 || nChannels==2); // invalid number of channels??
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(pClientAux.IsValid() && IsConnected())
    {
    CJsonFormatter jfHeader; // header data formatted in json
    jfHeader.WriteValue(ABK_AUDIOREC_ID,nId);
    jfHeader.WriteValue(ABK_AUDIOREC_SAMPLERATE_HZ,nSampleRateHz);
    jfHeader.WriteValue(ABK_AUDIOREC_CHANNELS,nChannels);
    jfHeader.WriteValue(ABK_AUDIOREC_BITSPERSAMPLE,nBitsPerSample);
    jfHeader.Close();
    bSuccess=NULL!=pClientAux->NavigatePut(_T(ABK_REQUESTURL_AUDIOREC_HEADER),-1,&jfHeader);
    }
  return bSuccess;
  }


//--------------------------------------------------------------------------
// SendAudioRecData()      sends audio data
// ------------------
// Input: nId = general purpose id the server wants to be reflected when the server initiated the recording operation
//        pData = data.
//                if bits per sample == 8: BYTES
//                if bits per sample == 16: WORDs in  little endian format.
//                value seauence for stereo: left, right, left, right ...
//        nBitsPerSample = bits per sample, 8 and 16 allowed
//        nChannels = number of channels. Allowed is 1 (mono) and 2 (stereo)
//        nSamplesPerChannel = number of samples of each channel in pData
// Return: true on success, false on error

bool CAbkClient::SendAudioRecData (int nId, const void *pData, int nBitsPerSample, int nChannels, int nSamplesPerChannel)
  {
  bool bSuccess=false;
  assert(nBitsPerSample==8 || nBitsPerSample==16); // invalid bits per sample??
  assert(nChannels==1 || nChannels==2); // invalid number of channels??
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(pClientAux.IsValid() && IsConnected())
    {
    CJsonFormatter jfData; // header data formatted in json
    jfData.WriteValue(ABK_AUDIOREC_ID,nId);

    CJsonStreamArray jaData(&jfData,ABK_AUDIOREC_DATA);
    if(nBitsPerSample==8)
      {
      const BYTE *pData8=(const BYTE *)pData;
      for(int nSample=0;nSample<nSamplesPerChannel;++nSample)
        {
        for(int nChannel=0;nChannel<nChannels;++nChannel)
          {
          jaData.WriteValue((int)(*pData8));
          ++pData8;
          }
        }
      }
    else if(nBitsPerSample==16)
      {
      const WORD *pData16=(const WORD *)pData;
      for(int nSample=0;nSample<nSamplesPerChannel;++nSample)
        {
        for(int nChannel=0;nChannel<nChannels;++nChannel)
          {
          jaData.WriteValue((int)(*pData16));
          ++pData16;
          }
        }
      }
    else
      {
      assert(false);
      }
    jaData.Close();

    jfData.Close();
    bSuccess=NULL!=pClientAux->NavigatePut(_T(ABK_REQUESTURL_AUDIOREC_DATA),-1,&jfData);
    }
  return bSuccess;
  }


//--------------------------------------------------------------------------
// SendAudioRecFooter()    sends audio footer
// --------------------
// Input: nId = general purpose id the server wants to be reflected when the server initiated the recording operation
// Return: true on success, false on error

bool CAbkClient::SendAudioRecFooter (int nId)
  {
  bool bSuccess=false;
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid());
  if(pClientAux.IsValid() && IsConnected())
    {
    CJsonFormatter jfFooter; // header data formatted in json
    jfFooter.WriteValue(ABK_AUDIOREC_ID,nId);
    jfFooter.Close();
    bSuccess=NULL!=pClientAux->NavigatePut(_T(ABK_REQUESTURL_AUDIOREC_FOOTER),-1,&jfFooter);
    }
  return bSuccess;
  }



//--------------------------------------------------------------------------
// SendAudioRecRejectEvent() sends event that user rejected audio recording
// -------------------------
// Input: nId = general purpose ID the sender wants to be reflected
// Return: 

bool CAbkClient::SendAudioRecRejectEvent (int nId)
  {
  return SendEvent(ABK_CLIENTEVENT_AUDIOREC_REJECT,_T(""),(double)nId,0,false);
  }




/** returns whether the server will be instructed to translate values into text representation
@return true, if the server is instructed to translate values into text representation
 false, if the server is instructed to send non-translated values and the client shall translate them by value-text tables, if applicable
*/
bool CAbkClient::TextTranslationByServer (void) const
{
  return m_bTextTranslationByServer;
}




//--------------------------------------------------------------------------
// GetInterfaceStatistics() returns interface statistics of server as json formatted string
// ------------------------
// Input: -
// Return: pointer to server interface information in json format
//         NULL on error
//         the pointer is valid until the next aux-request to the server
//         it shall not be freed nor deleted!

const char *CAbkClient::GetInterfaceStatistics (void)
  {
  CClientPtrRef pClientAux(m_pClientAux);
  assert(pClientAux.IsValid()); // no connection with the aux http client established
  if(!pClientAux.IsValid())
    return NULL;
  if(!IsConnected())
    return NULL;
  return pClientAux->NavigateGet(_T(ABK_REQUESTURL_INTERFACESTATS),-1);
  }



//--------------------------------------------------------------------------
// SuspendLongPolling()      pauses the long-poll thread
// ------------------
// Input: -
// Return: 

bool CAbkClient::SuspendLongPolling (void)
  {
  if(!m_hLongPollThread)
    return false;
  m_evLongPollEnable.ResetEvent(); // stall the long polling thread
  return true;
  }

//--------------------------------------------------------------------------
// ResumeLongPolling()     resumes long poll thread
// -------------------
// Input: -
// Return: 

bool CAbkClient::ResumeLongPolling (void)
  {
  if(!m_hLongPollThread)
    return false;
  m_evLongPollEnable.SetEvent(); // no longer stall the long polling thread
  return true;
  }


//--------------------------------------------------------------------------
// LongPollThreadS()        long polling thread
// ----------------
// Input: vpThis = pointer to CAbkClient
// Return: -

/*static*/ DWORD WINAPI CAbkClient::LongPollThreadS (void *vpThis)
  {
  assert(vpThis);
  return (reinterpret_cast<CAbkClient *>(vpThis))->LongPollThread();
  }


//--------------------------------------------------------------------------
// LongPollThread()        long polling thread
// ----------------
// Input: -
// Return: -

int CAbkClient::LongPollThread (void)
{
  AddLog(LOGSEVERITY_TRACE,_T("LongPollThread() started"));
  // int nErrorCount=0; // incrementing on errors, decrementing on http success
  DWORD dwTickLastSuccessfulResponse=0; // ticks when the last successfull response was received
  for (size_t nLoopCounter = 0; !m_bTerminateLongPoll; ++nLoopCounter)
  {
    const char *pszResponse; // answer from server with events and data

    WaitForSingleObject(m_evLongPollEnable.m_hObject,INFINITE); // if stalled, block here until the event gets set

    CClientPtrRef pClientEvent(m_pClientEvent);
    assert(pClientEvent.IsValid()); // no connection with the aux http client established
    if(!pClientEvent.IsValid())
      break;
    //    DWORD dwTickBefore=GetTickCount(); // tick count before the request
    pszResponse=pClientEvent->NavigateGet(_T(ABK_REQUESTURL_SERVEREVENT),m_nSessionId); // request event and wait for answer (blocks here)
    DWORD dwTickAfter=GetTickCount();

    //char buf[1024];
    //pszResponse=buf;
    //memcpy(buf,"{\"DataLists\":{\"DaqListVar\":[74,72174,72174,72174,72174,72174,72174,72174,72174,72174]}}",1024);
    //Sleep(50);

    int nHttpStatus=pClientEvent->GetStatus();
    if(pszResponse && nHttpStatus==HTTP_STATUS_OK)
    {
      dwTickLastSuccessfulResponse=dwTickAfter;
      CJsonParserAtl jpEvent(pszResponse);
      for(;!jpEvent.IsDone();++jpEvent)  // each event
      {
        if(jpEvent.TestObject(ABK_RSP_SERVEREVENT_DATALISTS)) // is there DataLists:{
        {
          OnServerDataBegin();
          for(++jpEvent;!jpEvent.IsDone();++jpEvent)  // each data list
          {
            std::string strDaqName;
            if(jpEvent.TestArray(&strDaqName)) // if array
            {
              CAbkSingleLock lockDaq(&m_mutexDaq,true,DAQ_TIMEOUT); // lock the daq map
              assert(m_mutexDaq.IsLocked());
              CAbkClientDaq *pDaq=FindDaq(strDaqName); // get the client-side daq list
              if(pDaq)
              {
                if (!pDaq->OnDataFromServer(jpEvent)) // here, data gets dispatched to the destination
                  CAbkClientDaq::DiscardJson(jpEvent);
              }
            } // end of data array
            jpEvent.SkipItem(); // skip unexpected items
          } // for each data list
          OnServerDataEnd();
        }
        else if(jpEvent.TestArray(ABK_RSP_SERVEREVENT_EVENTS)) // is there Events:[
        {
          for(++jpEvent;!jpEvent.IsDone();++jpEvent)  // each event
          {
            assert(m_pNextEventData); // there must be a location to store the event params
            BOOL bSuccessDecode=m_pNextEventData->SetEvent(jpEvent); // decode event into m_pNextEventData
            jpEvent.SkipItem(); // skip any unknown items
            if(bSuccessDecode)
            {
              AddLog (LOGSEVERITY_TRACE, _T ("Event: %s, Param1 = %d, Param2 = %d"), m_pNextEventData->GetData().m_strType, (int)m_pNextEventData->GetData().m_dParam1, (int)m_pNextEventData->GetData ().m_dParam2);
              m_pNextEventData=OnServerEvent(m_pNextEventData); // call the event handler and get the location for the next event
            }
            else // error in syntax or completelyness of the event data
            {
              AddLog(LOGSEVERITY_ERROR,_T("The event data was incomplete or had incorrect syntax")/*,m_pNextEventData->m_data.m_strType*/);
              break;
            }
            jpEvent.SkipItem(); // skip unexpected items            
          } // each event
        }
        jpEvent.SkipItem(); // skip unexpected items
      } // for each event
    }
    else if(pszResponse==NULL)
    {
      if(dwTickAfter>=dwTickLastSuccessfulResponse+LONGPOLL_FAILURE_TOLERANCE_MS) // tolerated error time span exceeded
      {
        DWORD dwErrorDuration=0;
        if(dwTickLastSuccessfulResponse)
          dwErrorDuration=dwTickAfter-dwTickLastSuccessfulResponse;
        m_bTerminateLongPoll=true; // terminate and signal that thread terminated itself (an error occured)
        AddLog(LOGSEVERITY_ERROR,_T("Successive http errors for %d ms."),(int)dwErrorDuration/*LONGPOLL_FAILURE_TOLERANCE_MS*/);
      }
      else
      {
        Sleep(LONGPOLL_FAILURE_RECOVERY_MS);
      }
    }
    else // server responded but with error
    {
      DWORD dwWaitBeforeResume=OnLongPollErrorResponse(nHttpStatus,m_nSessionId); // call custom handler
      while(!m_bTerminateLongPoll && dwWaitBeforeResume!=0)
      {
        Sleep(10);
        if(dwWaitBeforeResume>10)
          dwWaitBeforeResume-=10;
        else
          dwWaitBeforeResume=0;
      }
    }
  }
  AddLog(LOGSEVERITY_TRACE,_T("LongPollThread() terminating"));
  m_hLongPollThread=NULL;
  m_evLongPollDone.SetEvent();
  return 0;
}


//--------------------------------------------------------------------------
// FindDaq()               searches for a DAQ
// ---------
// Input: strDaqName = name of daq to search for
// Return: pointer to DAQ list, NULL if not found

CAbkClientDaq *CAbkClient::FindDaq (const std::string &strDaqName)
  {
  CAbkSingleLock lockDaq(&m_mutexDaq,true,DAQ_TIMEOUT);
  assert(m_mutexDaq.IsLocked());
  std::map<std::string,CAbkClientDaq *>::iterator iterDaq;
  iterDaq=m_mapDaq.find(strDaqName);
  if(iterDaq==m_mapDaq.end())
    return NULL; // not found
  return (*iterDaq).second;
  }


//--------------------------------------------------------------------------
// FindDaq()               searches for a DAQ
// ---------
// Input: strDaqName = name of DAQ
// Return: pointer to DAQ list, NULL if not found

CAbkClientDaq *CAbkClient::FindDaq (LPCTSTR pszDaqName)
  {
  std::string strDaqNameA=CT2A(pszDaqName);
  return FindDaq(strDaqNameA);
  }



//--------------------------------------------------------------------------
// AddDaq()                adds a daq list. It needs to be updated afterwards
// --------
// Input: pAdd = pointer to daq to be added. this daq will be deleted
//               with the DeleteDaq() method or on destruction
// Return: true if succeeded, false if daq with name already exists

bool CAbkClient::AddDaq (CAbkClientDaq *pAdd)
  {
  bool bSuccess=false;

  if(m_nSessionId>=0)
    {
    CAbkSingleLock lockDaq(&m_mutexDaq,true,DAQ_TIMEOUT);
    assert(m_mutexDaq.IsLocked());
    std::string strDaqNameA=CT2A(pAdd->m_strName,CP_UTF8);
    if(!FindDaq(strDaqNameA))
      {
      pair<map<std::string,CAbkClientDaq *>::iterator,bool> iterInsert; // result of the insert operation
      iterInsert=m_mapDaq.insert(pair<std::string,CAbkClientDaq *>(strDaqNameA,pAdd));
      assert(iterInsert.second); // error inserting the daq?
      pAdd->m_pOwner=this;
      bSuccess = true;
      }
    else
      AddLog(LOGSEVERITY_ERROR,_T("Tried to add a DAQ while another DAQ with same name exists: \"%s\""),(LPCTSTR)pAdd->m_strName);
    }
  else
    AddLog(LOGSEVERITY_ERROR,_T("Tried to add a DAQ without having a valid session ID"));
  return bSuccess;
  }


//--------------------------------------------------------------------------
// DeleteDaq()             deletes a daq list
// -----------
// Input: pszDaqName = name of daq list to be deleted
//        pDelete = pointer to daq to be deleted
// Return: true if deleted, false if daq not existent

bool CAbkClient::DeleteDaq (LPCTSTR pszDaqName)
  {
  std::string strDaqNameA=CT2A(pszDaqName,CP_UTF8);
  std::map<std::string,CAbkClientDaq *>::iterator iterDaq;
  CAbkSingleLock lockDaq(&m_mutexDaq,true,DAQ_TIMEOUT);
  assert(m_mutexDaq.IsLocked());
  iterDaq=m_mapDaq.find(strDaqNameA);
  if(iterDaq==m_mapDaq.end())
    {
    AddLog(LOGSEVERITY_ERROR,_T("Tried to delete a non-existing DAQ: \"%s\""),pszDaqName);
    return false; // not found
    }
  delete (*iterDaq).second;
  m_mapDaq.erase(iterDaq);
  return true;
  }

bool CAbkClient::DeleteDaq (CAbkClientDaq *pDelete)
  {
  return DeleteDaq(pDelete->m_strName);
  }




//--------------------------------------------------------------------------
// OnServerDataBegin()   called before data of a DAQ will be dispatched through the virtual functions of CAbkClientDaq
// -------------------
// Input: -
// Return: -

/*virtual*/ void CAbkClient::OnServerDataBegin (void)
  {
  ; // do nothing
  }


//--------------------------------------------------------------------------
// OnServerDataEnd()     called after data of a DAQs were dispatched. Counterpart to OnServerDataBegin()
// -----------------
// Input: -
// Return: -

/*virtual*/ void CAbkClient::OnServerDataEnd (void)
  {
  ; // do nothing
  }



//--------------------------------------------------------------------------
// OnServerEvent()         called when server sent an event.
// ---------------         you can override this function to receive events
//                         this function is called in the long polling
//                         threads context
// Input: pEventData = pointer to event with the params of the event
// Return: pointer to event objec receiving the next event
//         You can either return the same event object when you have only
//         one buffer for event reception. Alternatively you can return a
//         pointer to another event object if you have a queue. In this case
//         pEventData must be deleted manually

/*virtual*/ CAbkServerEvent *CAbkClient::OnServerEvent (CAbkServerEvent *pEventData)
  {
  return pEventData; // default implementation: use the same event object for the next event
  }




//--------------------------------------------------------------------------
// OnLongPollErrorResponse() gets called when a server responds with an error status code. Gets called from the long-poll thread context!
// -----------------------
// Input: nHttpStatusCode = http status code 400+x
//        nSessionId = session id which was appended to the long-polling url
// Return: resume time until next request shall be issued. INFINITE waits until thread gets terminated

/*virtual*/ DWORD CAbkClient::OnLongPollErrorResponse (int nHttpStatusCode, int nSessionId)
  {
  // in derived classes, handle the error. No need to call the base class implementation.
  DWORD dwWaitBeforeResume=0;
  if(nHttpStatusCode>=400 && nHttpStatusCode<=499)
    dwWaitBeforeResume=INFINITE; // default: no further requests
  return dwWaitBeforeResume;
  }


//--------------------------------------------------------------------------
// OnLogAdded()          notifies that the log queue has got new entities. may be called in any thread context!
// ---------
// Input: -
// Return: 

/*virtual*/ void CAbkClient::OnLogAdded (void)
  {
  LOGSEVERITY nSeverity;
  CString strError;
  for(;;)
    {
    BOOL bPopSuccess=PopLog(nSeverity,strError); // get one log message
    if(!bPopSuccess)
      break;

    ; // discard it. On derived classes, put the message to somwhere else
    }
  }



//--------------------------------------------------------------------------
// AddLog(), AddLogV            writes one line to error log
// ----------
// Input: nSeverity = severity, one of LOGSEVERITY_xxx
//        pszMessage = message format string, like printf
//        args = optional arguments
// Return: 

void CAbkClient::AddLog (LOGSEVERITY nSeverity, LPCTSTR pszMessage, ...)
  {
  va_list args;
  va_start(args,pszMessage);
  AddLogV(nSeverity,pszMessage,args);
  va_end(args);
  }


//--------------------------------------------------------------------------
// AddLogV()               writes one line to error log
// ---------
// Input: nSeverity = 
//        pszMessage = 
//        args = 
// Return: 

void CAbkClient::AddLogV (LOGSEVERITY nSeverity, LPCTSTR pszMessage, va_list args)
  {
  ASSERT(this); // called with a NULL instance pointer??
  ASSERT(pszMessage); // you have to specify a message
  m_queueLog.AddV((CLogQueue<TCHAR>::LOGSEVERITY)nSeverity,pszMessage,args);
  if(!m_bTerminateLongPoll) // if not in destruction phase (prevent from calling virtual function from within a destructor)
    OnLogAdded(); // notify about changes in error log
  }


//--------------------------------------------------------------------------
// AddLogHttp()            writes one line to error log containing HTTP ínfo
// ------------
// Input: nSeverity = 
//        nHttpStatusCode = 
//        pszUrl = http url
//        pszMethod = http method string
//        pcszResponse = response data
//        pszPostPutData = data when post or put method
// Return: 

void CAbkClient::AddLogHttp (LOGSEVERITY nSeverity, int nHttpStatusCode, LPCTSTR pszUrl, LPCTSTR pszMethod, const char *pcszResponse, const char *pcszoPostPutData/*=NULL*/)
{
  if (!m_bSuppressLog)
  {
    //ASSERT(nHttpStatusCode>=0);
    if(pcszoPostPutData)
      AddLog(LOGSEVERITY_ERROR,_T("HTTP status code %d. Url: \"%s\", Method: %s, Request: \"%s\", Response: \"%s\""),nHttpStatusCode,pszUrl,pszMethod,(LPCTSTR)CA2T(pcszoPostPutData,CP_UTF8),(LPCTSTR)CA2T(pcszResponse,CP_UTF8));
    else
      AddLog(LOGSEVERITY_ERROR,_T("HTTP status code %d. Url: \"%s\", Method: %s, Response: \"%s\""),nHttpStatusCode,pszUrl,pszMethod,(LPCTSTR)CA2T(pcszResponse,CP_UTF8));
    if(nHttpStatusCode<0)
    {
      // 23.10.18: Desktop-PC, Fehler in c:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\atlmfc\include\atlspriv.inl Zeile 218, inline bool ZEvtSyncSocket::Read()=> WSARecv()-Fehler. WSAGetLastError(): 10053
      DWORD dwError=GetLastError();
      AddLog(LOGSEVERITY_ERROR,_T("HTTP status code %d => Error-Code %d"),nHttpStatusCode,(int)dwError);
    }
  }
}


//--------------------------------------------------------------------------
// PopLog()                pops one entity from the error log
// --------
// Input: nSeverityGet = returns severity
//        strMessageGet = copies message string to this string
// Return: TRUE if message could be popped, FALSE if queue is empty

BOOL CAbkClient::PopLog (LOGSEVERITY &nSeverityGet, CString &strMessageGet)
  {
  std::basic_string<TCHAR> strTemp;
  BOOL bSuccess=m_queueLog.Pop((CLogQueue<TCHAR>::LOGSEVERITY &)nSeverityGet,strTemp);
  if(bSuccess)
    strMessageGet=strTemp.c_str();
  return bSuccess;
  }










//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------



/** Constructor of CBaseAbstraction
@param pOwner Pointer to the owning openABK client. Mainly used to emit log files entities
*/
CAbkClient::CBaseAbstraction::CBaseAbstraction (CAbkClient *pOwner)
: m_csNavigate(LOCK_HIER_ABK_NAVIGATE,_T("Abk::CAbkClient::CBaseAbstraction::m_csNavigate"))
{
  ASSERT (pOwner);
  m_pOwner = pOwner;
  m_pszServerAddress = NULL;
  m_dwTimeout = 10000;
  m_nPort = 0;
}




//--------------------------------------------------------------------------
// ~CBaseAbstraction()           Destructor of CBaseAbstraction
// -------------
// Input: -
// Return: 

CAbkClient::CBaseAbstraction::~CBaseAbstraction ()
  {
  }


//--------------------------------------------------------------------------
// SetServerAddr()         re-assigns the server address and port
// ---------------
// Input: strServerAddress = address of server, must not be volatile
//        nPort = port for server connection
// Return: 

void CAbkClient::CBaseAbstraction::SetServerAddr (LPCTSTR pszServerAddress, int nPort)
  {
  m_pszServerAddress=pszServerAddress;
  m_nPort=nPort;
  }




/** sets the timeout for reads on http requests
@param dwNewTimeout timeout in terms of milli seconds to be set
*/
void CAbkClient::CBaseAbstraction::SetTimeout (DWORD dwNewTimeout)
{
  if (dwNewTimeout != m_dwTimeout)
  {
    m_dwTimeout = dwNewTimeout;
    SetSocketTimeout (dwNewTimeout);
  }
}




/** returns timeout for reads on http requests
@return timeout for reads on http requests
*/
DWORD CAbkClient::CBaseAbstraction::GetTimeout (void) const
{
  return m_dwTimeout;
}




//--------------------------------------------------------------------------
// NavigateX()             navigates
// -----------
// Input: pszServer = 
//        pszPath = 
//        pNavData = 
// Return: 

bool CAbkClient::CBaseAbstraction::NavigateX (LPCTSTR pszServer, LPCTSTR pszPath, ATL_NAVIGATE_DATA *pNavData)
  {
#ifdef WINCE
  return Navigate (pszServer, pszPath, pNavData, NULL, m_pOwner->m_bSuppressLog);
#else
  bool bSuccess=false;
  for(int nRetry=0;nRetry<2 && !bSuccess;++nRetry)
    {
    bSuccess=Navigate(pszServer,pszPath,pNavData);
    if(!bSuccess && m_nStatus>=0) // a failure contained in the response (means that request was performed). A status of -1 was expirienced when server closed the connection due to keep-alive timeout
      bSuccess=true;
    }
  return bSuccess;
#endif
  }


//--------------------------------------------------------------------------
// NavigateGet()           sends GET request and waits for answer
// -------------
// Input: pszPath = path within the server
//        nSessionId = id of session to be formatted into query string,
//                     -1 if no session shall be put to query string
// Return: pointer to buffer with response data, NULL on error

const char *CAbkClient::CBaseAbstraction::NavigateGet (LPCTSTR pszPath, int nSessionId)
{
  const char *pResponse = NULL;
  LOCKED_SECTION_NAV(_T("CAbkClient::CBaseAbstraction::NavigateGet()"));
  assert(m_pszServerAddress);
  CAtlNavigateData nav;
  nav.SetPort(m_nPort);
  nav.SetMethod(ATL_HTTP_METHOD_GET);
  nav.SetPostData(NULL,0,NULL);
  nav.RemoveFlags(ATL_HTTP_FLAG_SEND_BLOCKS);
  nav.SetExtraHeaders(_T("Connection: keep-alive\r\n"));
  nav.dwTimeout = m_dwTimeout;
  bool bSuccess;
  if(nSessionId>=0)
  {
    CString strPathAndQuery;
    strPathAndQuery.Format(_T("%s?")_T(ABK_QRY_SESSIONID)_T("=%d"),pszPath,nSessionId);
    bSuccess=NavigateX(m_pszServerAddress,strPathAndQuery,&nav);
    if (!bSuccess)
      m_pOwner->AddLogHttp (LOGSEVERITY_ERROR, GetStatus (), (LPCTSTR)strPathAndQuery, ATL_HTTP_METHOD_GET, GetBodySave ());
  }
  else
  {
    bSuccess=NavigateX(m_pszServerAddress,pszPath,&nav);
    if (!bSuccess)
      m_pOwner->AddLogHttp (LOGSEVERITY_ERROR, GetStatus (), pszPath, ATL_HTTP_METHOD_GET, GetBodySave ());
  }
  if(bSuccess)
    pResponse = (const char *)GetBody();
  return pResponse;
  }


//--------------------------------------------------------------------------
// NavigateGet()          sends a GET request and waits for answer, stores result as file in the local file system
// --------------
// Input: pszPath = URL path within the server
//        nSessionId = id of session to be formatted into query string,
//                     -1 if no session shall be put to query string
//        pszStorePath = local path where to store the result to. Any existing file
//                       will be overwritten
//        pFileDest = file the result shall be written to
// Return: true on success, false on error

bool CAbkClient::CBaseAbstraction::NavigateGet (LPCTSTR pszPath, int nSessionId, LPCTSTR pszStorePath, PFNATLSTATUSCALLBACK pfnReadCallback/*=NULL*/, DWORD_PTR dwCookie/*=0*/)
  {
  ASSERT(pszStorePath);
  ASSERT(pszPath);
  bool bSuccess=FALSE;

  CFile fileStore;
  if(fileStore.Open(pszStorePath,CFile::modeWrite|CFile::typeBinary|CFile::modeCreate|CFile::shareExclusive)) // create the file
    { // if succeeded
    bSuccess=NavigateGet(pszPath,nSessionId,&fileStore,pfnReadCallback,dwCookie);
    }
  else
    {
    m_pOwner->AddLog(LOGSEVERITY_ERROR,_T("Failed in creating local file \"%s\". Url: \"%s\", Method: GET, Response: \"%s\""),pszStorePath,pszPath,(LPCTSTR)CA2T(GetBodySave(),CP_UTF8));
    }
  return bSuccess;
  }


bool CAbkClient::CBaseAbstraction::NavigateGet (LPCTSTR pszPath, int nSessionId, CFile *pFileDest, PFNATLSTATUSCALLBACK pfnReadCallback/*=NULL*/, DWORD_PTR dwCookie/*=0*/)
  {
  ASSERT(pszPath);
  ASSERT(pFileDest);
  assert(m_pszServerAddress);
  bool bSuccessNav=false;
  bool bSuccessFile=true; // #### todo: handle file write exception
  if(1)
    {
    LOCKED_SECTION_NAV(_T("CAbkClient::CBaseAbstraction::NavigateGet()file"));
    CAtlNavigateData nav;
    nav.SetPort(m_nPort);
    nav.SetMethod(ATL_HTTP_METHOD_GET);
    nav.SetPostData(NULL,0,NULL);
    nav.SetReadStatusCallback(pfnReadCallback,dwCookie);
    nav.RemoveFlags(ATL_HTTP_FLAG_SEND_BLOCKS);
    nav.SetExtraHeaders(_T("Connection: keep-alive\r\n"));
    nav.dwTimeout = m_dwTimeout;
    if (nSessionId >= 0)
      {
      CString strPathAndQuery;
      strPathAndQuery.Format(_T("%s?")_T(ABK_QRY_SESSIONID)_T("=%d"),pszPath,nSessionId);
#ifdef WINCE
      bSuccessNav=Navigate(m_pszServerAddress,strPathAndQuery,&nav,pFileDest);
#else
      bSuccessNav=NavigateX(m_pszServerAddress,strPathAndQuery,&nav);
#endif
      }
    else
      {
#ifdef WINCE
      bSuccessNav=Navigate(m_pszServerAddress,pszPath,&nav,pFileDest);
#else
      bSuccessNav=NavigateX(m_pszServerAddress,pszPath,&nav);
#endif
      }
#ifndef WINCE
    if(bSuccessNav)
      {
      const BYTE *pBody=GetBody(); // the potential binary data
      DWORD dwBodyLen=GetBodyLength(); // length of the data
      pFileDest->Write(pBody,dwBodyLen);
      }
#endif
    }
  if(!bSuccessNav)
    m_pOwner->AddLogHttp(LOGSEVERITY_ERROR,GetStatus(),pszPath,ATL_HTTP_METHOD_GET,GetBodySave());
  else if(!bSuccessFile)
    m_pOwner->AddLog(LOGSEVERITY_ERROR,_T("Failed in writing local file. Url: \"%s\", Method: GET, Response: \"%s\""),GetStatus(),pszPath,(LPCTSTR)CA2T(GetBodySave(),CP_UTF8));
  return bSuccessNav && bSuccessFile;    
  }


//--------------------------------------------------------------------------
// NavigatePost()          sends a POST request and waits for answer
// --------------
// Input: pszPath = URL path within the server
//        nSessionId = id of session to be formatted into query string,
//                     -1 if no session shall be put to query string
//        pPostData = data to be sent in the post request
// Return: pointer to response data, empty string if server produced no output, NULL if error

const char *CAbkClient::CBaseAbstraction::NavigatePost (LPCTSTR pszPath, int nSessionId, CJsonFormatter *pPostData)
  {
  assert(m_pszServerAddress);
  std::string strPostData=pPostData->GetStream()->str();
  if(1)
    {
    LOCKED_SECTION_NAV(_T("CAbkClient::CBaseAbstraction::NavigatePost()"));
    CAtlNavigateData nav;
    nav.SetPort(m_nPort);
    nav.SetMethod(ATL_HTTP_METHOD_POST);
    nav.SetPostData((BYTE *)strPostData.c_str(),(DWORD)strPostData.length(),_T(MIME_TYPE_JSON));
    nav.RemoveFlags(ATL_HTTP_FLAG_SEND_BLOCKS);
    nav.SetExtraHeaders(_T("Connection: keep-alive\r\n"));
    nav.dwTimeout = m_dwTimeout;
    bool bSuccess;
    if(nSessionId>=0)
      {
      CString strPathAndQuery;
      strPathAndQuery.Format(_T("%s?")_T(ABK_QRY_SESSIONID)_T("=%d"),pszPath,nSessionId);
      bSuccess=NavigateX(m_pszServerAddress,strPathAndQuery,&nav);
      }
    else
      {
      bSuccess=NavigateX(m_pszServerAddress,pszPath,&nav);
      }
    if(bSuccess)
      {
      return (const char *)GetBody();
      }
    }
  m_pOwner->AddLogHttp(LOGSEVERITY_ERROR,GetStatus(),pszPath,ATL_HTTP_METHOD_POST,GetBodySave(),strPostData.c_str());
  return NULL;
  }


//--------------------------------------------------------------------------
// NavigateDelete()        sends a DELETE request
// ----------------
// Input: strPath = path within t6he server
//        nSessionId = id of session to be formatted into query string,
//                     -1 if no session shall be put to query string
//        pPostData = data to be sent in the delete request
// Return: true on success, false on error

bool CAbkClient::CBaseAbstraction::NavigateDelete (LPCTSTR pszPath, int nSessionId, CJsonFormatter *pPostData)
  {
  assert(m_pszServerAddress);
  bool bSuccess;
  LPCTSTR pszMethod=_T("DELETE");
  std::string strPostData=pPostData->GetStream()->str();
  if(1)
    {
    LOCKED_SECTION_NAV(_T("CAbkClient::CBaseAbstraction::NavigateDelete()"));
    CAtlNavigateData nav;
    nav.SetPort(m_nPort);
    nav.SetMethod(pszMethod);
    nav.SetPostData((BYTE *)strPostData.c_str(),(DWORD)strPostData.length(),_T(MIME_TYPE_JSON));
    nav.RemoveFlags(ATL_HTTP_FLAG_SEND_BLOCKS);
    nav.SetExtraHeaders(_T("Connection: keep-alive\r\n"));
    nav.dwTimeout = m_dwTimeout;
    if (nSessionId >= 0)
      {
      CString strPathAndQuery;
      strPathAndQuery.Format(_T("%s?")_T(ABK_QRY_SESSIONID)_T("=%d"),pszPath,nSessionId);
      bSuccess=NavigateX(m_pszServerAddress,strPathAndQuery,&nav);
      }
    else
      {
      bSuccess=NavigateX(m_pszServerAddress,pszPath,&nav);
      }
    }
  if(!bSuccess)
    m_pOwner->AddLogHttp(LOGSEVERITY_ERROR,GetStatus(),pszPath,pszMethod,GetBodySave(),strPostData.c_str());
  return bSuccess;
  }


//--------------------------------------------------------------------------
// NavigatePut()           sends a PUT request
// -------------
// Input: pszPath = path within the server
//        nSessionId = id of session to be formatted into query string,
//                     -1 if no session shall be put to query string
//        pPostData = data to be sent in the put request
// Return: returned data or empty string if no output from server, NULL on error.

const char *CAbkClient::CBaseAbstraction::NavigatePut (LPCTSTR pszPath, int nSessionId, CJsonFormatter *pPutData)
  {
  assert(m_pszServerAddress);
  std::string strPutData=pPutData->GetStream()->str();
  return NavigatePut(pszPath,nSessionId,strPutData.c_str(),(DWORD)strPutData.length(),_T(MIME_TYPE_JSON));
  }


//--------------------------------------------------------------------------
// NavigatePut()           sends a PUT request
// -------------
// Input: pszPath = path within the server
//        nSessionId = id of session to be formatted into query string,
//                     -1 if no session shall be put to query string
//        pPutData = data to be sent in the put request
//        nLen = number of bytes to be sent
//        strMimeType = mime-type of the data
// Return: returned data or empty string if no output from server, NULL on error.

const char *CAbkClient::CBaseAbstraction::NavigatePut (LPCTSTR pszPath, int nSessionId, const char *pPutData, int nLen, LPCTSTR pszMimeType)
  {
  LOCKED_SECTION_NAV(_T(__FUNCTIONW__));
  assert(m_pszServerAddress);
  bool bSuccess=false;
  LPCTSTR pszMethod=_T("PUT");
  CAtlNavigateData nav;
  nav.SetPort(m_nPort);
  nav.SetMethod(pszMethod);
  nav.SetPostData((BYTE *)pPutData,nLen,pszMimeType);
  nav.RemoveFlags(ATL_HTTP_FLAG_SEND_BLOCKS);
  nav.SetExtraHeaders(_T("Connection: keep-alive\r\n"));
  nav.dwTimeout = m_dwTimeout;
  if (nSessionId >= 0)
    {
    CString strPathAndQuery;
    strPathAndQuery.Format(_T("%s?")_T(ABK_QRY_SESSIONID)_T("=%d"),pszPath,nSessionId);
    bSuccess=NavigateX(m_pszServerAddress,strPathAndQuery,&nav);
    }
  else
    {
    bSuccess=NavigateX(m_pszServerAddress,pszPath,&nav);
    }
  if(!bSuccess)
    {
    std::string strPutData(pPutData,nLen);
    int nStatus=GetStatus();
    m_pOwner->AddLogHttp(LOGSEVERITY_ERROR,nStatus,pszPath,pszMethod,GetBodySave(),strPutData.c_str());
    }
  else
    {
    return (const char *)GetBody();
    }
  return NULL;
  }


//--------------------------------------------------------------------------
// GetBodySave()           returns last response data, empty string on no data
// -------------
// Input: -
// Return: 

const char *CAbkClient::CBaseAbstraction::GetBodySave (void)
  {
  const char *pBody=(const char *)GetBody();
  if(!pBody)
    pBody="";
  return pBody;
  }




/**   
@param pszClientClass class name of the client
@param pszClientType type name of the client
@param pszClientSerial serial number or id of the client. If an empty string, the field will not be encoded
@param pszClientFwRev Firmware revision of the client. If an empty string, the field will not be encoded
@param pszClientHwRev Hardware revision of the client. If an empty string, the field will not be encoded
@return session id, -1 on error
*/
int CAbkClient::CBaseAbstraction::ObtainSessionId (LPCTSTR pszClientClass, LPCTSTR pszClientType, LPCTSTR pszClientSerial, LPCTSTR pszClientFwRev, LPCTSTR pszClientHwRev)
  {
  int nSessionId=-1; // result
  assert(pszClientClass);
  assert(pszClientType);
  CJsonFormatter jfPost;
  jfPost.WriteValue(ABK_REQ_SESSIONID_CLASS,CT2A(pszClientClass,CP_UTF8));
  jfPost.WriteValue(ABK_REQ_SESSIONID_TYPE,CT2A(pszClientType,CP_UTF8));
  if (pszClientSerial && pszClientSerial[0])
    jfPost.WriteValue(ABK_REQ_SESSIONID_SERIAL,CT2A(pszClientSerial,CP_UTF8));
  if (pszClientFwRev && pszClientFwRev[0])
    jfPost.WriteValue(ABK_REQ_SESSIONID_FWVERSION,CT2A(pszClientFwRev,CP_UTF8));
  if (pszClientHwRev && pszClientHwRev[0])
    jfPost.WriteValue(ABK_REQ_SESSIONID_HWVERSION,CT2A(pszClientHwRev,CP_UTF8));

  const char *pReturn=NavigatePost(_T(ABK_REQUESTURL_SESSIONID),-1,&jfPost);
  if(!pReturn)
    return -1;

  // extract the session id
  CJsonParser parsResponse(pReturn);
  for(;!parsResponse.IsDone();++parsResponse)
    parsResponse.ExtractValue(ABK_RSP_SESSIONID_ID,&nSessionId);
  if(nSessionId<0)
    {
    m_pOwner->AddLog(LOGSEVERITY_ERROR,_T("Got no session id from server."));
    return -1;
    }
  return nSessionId;
  }




//--------------------------------------------------------------------------
// GetLastModified()       returns the last modified date of a file by given url
// -----------------
// Input: strUrl = url of file to be examined
//        pGet = pointer to return the last modified date
// Return: true on success, false on error

bool CAbkClient::CBaseAbstraction::GetLastModified (LPCTSTR pszUrl, CTime *pGet)
  {
  assert(m_pszServerAddress);
  bool bSuccess;
  LPCTSTR pszMethod=_T("HEAD");
  if(1)
    {
    LOCKED_SECTION_NAV(_T("CAbkClient::CBaseAbstraction::GetLastModified()"));
    CAtlNavigateData nav;
    nav.SetPort(m_nPort);
    nav.SetMethod(pszMethod);
    nav.SetPostData(NULL,0,NULL);
    nav.RemoveFlags(ATL_HTTP_FLAG_SEND_BLOCKS);
    nav.SetExtraHeaders(_T("Connection: keep-alive\r\n"));
    nav.dwTimeout = m_dwTimeout;
    bSuccess = NavigateX (m_pszServerAddress, pszUrl, &nav);
    if(bSuccess||GetResponseStatus()==RR_READBODY_FAILED)  // HEAD method creates an error but with the RR_READBODY_FAILED it is OK
      {
      CString strDateModified;
      GetHeaderValue(_T("Last-Modified"),strDateModified);
      return DecodeDate(strDateModified,pGet);
      }
    }
  m_pOwner->AddLogHttp(LOGSEVERITY_ERROR,GetStatus(),pszUrl,pszMethod,GetBodySave());
  return false;    
  }


//--------------------------------------------------------------------------
// DeleteSession()         deletes the actual session
// ---------------
// Input: nSessionId = id of session to be deleted
// Return: true on success, false on error

bool CAbkClient::CBaseAbstraction::DeleteSession (int nSessionId)
  {
  CJsonFormatter jfSend;
  return NavigateDelete(_T(ABK_REQUESTURL_SESSIONID),nSessionId,&jfSend);
  }





//--------------------------------------------------------------------------
// DecodeDate()            decodes date into a CTime
// ------------
// Input: pszDate = string representation of the date
//        pResult = pointer to return the date, local time
// Return: true on success

/*static*/ bool CAbkClient::CBaseAbstraction::DecodeDate (LPCTSTR pszDate, CTime *pResult)
  {
  #define DECODEDATE_BUFSIZE 20
  assert(pszDate);
  int nLen=(int)_tcslen(pszDate);
  TCHAR bufDummy[DECODEDATE_BUFSIZE];
  TCHAR bufMonth[DECODEDATE_BUFSIZE];
  static LPCTSTR pszMonths[]={_T("Jan"),_T("Feb"),_T("Mar"),_T("Apr"),_T("May"),_T("Jun"),_T("Jul"),_T("Aug"),_T("Sep"),_T("Oct"),_T("Nov"),_T("Dec"),};
  int nDay=-1, nYear=-1, nHour=-1, nMinute=-1, nSecond=-1, nMonth=-1;
  bool bSuccess=false;

  // test for "Sun, 06 Nov 1994 08:49:37 GMT"
  if((!bSuccess)&&(nLen==29))
    {
    int nScanned=_stscanf_s(pszDate,_T("%3s, %d %s %d %d:%d:%d %s"),bufDummy,DECODEDATE_BUFSIZE,&nDay,bufMonth,DECODEDATE_BUFSIZE,&nYear,&nHour,&nMinute,&nSecond,bufDummy,DECODEDATE_BUFSIZE);
    if(nScanned==8)
      {
      for(nMonth=0;nMonth<(sizeof(pszMonths)/sizeof(pszMonths[0]));nMonth++)
        {
        if(!_tcscmp(pszMonths[nMonth],bufMonth))
          break;
        }
      nMonth++;
      bSuccess=true;
      }
    }

  // test for "Sunday, 06-Nov-94 08:49:37 GMT"
  if(!bSuccess)
    {
    int nScanned=_stscanf_s(pszDate,_T("%s %d-%3s-%d %d:%d:%d %s"),bufDummy,DECODEDATE_BUFSIZE,&nDay,bufMonth,DECODEDATE_BUFSIZE,&nYear,&nHour,&nMinute,&nSecond,bufDummy,DECODEDATE_BUFSIZE);
    if(nScanned==8)
      {
      for(nMonth=0;nMonth<(sizeof(pszMonths)/sizeof(pszMonths[0]));nMonth++)
        {
        if(!_tcscmp(pszMonths[nMonth],bufMonth))
          break;
        }
      nMonth++;
      if(nYear<70)
        nYear+=2000;
      else
        nYear+=1900;
      bSuccess=true;
      }
    }

  // test for "Sun Nov  6 08:49:37 1994"
  if(!bSuccess)
    {
    int nScanned=_stscanf_s(pszDate,_T("%3s %3s %d %d:%d:%d %d"),bufDummy,DECODEDATE_BUFSIZE,bufMonth,DECODEDATE_BUFSIZE,&nDay,&nHour,&nMinute,&nSecond,&nYear);
    if(nScanned==7)
      {
      for(nMonth=0;nMonth<(sizeof(pszMonths)/sizeof(pszMonths[0]));nMonth++)
        {
        if(!_tcscmp(pszMonths[nMonth],bufMonth))
          break;
        }
      nMonth++;
      bSuccess=true;
      }
    }

  if(bSuccess)
    {
    if((nDay<1)||(nDay>31))
      return false;
    if((nMonth<1)||(nMonth>12))
      return false;
    if((nYear<1970)||(nYear>3000))
      return false;
    if((nHour<0)||(nHour>24))
      return false;
    if((nMinute<0)||(nMinute>59))
      return false;
    if((nSecond<0)||(nSecond>59))
      return false;

    // convert from GMT to local time. Date is always GMT, see http://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html#sec3.3.1
    *pResult=CTime(nYear,nMonth,nDay,nHour,nMinute,nSecond);
    tm tmLocal, tmGmt;
    pResult->GetLocalTm(&tmLocal);
    pResult->GetGmtTm(&tmGmt);
    int nDiff=tmLocal.tm_hour-tmGmt.tm_hour;
    *pResult+=CTimeSpan(0,nDiff,0,0);
    }
  return bSuccess;
  }






//--------------------------------------------------------------------------
// GetVarOrMailboxValue()  queries value of variable or mailbos
// ----------------------
// Input: pszPath = path within server, either from variable or mailbox get url
//        pszName = Name of variable or mailbox
//        pGet = pointer to return the value
// Return: true on success, false on error

template <typename U>
bool CAbkClient::CBaseAbstraction::GetVarOrMailboxValue (LPCTSTR pszPath, const char *pszName, U *pGet)
  {
  CJsonFormatter jfRequest;
    {
    CJsonStreamArray jaGetList(&jfRequest,ABK_REQ_VARVALUE_GETLIST);
    jaGetList.WriteValue(pszName);
    } // let array object fall out of scope
  jfRequest.Close();
  const char *pszResponse=NavigatePost(pszPath,-1,&jfRequest);
  if(!pszResponse)
    return false;
  bool bSuccess=false; // will be true when value was decoded
  CJsonParser jpResponse(pszResponse);
  for(;!jpResponse.IsDone();++jpResponse)
    {
    if(jpResponse.TestArray(ABK_RSP_VARVALUE_DATA)) // is there the returned data
      {
      ++jpResponse; // jump into the array members
      jpResponse.ExtractValue(pGet);
      bSuccess=true;
      break; // we are only interested in one data element
      }
    }
  return bSuccess;
  }




//--------------------------------------------------------------------------
// SetVarOrMailboxValue()  sets value of variable or mailbox
// ----------------------
// Input: pszPath = path within server, either from variable or mailbox get url
//        pszName = Name of variable or mailbox
//        pSet = poiter to value to be set
// Return: true on success, false on error

template <typename U>
bool CAbkClient::CBaseAbstraction::SetVarOrMailboxValue (LPCTSTR pszPath, const char *pszName, const U *pSet)
  {
  CJsonFormatter jfRequest;
    {
    CJsonStreamArray jaGetList(&jfRequest,ABK_REQ_VARVALUE_PUTLIST);
      {
      CJsonStreamObject joVarPut(&jaGetList);
      joVarPut.WriteValue(ABK_REQ_VARVALUE_NAME,pszName);
      joVarPut.WriteValue(ABK_REQ_VARVALUE_VALUE,*pSet);
      }
    } // let array object fall out of scope
  jfRequest.Close();
  return NULL!=NavigatePut(pszPath,-1,&jfRequest);
  }






/** requests meta data of a variables or mailboxes
@param vectVarNames[in] list of variable names
@param pGet[out] pointer to return the meta data. Data will be appended rather than overwritten
@param bMailboxFlag[in] mailbox-flag for the meta data entity
@return true on success, false on error
*/
bool CAbkClient::CBaseAbstraction::GetVarOrMailboxMeta (const std::vector<LPCTSTR> &vectVarNames, std::vector<CAbkClientMeta> *pGet, bool bMailboxFlag)
{
  assert(pGet);
  bool bSuccess=false;
  LPCTSTR pszPath=bMailboxFlag ? _T(ABK_REQUESTURL_MAILBOXMETA) : _T(ABK_REQUESTURL_VARMETA);
  CJsonFormatter jfRequest;
  {
    CJsonStreamArray jaVarList(&jfRequest,ABK_REQ_VARMETA_VARLIST); // "VarList": [
    for(std::vector<LPCTSTR>::const_iterator itVarNames=vectVarNames.begin();itVarNames!=vectVarNames.end();++itVarNames)
    {
      LPCTSTR pszVarName=*itVarNames;
      jaVarList.WriteValue(CT2A(pszVarName,CP_UTF8)); // "Var1",
    }
  } // jaVarList falls out of scope => "]"
  jfRequest.Close(); // "}"

  if(const char *pszResponse=NavigatePost(pszPath,-1,&jfRequest))
  {
    size_t nReserve = pGet->size() + vectVarNames.size();
    if (nReserve > pGet->capacity())
      pGet->reserve(nReserve);
    CJsonParser jpMeta(pszResponse);
    for(;!jpMeta.IsDone();++jpMeta)
    {
      if(jpMeta.TestArray(ABK_RSP_VARMETA_METADATA)) // is there an array named "MetaData":
      {
        for(++jpMeta;!jpMeta.IsDone();++jpMeta)
        {
          CAbkClientMeta metaVar;
          bSuccess=metaVar.ExtractFromJson(jpMeta);
          metaVar.m_bIsMailbox=bMailboxFlag;
          pGet->push_back(metaVar); // append meta data to result list
        }
      }
      jpMeta.SkipItem(); // skip any other members
    }
    bSuccess=true;
  }
  else
  {
    m_pOwner->AddLogHttp(LOGSEVERITY_WARNING,GetStatus(),pszPath,ATL_HTTP_METHOD_POST,GetBodySave());
  }
  return bSuccess;
}




/** requests list of variables or mailboxes
@param pszPath[in] url path for requesting the list
@param pGet[out] pointer to return the var/mailbox list
@return true on success, false on error
*/
bool CAbkClient::CBaseAbstraction::GetVarOrMailboxList (LPCTSTR pszPath, std::vector<CString> *pGet)
{
  assert(pszPath);
  bool bSuccess=false;
  const size_t nCapacityGranularity=256;
  if(const char *pszResponse=NavigateGet(pszPath,-1))
  {
    CJsonParser jpVars(pszResponse);
    for(;!jpVars.IsDone();++jpVars)
    {
      if(jpVars.TestArray(ABK_RSP_VARLIST)) // is there "VarList": [
      {
        for(++jpVars;!jpVars.IsDone();++jpVars)
        {
          std::string strVarName;
          jpVars.ExtractValue(&strVarName);
          if(pGet->size()==pGet->capacity())
            pGet->reserve(pGet->size()+nCapacityGranularity); // some granularity
          pGet->push_back(CString(CA2T(strVarName.c_str(),CP_UTF8)));
        }
        bSuccess=true;
      }
      jpVars.SkipItem();
    }
  }
  return bSuccess;
}





//--------------------------------------------------------------------------
// CFormElement()          Constructor of CFormElement
// --------------
// Input: -
// Return: 

CAbkClient::CFormElement::CFormElement ()
  {
  m_nType=TYPE_INVALID;
  m_nMaxLen=0;
  }


//--------------------------------------------------------------------------
// ~CFormElement()         Constructor of ~CFormElement
// ---------------
// Input: -
// Return: 

CAbkClient::CFormElement::~CFormElement ()
  {
  VariantClear(&m_varValue);  // clear byte array e.g. when m_varValue holds a string
  }


//--------------------------------------------------------------------------
// DecodeJson()            docodes from JSON
// ------------
// Input: jpElement = json parser containing a form element
// Return: TRUE on success, FALSE on decode error

bool CAbkClient::CFormElement::DecodeJson (CJsonParserAtl &jpElement)
  {
  bool bNameSent=false;
  bool bCaptionSent=false;
  bool bTypeSent=false;
  bool bValueSent=false;
  CString strType;
  m_vectOptions.clear(); // empty the option list
  m_nMaxLen=0;
  m_nFlags=0;
  m_nType=TYPE_INVALID;

  for(++jpElement;!jpElement.IsDone();++jpElement) // each attribute
    {
    bNameSent|=jpElement.ExtractValueAtl(ABK_RSP_FORMS_CONTROL_NAME,m_strName);
    bCaptionSent|=jpElement.ExtractValueAtl(ABK_RSP_FORMS_CONTROL_CAPTION,m_strCaption);
    bTypeSent|=jpElement.ExtractValueAtl(ABK_RSP_FORMS_CONTROLTYPE,strType);
    bValueSent|=jpElement.ExtractValueAtl(ABK_RSP_FORMS_CONTROL_INITIALVALUE,m_varValue);
    jpElement.ExtractValue(ABK_RSP_FORMS_CONTROL_MAXLEN,&m_nMaxLen); // ocassionally decode the maximum len
    if(jpElement.TestArray(ABK_RSP_FORMS_CONTROL_OPTIONS)) // is there "Options":[
      {
      CString strOption;
      for(++jpElement;!jpElement.IsDone();++jpElement) // each option
        {
        jpElement.ExtractValueAtl(strOption);
        m_vectOptions.push_back(strOption);
        }
      }

    // test for additional attributes
    bool bAttribute;
    if(jpElement.ExtractValue(ABK_RSP_FORMS_CONTROL_READONLY,&bAttribute) && bAttribute)
      m_nFlags|=READONLY;
    if(jpElement.ExtractValue(ABK_RSP_FORMS_CONTROL_PASSWORD,&bAttribute) && bAttribute)
      m_nFlags|=PASSWORD;
    if(jpElement.ExtractValue(ABK_RSP_FORMS_CONTROL_NUMERIC,&bAttribute) && bAttribute)
      m_nFlags|=NUMERIC;
    if(jpElement.ExtractValue(ABK_RSP_FORMS_CONTROL_SUBMIT,&bAttribute) && bAttribute)
      m_nFlags|=SUBMIT;
    if(jpElement.ExtractValue(ABK_RSP_FORMS_CONTROL_CANCEL,&bAttribute) && bAttribute)
      m_nFlags|=CANCEL;
    if(jpElement.ExtractValue(ABK_RSP_FORMS_CONTROL_UPDATEABLE,&bAttribute) && bAttribute)
      m_nFlags|=UPDATEABLE;
    }

  // decode type
  if(!strType.Compare(_T(ABK_RSP_FORMS_CONTROLTYPE_INPUT)))
    m_nType=TYPE_EDIT;
  else if(!strType.Compare(_T(ABK_RSP_FORMS_CONTROLTYPE_CHECKBOX)))
    m_nType=TYPE_CHECKBOX;
  else if(!strType.Compare(_T(ABK_RSP_FORMS_CONTROLTYPE_COMBO)))
    m_nType=TYPE_COMBO;
  else if(!strType.Compare(_T(ABK_RSP_FORMS_CONTROLTYPE_BUTTON)))
    m_nType=TYPE_BUTTON;
  else
    return false;

  // test if all mandatory fields were present
  if(!bNameSent || !bCaptionSent || !bTypeSent)
    return false;
  if((m_nType!=TYPE_BUTTON)&&(!bValueSent)) // all excapt button requires a initial value field
    return false;

  return true;
  }





} // namespace

