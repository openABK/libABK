// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------------------------
//  _____  __   __  _____  _    _        ____              
// |  ___||  \ /  ||  _  \| |  | |      / ___|  _   _  ___ 
// |  __| |   V   ||  -  /| |  | |  __  \___ \ | | | |/ __|
// | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
// |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
//                                              |___/      
// Filename:    Daq.h
// Created:     2012-05-07 (08:55)
// Author:      D. Burger
// Description: Interface between HTTP and loggers internal data:: data acquisition
//------------------------------------------------------------------------------------------------

#pragma once

#include <iostream>
#include <string>
#include <map>
#include <list>
#include <sstream>

#include "JsonFormatter.h"
#include "VarRef.h"
#include "CrossPlatform.h"


#define ABK_DAQ_TIMEOUT 1000 // timeout for accessing the daq list



namespace Abk {

class CSession;

// data aquisition list, list of variables a client can order
class CDaq
  {
  friend class CSession;
  friend class CLoggerInterface;
  // data members
  protected:
    std::string m_strName; // name of the list, must be synced to the sessions map key
    CSession *m_pOwner; // owning session
    int m_nCycleMs; // update cycle in ms
    CAbkEvent m_eventSleep; // event for signalling termination of logger interface and for sleep timing purposes
    bool m_bMaintainCycleActive; // true if maintain cycle runs
    CAbkMutex m_mutex; // protecting members
    bool m_bTerminate; // true if maintaining thread shall terminate
    bool m_bTransferPending; // true when daq fired and data is not yet transferred to client
    bool m_bAbort;

  // construction/destruction
  public:
    CDaq ();
    CDaq (CDaq &rOther);
    virtual ~CDaq ();
    bool Create (CSession *pOwner, const char *pszName, int nCycleMs);
    void TimerTic (void);  //should be called by an external timer if new data exist. (if an external timer is used instead of the internal daq timer)
    void SetTransferPendingStatus(void) {m_bTransferPending=true;}
  // implementation
  protected:
    static unsigned int THREAD_CALLCONV MaintainCycle (void *pArg); // thread giving the cycle for data updates
    void Fire (void); // fires an event
    bool IsTranferPending (void) const {return m_bTransferPending;} // returns true if daq fired but content not transferred to client
    void ClearTransferPendingStatus (void) {m_bTransferPending=false;} // clears the status of pending transfer

  public:
    CDaq &operator = (const CDaq &rOther);
    CSession *GetOwner (void) const {return m_pOwner;} // returns owning session
    bool Sleep (int nTimeMs) /*const*/; // sleeps or returns if interface shall terminate
    void SetCycle (int nCycleMs); // sets the data update cycle period
    int GetCycle();
    std::string GetName() { return m_strName; };
    bool Lock (int nTimeoutMs=ABK_DAQ_TIMEOUT) const {return m_mutex.Lock(nTimeoutMs);} // locks the object
    void Unlock (void) const {m_mutex.Unlock();} // unlocks the object
    virtual void FormatStatistics (CJsonStreamObject *pDump)=0; // dumps proerties into json stream
    virtual void FormatAllData (CJsonStreamObject *pTarget)=0; // dumps value array into an object
    virtual void FormatAllData (CJsonStreamArray *pTarget)=0; // dumps value array into an array
  };


class CDaqValues : public CDaq
  {
  // data members
  protected:
    std::list <CVarRef *> m_lstVars; // list of variables

  public:
    CDaqValues &operator = (const CDaqValues &rOther);
    bool AddVar (CVarRef *pVar); // adds a variable to the list
    void ClearList (void); // empties the list
    /*virtual*/ void FormatStatistics (CJsonStreamObject *pDump); // dumps proerties into json stream
	virtual void FormatAllData (CJsonStreamObject *pTarget)=0; // dumps value array into an object
	/*virtual*/ void FormatAllData (CJsonStreamArray *pTarget); // dumps value array into an array
  };
  
class CDaqList : public CDaqValues
  {
  friend class CSession;
  friend class CLoggerInterface;
  
  public:
    static CDaqValues* Construct() { return new CDaqList; }
  
    /*virtual*/ void FormatAllData (CJsonStreamObject *pTarget); // dumps value array into an object
  };

class CDaqMAM : public CDaqValues
  {
  friend class CSession;
  friend class CLoggerInterface;
  
  public:
    static CDaqValues* Construct() { return new CDaqMAM; }

    /*virtual*/ void FormatAllData (CJsonStreamObject *pTarget); // dumps value array into an object
  };


class CDaqTrend : public CDaq
  {
  friend class CSession;
  friend class CLoggerInterface;

  // data members
  protected:
    CVarRef *m_pVar; // variable to be traced
    CJsonFormatter m_jfData; // formatted data, queue
    CJsonStreamArray *m_pJaData; // array formatter
    bool m_bEmpty; // false if at least one data element is queued

  // construction/destruction
  public:
    CDaqTrend (const char *pszName);
    CDaqTrend (const std::string &strName);
    CDaqTrend (CDaqTrend &rOther);
    /*virtual*/ ~CDaqTrend ();

  public:
    CDaqTrend &operator = (const CDaqTrend &rOther);
    void SetVar (CVarRef *pVar); // sets the variable to be monitored
    /*virtual*/ void FormatStatistics (CJsonStreamObject *pDump); // dumps proerties into json stream
    /*virtual*/ void FormatAllData (CJsonStreamObject *pTarget); // dumps value array into an object
    /*virtual*/ void FormatAllData (CJsonStreamArray *pTarget); // dumps value array into an array
    CJsonStreamArray *GetQueue (void) {return m_pJaData;}

  protected:
    void Flush(); // flushes the data
  };


  
} // namespace
