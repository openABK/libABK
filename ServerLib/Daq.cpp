// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------------------------
//  _____  __   __  _____  _    _        ____              
// |  ___||  \ /  ||  _  \| |  | |      / ___|  _   _  ___ 
// |  __| |   V   ||  -  /| |  | |  __  \___ \ | | | |/ __|
// | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
// |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
//                                              |___/      
// Filename:    Daq.cpp
// Created:     2012-05-07 (08:55)
// Author:      D. Burger
// Description: Interface between HTTP and loggers internal data:: data acquisition
//------------------------------------------------------------------------------------------------

#include "stdafx.h"
#include "Daq.h"

#include "LoggerIf.h"
#include "Session.h"
#include "ValuesFromSpec.h"
#include "JsonParser.h"
#include <assert.h>


namespace Abk {


//--------------------------------------------------------------------------
// CDaq()            Constructor of CDaq
// ------------
// Input: -
// Return: 

CDaq::CDaq()
  : m_bAbort(false)
  {
  m_bMaintainCycleActive=false;
  m_bTransferPending=false;
  }

CDaq::CDaq (CDaq &rOther)
  {
  m_bMaintainCycleActive=false;
  m_bTransferPending=false;
  }

//--------------------------------------------------------------------------
// ~CDaq()           Destructor of CDaq
// -------------
// Input: -
// Return: 

CDaq::~CDaq ()
  {
  m_bTerminate=true;
  // m_eventSleep.Abort();
  if(m_bMaintainCycleActive)
    m_eventSleep.Set(); // tell thread to terminate
  while(m_bMaintainCycleActive)
    AbkSleepMs(10);
  }


//--------------------------------------------------------------------------
// Create()                creates the list
// --------
// Input: pOwner = owning session
//        strName = name of the list
//        nCycleMs = update cycle in terms of ms
// Return: true on success (always)

bool CDaq::Create (CSession *pOwner, const char *pszName, int nCycleMs)
  {
  m_pOwner=pOwner;
  m_strName=pszName;
  m_nCycleMs=nCycleMs;
  // when adding new members, dont forget to
  // a) extend the copy operator

  // m_mutexSleep.Lock(ABKMUTEX_INFINITE);
  m_bTerminate=false;
  bool bUseExternalTimer = false;
  // inform loggerinterface about the new daq. loggerinterface may instruct to use a external timer
  m_pOwner->GetOwner()->OnDAQCreate(m_pOwner->GetSessionId(), pszName, bUseExternalTimer);
  if(!bUseExternalTimer)
    {
    m_bMaintainCycleActive = true;
    AbkStartThread(MaintainCycle, this);
    }
  return true;
  }


//--------------------------------------------------------------------------
// operator()              tells owning session that cycle timed-out
// ----------
// Input: -
// Return: -

void CDaq::TimerTic (void)
  {
  Lock(); // guard fire-operation and the corresponding flag
  m_bTransferPending = true; // tell the session to transfer me to the client
  GetOwner()->FireEvent(); // periodically fire the event
  Unlock();
  }


//--------------------------------------------------------------------------
// operator()              copy operator
// ----------
// Input: rOther = copy source
// Return: reference to this object

CDaq &CDaq::operator = (const CDaq &rOther)
  {
  m_pOwner=rOther.m_pOwner;
  m_strName=rOther.m_strName;
  m_nCycleMs=rOther.m_nCycleMs;
  return *this;
  }


//--------------------------------------------------------------------------
// Sleep()                 sleeps or returns if list shall terminate
// -------
// Input: nTimeMs = time to wait in ms
// Return: true if time elapsed, false if abort operation desired or abnormal triggering

bool CDaq::Sleep (int nTimeMs) /*const*/
  {
  return !m_eventSleep.Wait(nTimeMs);
  }


//--------------------------------------------------------------------------
// SetCycle()              sets the data update cycle period
// ----------
// Input: nCycleMs = new update cycle period
// Return: -

void CDaq::SetCycle (int nCycleMs)
  {
  // inform loggerinterface about the requested cycle. dataloggerinterface may reject the new cycle.
  m_pOwner->GetOwner()->OnDAQCycleUpdate(m_pOwner->GetSessionId(), m_strName.c_str(), nCycleMs);
  if(!Lock())
    return;
  m_nCycleMs=nCycleMs;
  Unlock();
  Fire();
  }

int CDaq::GetCycle()
{
   CAbkSingleLock guard(&m_mutex, true, 1000);
   return m_nCycleMs;
}



//--------------------------------------------------------------------------
// Fire()                  fires an event
// ------
// Input: -
// Return: -

void CDaq::Fire (void)
  {
  m_eventSleep.Set(); // cycle changed and may got shorter => trigger the list event
  m_eventSleep.Reset();
  }


//--------------------------------------------------------------------------
// MaintainCycle()         thread giving the cycle for data updates
// ---------------
// Input: pArg = pointer to DAQ list
// Return: always 0

/*static*/ unsigned int THREAD_CALLCONV CDaq::MaintainCycle (void *pArg)
  {
  assert(pArg);
  CDaq *pThis=reinterpret_cast<CDaq *>(pArg);
  for(;;)
    {
    assert(pThis);
    int nCycleMs=0;
    pThis->Lock();
    nCycleMs=pThis->m_nCycleMs;
    pThis->Unlock();
    if(!pThis->Sleep(nCycleMs))
      {
      if(pThis->m_bTerminate)
        break; // list shall terminate
      }
    pThis->TimerTic(); // guard fire-operation and the corresponding flag
    }
  pThis->m_bMaintainCycleActive=false;
  return 0;
  }










//--------------------------------------------------------------------------
// operator()              copy operator
// ----------
// Input: rOther = copy source
// Return: reference to this object

CDaqValues &CDaqValues::operator = (const CDaqValues &rOther)
  {
  CDaq::operator=(rOther);
  m_lstVars=rOther.m_lstVars;
  return *this;
  }


//--------------------------------------------------------------------------
// AddVar()                adds a variable to the list end
// --------
// Input: pVar = variable to be added
// Return: true on success, false on error

bool CDaqValues::AddVar (CVarRef *pVar)
  {
  if(!Lock())
    return false;
  m_lstVars.push_back(pVar);
  Unlock();
  return true;
  }


//--------------------------------------------------------------------------
// ClearList()             empties the list
// -----------
// Input: -
// Return: 

void CDaqValues::ClearList (void)
  {
  if(!Lock())
    return;
  m_lstVars.clear();
  Unlock();
  }


//--------------------------------------------------------------------------
// FormatStatistics()            dumps proerties into json stream
// ------------
// Input: pDump = json stream to dump to
// Return: -

/*virtual*/ void CDaqValues::FormatStatistics (CJsonStreamObject *pDump)
  {
  if(!Lock())
    return;
  pDump->WriteValue("Name",m_strName);
  pDump->WriteValue("Cycle",m_nCycleMs);
  int nVarCount=m_lstVars.size();
  pDump->WriteValue("VariableCount",nVarCount);
  Unlock();
  }

//--------------------------------------------------------------------------
// FormatAllData()         dumps value array into an array
// ---------------
// Input: pTarget = array into which the data shall be formatted into
// Return: 

/*virtual*/ void CDaqValues::FormatAllData (CJsonStreamArray *pTarget)
  {
  CJsonStreamObject joData(pTarget);
  FormatAllData(&joData);
  }


//--------------------------------------------------------------------------
// FormatAllData()         dumps value array into an object
// ---------------
// Input: pTarget = object into which the data shall be formatted into
// Return: -

/*virtual*/ void CDaqList::FormatAllData (CJsonStreamObject *pTarget)
  {
  CJsonStreamArray jaData(pTarget,m_strName);
  std::list<CVarRef *>::const_iterator iterVar=m_lstVars.begin();
  int nVar=0;
  for(;iterVar!=m_lstVars.end();iterVar++)
    {
    CVarRef *pVarRef=*iterVar;
    jaData.BeginMember();
    pVarRef->OnFormatValue(jaData); // let the variable format its value
    nVar++;
    }
  }

//--------------------------------------------------------------------------
// FormatAllData()         dumps value array into an object
// ---------------
// Input: pTarget = object into which the data shall be formatted into
// Return: -

/*virtual*/ void CDaqMAM::FormatAllData (CJsonStreamObject *pTarget)
  {
  CJsonStreamArray jaVars(pTarget,m_strName); // array listing all variables
  std::list<CVarRef *>::const_iterator iterVar=m_lstVars.begin();
  int nVar=0;
  for(;iterVar!=m_lstVars.end();iterVar++)
    {
    if(1)
      {
      CJsonStreamArray jaData(&jaVars); // array listing all data (i.e. Min, Avg, Max)
      CVarRef *pVarRef=*iterVar;
      pVarRef->OnFormatMAM(jaData); // let the variable format its 3 values (min, avg, max)
      }
    nVar++;
    }
  }






//--------------------------------------------------------------------------
// CDaqTrend()             Constructor of CDaqTrend
// -----------
// Input: -
// Return: 

CDaqTrend::CDaqTrend (const char *pszName)
  : CDaq() 
  {
  m_pVar=NULL;
  m_pJaData=NULL;
  m_strName=pszName;
  Flush();
  }

CDaqTrend::CDaqTrend (const std::string &strName)
  : CDaq() 
  {
  m_pVar=NULL;
  m_pJaData=NULL;
  m_strName=strName;
  Flush();
  }

//--------------------------------------------------------------------------
// CDaqTrend()             Constructor of CDaqTrend
// -----------
// Input: rOther = 
// Return: 

CDaqTrend::CDaqTrend (CDaqTrend &rOther)
  : CDaq(rOther)
  {
  m_pVar=NULL;
  m_pJaData=NULL;
  Flush();
  }



//--------------------------------------------------------------------------
// ~CDaqTrend()            Destructor of CDaqTrend
// ------------
// Input: -
// Return: 

/*virtual*/ CDaqTrend::~CDaqTrend ()
  {
  if(m_pJaData)
    delete m_pJaData;
  assert(m_pVar);
  m_pVar->RemoveTrend(this);
  }


//--------------------------------------------------------------------------
// operator()              copy operator
// ----------
// Input: rOther = copy source
// Return: reference to this object

CDaqTrend &CDaqTrend::operator = (const CDaqTrend &rOther)
  {
  CDaq::operator=(rOther);
  m_pVar=rOther.m_pVar;
 // no need to copy the formatter   m_jfData=rOther.m_jfData;
  return *this;
  }


//--------------------------------------------------------------------------
// SetVar()                sets the variable to be monitored
// --------
// Input: pVar = variable to be set
// Return: -

void CDaqTrend::SetVar (CVarRef *pVar)
  {
  assert(pVar);
  m_pVar=pVar;
  pVar->AddTrend(this);
  }


//--------------------------------------------------------------------------
// FormatStatistics()      dumps proerties into json stream
// ------------------
// Input: pDump = 
// Return: 

/*virtual*/ void CDaqTrend::FormatStatistics (CJsonStreamObject *pDump)
  {
  
  }



//--------------------------------------------------------------------------
// FormatAllData()         dumps value array into an object
// ---------------
// Input: pTarget = object into which the data shall be formatted into
// Return: 

/*virtual*/ void CDaqTrend::FormatAllData (CJsonStreamObject *pTarget)
  {
  m_jfData.Close();
  pTarget->WriteValue(const_cast<const CJsonFormatter&>(m_jfData)); // why in the hell we need a const_cast here??
  Flush();
  }



//--------------------------------------------------------------------------
// FormatAllData()         dumps value array into an array
// ---------------
// Input: pTarget = array into which the data shall be formatted into
// Return: 

/*virtual*/ void CDaqTrend::FormatAllData (CJsonStreamArray *pTarget)
  {
  CJsonStreamObject joData(pTarget);
  FormatAllData(&joData);
  }


//--------------------------------------------------------------------------
// Flush()                 flushes the data
// -------
// Input: -
// Return: 

void CDaqTrend::Flush ()
  {
  if(m_pJaData)
    delete m_pJaData;
  m_jfData.InitNew();
  m_pJaData=new CJsonStreamArray(&m_jfData,m_strName);
  m_bEmpty=true;
  }






} // namespace

