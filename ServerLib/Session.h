// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------------------------
//  _____  __   __  _____  _    _        ____              
// |  ___||  \ /  ||  _  \| |  | |      / ___|  _   _  ___ 
// |  __| |   V   ||  -  /| |  | |  __  \___ \ | | | |/ __|
// | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
// |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
//                                              |___/      
// Filename:    Session.h
// Created:     2012-08-17 (05:12)
// Author:      D. Burger
// Description: ABK session, used as counterpart for each connected client
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
#include "Daq.h"
#include "EventQueue.h"
#include "LoggerIf.h"

namespace Abk {


// session a client can open
class CSession
  {
  friend class CLoggerInterface;
  // data members
  protected:
    CLoggerInterface *m_pOwner; // owning logger interface
    CLoggerInterface::CClientUid m_clientUid; // characteristics of the client the session is installed for
    //int m_nSessionId; // id of the session within the logger interface
    //std::string m_strClientAddr; // address of the client
    //std::string m_strClientClass; // class of the client, e.g. "display"
    //std::string m_strClientType; // type of the client e.g. "mytronics_superdisplay3000"
    //std::string m_strClientSerial; // serial number of the client, may also be the mac address or other unique id
    CAbkMutex m_mutex; // mutex protecting the session
    std::map <std::string,CDaq *> m_mapDaq; // map of daq-lists
    int m_nRemainingS; // remaining life time of the session in seconds
    CAbkEvent m_event; // event mechanism, gets fired when session got an event
    CAbkEvent m_eventSleep; // event for signalling termination of session and for sleep timing purposes
    bool m_bRequestInProgress; // true if an http request is in progress
    bool m_bExpThreadInProgress; // true as long the expiration thread is in progress
    CEventQueue m_queueEvents; // events, queued in a json formatter
    bool m_bFirstPoll;
   
  // construction/destruction
  public:
    CSession (CLoggerInterface *pOwner, const CLoggerInterface::CClientUid &uidClient);
    virtual ~CSession ();

  // implementation
  protected:
    CDaq *GetDaq (const char *pszListName) const; // returns pointer to an daq-list
    CDaqValues *PutDaqList (const char *pszListName, PFN_CREATE_DAQ); // adds an empty daq list to the session or returns an existing
    CDaqTrend *PutDaqTrend (const char *pszListName); // adds an empty daq trend to the session or returns an existing
    bool DeleteDaqList (const char *pszListName); // deletes an daq-list
  public:
	  bool DeleteDaqLists (); // deletes all daq-lists
  protected:
    static unsigned int THREAD_CALLCONV MaintainExpirationThread (void *pArg); // thread maintaining session timeouts
    void Refresh (void); // refreshes the session timeout
  public:
    int GetSessionId (void) const {return m_clientUid.m_nSession;} // returns the session id
    const CLoggerInterface::CClientUid GetClientUid (void) const {return m_clientUid;} // returns the client characteristics
    bool Lock (int nTimeout) {return m_mutex.Lock(nTimeout);} // locks the session
    bool Unlock (void) {return m_mutex.Unlock();} // unlocks the session
    bool Sleep (int nTimeMs); // sleeps or returns if session shall terminate
    bool WaitEvent (int nTimeoutMs); // waits for event, returns false on timeout
    void FireEvent (void); // fires the event to trigger thread waiting in WaitEvent
	  bool IsFirstPoll (void); //returns whether this is the first call to this function
    CLoggerInterface *GetOwner (void) {return m_pOwner;} // returns owning logger interface
    void FormatStatistics (CJsonStreamObject *pDump); // dumps proerties into json stream
    void FireEventToClient (int nSender, time_t tmSent, const char *pszEventType, const std::string &strParam, double dParam1, double dParam2, bool bForcePrimaryRole=false); // fires an event to the client
  };


  } // namespace
