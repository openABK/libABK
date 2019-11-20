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
// |  __| |   �   ||  -  /| |  | |  __  \___ \ | | | |/ __|
// | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
// |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
//                                              |___/      
// Filename:    AbkServerEvent.cpp
// Created:     2012-08-02 (07:44)
// Author:      D. Burger
// Description: Object to store an event coming from server
//------------------------------------------------------------------------------------------------

#include "stdafx.h"
#include "AbkServerEvent.h"
#include "JsonParserAtl.h"
#include "ValuesFromSpec.h"

namespace Abk {

#define SERVEREVENT_LOCK_TIMEOUT 1000 // time to try to acquire the mutex

#define LOCKED_SECTION CAbkSingleLock lockSection(&m_mutex,true,SERVEREVENT_LOCK_TIMEOUT);
  

//--------------------------------------------------------------------------
// CAbkServerEvent()       Constructor of CAbkServerEvent
// -----------------
// Input: -
// Return: 

CAbkServerEvent::CAbkServerEvent (void)
  {
  m_evIsEmpty.Set(); // object can be populated with data
  }



//--------------------------------------------------------------------------
// ~CAbkServerEvent()      Constructor of ~CAbkServerEvent
// ------------------
// Input: -
// Return: 

/*virtual*/ CAbkServerEvent::~CAbkServerEvent (void)
  {
     
  }




//--------------------------------------------------------------------------
// SetEvent()              sets event, blocks when object is not empty
// ----------              typically called by the long-poll thread
// Input: jpEvent = json parser containing the event
// Return: TRUE on success, FALSE on error

BOOL CAbkServerEvent::SetEvent (CJsonParserAtl &jpEvent)
  {
  if(!m_evIsEmpty.Wait(SERVEREVENT_LOCK_TIMEOUT)) // wait until event gets empty
    return FALSE; // event was not flushed by another thread via CAbkServerEvent::GetEvent() => error

  // LOCKED_SECTION

  // decode event
  bool bEventTypeSent=FALSE;
  bool bTimeSent=FALSE;
  bool bSenderSent=FALSE;
  bool bRoleSent=FALSE;
  bool bStrParamSent=FALSE;
  bool bParam1Sent=FALSE;
  bool bParam2Sent=FALSE;
  for(++jpEvent;!jpEvent.IsDone();++jpEvent) // each member of the event
    {
    bEventTypeSent   |= jpEvent.ExtractValueAtl(ABK_RSP_SERVEREVENT_TYPE     , m_data.m_strType );
    bTimeSent        |= jpEvent.ExtractValue   (ABK_RSP_SERVEREVENT_TIME     ,&m_data.m_tmEvent );
    bSenderSent      |= jpEvent.ExtractValue   (ABK_RSP_SERVEREVENT_SENDER   ,&m_data.m_nSender );
    bRoleSent        |= jpEvent.ExtractValueAtl(ABK_RSP_SERVEREVENT_ROLE     , m_data.m_strRole );
    bStrParamSent    |= jpEvent.ExtractValue   (ABK_RSP_SERVEREVENT_STRPARAM ,&m_data.m_strParam);
    bParam1Sent      |= jpEvent.ExtractValue   (ABK_RSP_SERVEREVENT_PARAM1   ,&m_data.m_dParam1 );
    bParam2Sent      |= jpEvent.ExtractValue   (ABK_RSP_SERVEREVENT_PARAM2   ,&m_data.m_dParam2 );
    }
  jpEvent.SkipItem(); // skip any unknown items
  if(bEventTypeSent && bTimeSent && bSenderSent && bRoleSent && bStrParamSent && bParam1Sent && bParam2Sent) // if all mandatory fields were sent
    {
    m_evIsEmpty.Reset(); // if successfully decoded: mark object as not empty
    return TRUE;
    }
  return FALSE; // discard the event since not all mandatory fields were present
  }



//--------------------------------------------------------------------------
// GetEvent()              peeks data from event and then marks it as empty
// ----------              typically called by the main tread
// Input: dataGet = reference to return a copy of the event data 
// Return: TRUE on success, FALSE on error

BOOL CAbkServerEvent::GetEvent (CData &dataGet)
  {
  if(m_evIsEmpty.IsSet()) // if there is no valid data (previousely set with CAbkServerEvent::SetEvent())..
    return FALSE; // return with error
  // LOCKED_SECTION

  // copy the data to the desired destination
  dataGet=m_data;

  m_evIsEmpty.Set(); // mark the object as empty (available to put new data into it)
  return TRUE;
  }




////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////


//--------------------------------------------------------------------------
// IsPrimary()             returns TRUE if client is the primary handling client
// -----------
// Input: -
// Return: 

BOOL CAbkServerEvent::CData::IsPrimary (void) const
  {
  return m_strRole.Compare(_T(ABK_CLIENTROLE_PRIMARY))==0;
  }


//--------------------------------------------------------------------------
// IsButton()              returns TRUE if event is a button event
// ----------
// Input: -
// Return: 

BOOL CAbkServerEvent::CData::IsButton (void) const
  {
  return m_strType.Compare(_T(ABK_CLIENTEVENT_BUTTON))==0;
  }


//--------------------------------------------------------------------------
// GetButtonName()         returns the button name
// ---------------
// Input: -
// Return: 

CString CAbkServerEvent::CData::GetButtonName (void) const
  {
  return CString(CA2T(m_strParam.c_str()));
  }


//--------------------------------------------------------------------------
// GetButtonState()        returns the button state. TRUE=pressed, FALSE=released
// ----------------
// Input: -
// Return: 

BOOL CAbkServerEvent::CData::GetButtonState (void) const
  {
  return m_dParam1!=0;
  }


//--------------------------------------------------------------------------
// GetButtonFilter()       returns TRUE if button event shall be handled
// -----------------
// Input: nSession = 
// Return: 

BOOL CAbkServerEvent::CData::GetButtonFilter (int nSession) const
  {
  return (nSession==m_nSender)&&(IsPrimary()); // button shall be processed if from me or if i am the primary client
  }


//--------------------------------------------------------------------------
// IsWheel()               returns TRUE if event is a wheel movement
// ---------
// Input: -
// Return: 

BOOL CAbkServerEvent::CData::IsWheel (void) const
  {
  return m_strType.Compare(_T(ABK_CLIENTEVENT_WHEEL))==0;
  }


//--------------------------------------------------------------------------
// GetWheelName()          gets the name of the wheel
// --------------
// Input: -
// Return: 

CString CAbkServerEvent::CData::GetWheelName (void) const
  {
  return CString(CA2T(m_strParam.c_str()));
  }


//--------------------------------------------------------------------------
// GetWheelIncrements()    gets the wheel increments
// --------------------
// Input: -
// Return: 

int CAbkServerEvent::CData::GetWheelIncrements (void) const
  {
  return (int)m_dParam1;
  }


//--------------------------------------------------------------------------
// GetWheelFilter()        returns TRUE if wheel event shall be handled
// ----------------
// Input: -
// Return: 

BOOL CAbkServerEvent::CData::GetWheelFilter (int nSession) const
  {
  return (nSession==m_nSender)&&(IsPrimary()); // wheel shall be processed if from me or if i am the primary client    
  }


//--------------------------------------------------------------------------
// IsIdentification()      returns TRUE if event is an identification event
// ------------------
// Input: -
// Return: 

BOOL CAbkServerEvent::CData::IsIdentification (void) const
  {
  return m_strType.Compare(_T(ABK_SVREVENT_IDENTIFY))==0;    
  }


//--------------------------------------------------------------------------
// GetIndentificationMessage() returns the message to be displayed as identification
// ---------------------------
// Input: -
// Return: 

CString CAbkServerEvent::CData::GetIndentificationMessage (void) const
  {
  return CString(CA2T(m_strParam.c_str()));
  }



//--------------------------------------------------------------------------
// IsLimitAlert()          returns TRUE if event is a limit violation alert
// --------------
// Input: -
// Return: 

BOOL CAbkServerEvent::CData::IsLimitAlert (void) const
  {
  return ((m_strType.Compare(_T(ABK_SVREVENT_LIMITALERT))==0) || (m_strType.Compare(_T(ABK_SVREVENT_ALERT))==0));
  }


//--------------------------------------------------------------------------
// GetLimitParams()        retrieves the parameters of limit alert
// ----------------
// Input: strVarName = reference to receive the name of the limit alert variable
//        varValue = value of variable when the violation was detected
//        strClass = class of limit alert (e.g. "KickDown")
//        bNoClientSuppress = server wants the client not to suppress displaying alert to the user
// Return: TRUE if succeeded decoding, FALSE if error in decoding

BOOL CAbkServerEvent::CData::GetLimitParams (CString &strVarName, _variant_t &varValue, CString &strClass, BOOL &bNoClientSuppress) const
  {
  CJsonParserAtl jpAlert(m_strParam.c_str());
  bool bNoCliSupA=false; // default, if the field is not present
  strVarName.Empty();
  varValue.Clear();
  bool bNameDecoded=false;
  bool bValueDecoded=false;
  bool bClassDecoded=false;
  bool bNoClientSuppressDecoded=false;
  for(;!jpAlert.IsDone();++jpAlert) // each member
    {
    bNameDecoded|=jpAlert.ExtractValueAtl(ABK_SVREVENT_LIMITALERT_NAME,strVarName); // decode the name
    bValueDecoded|=jpAlert.ExtractValueAtl(ABK_SVREVENT_LIMITALERT_VALUE,varValue); // decode the variable value
    bClassDecoded|=jpAlert.ExtractValueAtl(ABK_SVREVENT_LIMITALERT_CLASS,strClass); // decode alert class
    bNoClientSuppressDecoded=jpAlert.ExtractValue(ABK_SVREVENT_ALERT_NOCLISUP,&bNoCliSupA);
    jpAlert.SkipItem(); // skip any unknown items
    }
  bNoClientSuppress=bNoCliSupA;
  if(bNameDecoded&&bValueDecoded) // if all mandatory fields are decoded..
    return TRUE; // .. ok
  if(bClassDecoded) // if only class found
    return TRUE; // .. ok
  return FALSE;
  }


//--------------------------------------------------------------------------
// IsFormOpen()             returns TRUE if event is a form-open request
// --------
// Input: -
// Return: TRUE if event is a form request

BOOL CAbkServerEvent::CData::IsFormOpen (void) const
  {
  return m_strType.Compare(_T(ABK_SVREVENT_FORMREQUIRED))==0;
  }


//--------------------------------------------------------------------------
// IsFormClose()           returns TRUE if event is a form-close request
// -------------
// Input: -
// Return: 

BOOL CAbkServerEvent::CData::IsFormClose (void) const
  {
  return m_strType.Compare(_T(ABK_SVREVENT_FORMCLOSE))==0;
  }

//--------------------------------------------------------------------------
// GetFormName()           returns the name of the requested form
// -------------
// Input: -
// Return: the name of the requested form

CString CAbkServerEvent::CData::GetFormName (void) const
  {
  return CString(CA2T(m_strParam.c_str()));
  }


//--------------------------------------------------------------------------
// IsVarlistChanged()      returns TRUE if it is a VarlistChanged event
// ------------------
// Input: -
// Return: TRUE if it is a VarlistChanged event

BOOL CAbkServerEvent::CData::IsVarlistChanged (void) const
  {
  return m_strType.Compare(_T(ABK_SVREVENT_VARLISTCHANGED))==0;
  }


//--------------------------------------------------------------------------
// IsAppChanged()          returns TRUE if event to restart all apps
// --------------
// Input: -
// Return: 

BOOL CAbkServerEvent::CData::IsAppChanged (void) const
  {
  return m_strType.Compare(_T(ABK_SVREVENT_APPCHANGED))==0;
  }


//--------------------------------------------------------------------------
// IsAudioRecReq()       returns TRUE if event for requesting audio recorder
// ---------------
// Input: -
// Return: 

BOOL CAbkServerEvent::CData::IsAudioRecReq (void) const
  {
  return m_strType.Compare(_T(ABK_SVREVENT_AUDIOREC_REQ))==0;
  }



//--------------------------------------------------------------------------
// IsAudioRecStop()        server requests to stop an audio recording
// ----------------
// Input: -
// Return: 

int CAbkServerEvent::CData::IsAudioRecStop (void) const
  {
  return m_strType.Compare(_T(ABK_SVREVENT_AUDIOREC_STOP))==0;
  }

//--------------------------------------------------------------------------
// GetAudioRecId()          returns audio recording ID of the recording or stop request
// --------------
// Input: -
// Return: 

int CAbkServerEvent::CData::GetAudioRecId (void) const
  {
  return (int)m_dParam1;
  }



//--------------------------------------------------------------------------
// GetAudioRecTimeLimit()  returns audio recording time limit the server put in its request
// ----------------------
// Input: -
// Return: 

double CAbkServerEvent::CData::GetAudioRecTimeLimit (void) const
  {
  return (int)m_dParam2;
  }






} // namespace


