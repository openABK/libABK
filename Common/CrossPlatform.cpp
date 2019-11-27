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
// Filename:    CrossPlatform.cpp
// Created:     2012-06-17 (05:08)
// Author:      D. Burger
// Description: cross platform abstraction
//------------------------------------------------------------------------------------------------




#include "stdafx.h"
#include <CrossPlatform.h>
#include <assert.h>
#include <iostream>




namespace Abk
  {



//--------------------------------------------------------------------------
// AbkStartThread()        starts a thread
// ----------------
// Input: start_address = thread function address
//        pArgs = argument passed to the thread function
// Return: handle of the thread, ABK_INVALID_THREAD_HANDLE on error

ABK_THREAD_HANDLE AbkStartThread (PFN_THREAD pfnThread, void *pArgs)
  {
  #if defined(PREFER_BOOST_PLATFORM)
	return boost::thread(pfnThread, pArgs);
  #elif defined(_WIN32)
#ifdef USE_BEGINTHREADEX
  uintptr_t hThread=_beginthreadex(NULL,0,pfnThread,pArgs,0,NULL);
  return (ABK_THREAD_HANDLE)hThread;
#else
  return (ABK_THREAD_HANDLE)CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)pfnThread,pArgs,0,NULL);
  // alternative, but not available on WIN_CE:  return _beginthread(start_address,0,pArgs);
#endif
  #else
  #error Please define your target system
  #endif
  }



//--------------------------------------------------------------------------
// AbkKillThread()         kills a running thread
// ---------------
// Input: hThread = handle of thread to be killed
// Return: -

void AbkKillThread (ABK_THREAD_HANDLE hThread)
  {
  #if defined(PREFER_BOOST_PLATFORM)
		hThread.interrupt();
		hThread.join();
  #elif defined(_WIN32)
    CloseHandle((HANDLE)hThread);
  #else
    #error Please define your target system
  #endif
  }



//--------------------------------------------------------------------------
// AbkSleepMs()            sleeps for specified time
// ------------
// Input: nDelayMs = delay in terms of milliseconds
// Return: -

void AbkSleepMs (int nDelayMs)
  {
  #if defined(PREFER_BOOST_PLATFORM)
	boost::this_thread::sleep(boost::posix_time::milliseconds(nDelayMs));
  #elif defined(_WIN32)
    Sleep(nDelayMs);
  #else
    #error Please define your target system
  #endif
  }



//--------------------------------------------------------------------------
// AbkGetOwnIpAddress()    returns the ip address of the main ethernet adapter
// --------------------
// Input: -
// Return: the ip address of the main ethernet adapter

const std::string &AbkGetOwnIpAddress (void)
  {
  // see also http://tangentsoft.net/wskfaq/examples/ipaddr.html
  static std::string strIpAddr;
  char ac[80];

  if(strIpAddr.empty()) // if not yet retrieved
    {
    do
      {
      if(::gethostname(ac, sizeof(ac)) == SOCKET_ERROR)
        {
        // cerr << "Error " << WSAGetLastError() << " when getting local host name." << endl;
        break;
        }
      // cout << "Host name is " << ac << "." << endl;
      struct hostent *phe = gethostbyname(ac);
      if (phe == 0)
        {
        // cerr << "Yow! Bad host lookup." << endl;
        break;
        }

      for (int i = 0; phe->h_addr_list[i] != 0; ++i)
        {
        struct in_addr addr;
        memcpy(&addr, phe->h_addr_list[i], sizeof(struct in_addr));
        strIpAddr=inet_ntoa(addr);
        // cout << "Address " << i << ": " << inet_ntoa(addr) << endl;
        }
      } while(0);
    }
  return strIpAddr;
  }



//--------------------------------------------------------------------------
// AbkMutexInit()          initializes a mutex
// --------------
// Input: mutex = handle of mutex to be initialized
// Return: true on success, false on error

bool AbkMutexInit (ABK_MUTEX_REF mutex)
  {
  #if defined(PREFER_BOOST_PLATFORM)
	return true;
  #elif defined(_WIN32)
    #ifdef SYNC_USE_CS
    mutex=new CRITICAL_SECTION; //  mutex = CreateMutex(0, FALSE, 0);
    if(mutex)
      InitializeCriticalSection(mutex);
    return (mutex!=0);
    #else
    mutex = CreateMutex(0, FALSE, 0);
    return (mutex!=0);
    #endif
  #endif
  return false;
  }



//--------------------------------------------------------------------------
// AbkMutexLock()          locks a mutex
// --------------
// Input: mutex = handle of mutex to be locked
//        nTimeout = timeout in ms
// Return: true on success, false on timeout

bool AbkMutexLock (ABK_MUTEX_REF mutex, int nTimeout)
  {
  #if defined(PREFER_BOOST_PLATFORM)
	mutex.lock();
	return true;
  #elif defined(_WIN32)
    #ifdef SYNC_USE_CS
    EnterCriticalSection(mutex);
    return true;
    #else
    DWORD dwReturn=WaitForSingleObject(mutex,nTimeout);
    return (dwReturn==WAIT_OBJECT_0?true:false);
    #endif
  #endif
  return false;
  }



//--------------------------------------------------------------------------
// AbkMutexUnlock()        releases a mutex
// ----------------
// Input: mutex = handle of mutex to be unlocked
// Return: true on success, false on error

bool AbkMutexUnlock (ABK_MUTEX_REF mutex)
  {
  #if defined(PREFER_BOOST_PLATFORM)
	mutex.unlock();
	return true;
  #elif defined(_WIN32)
    #ifdef SYNC_USE_CS
    LeaveCriticalSection(mutex);
    return true;
    #else
    return ReleaseMutex(mutex)==TRUE;
    #endif
  #endif
  return false;
  }



//--------------------------------------------------------------------------
// AbkMutexDestroy()       destroys a mutex
// -----------------
// Input: mutex = handle of mutex to be destroyed
// Return: true on success, false on error

bool AbkMutexDestroy (ABK_MUTEX_REF mutex)
  {
  #if defined(PREFER_BOOST_PLATFORM)
	return true;
  #elif defined(_WIN32)
    #ifdef SYNC_USE_CS
    DeleteCriticalSection(mutex);
    delete mutex;
    #else
    return CloseHandle(mutex)==TRUE;
    #endif
  #endif
  return false;
  }












////////////////////////////////////////////////////////////////////////////
//
// Mutex class
//
////////////////////////////////////////////////////////////////////////////


//--------------------------------------------------------------------------
// CAbkMutex()             Constructor of CAbkMutex
// -----------
// Input: -
// Return: -

CAbkMutex::CAbkMutex()
  {
  AbkMutexInit(m_mutex);
  m_nCount=0;
  }



//--------------------------------------------------------------------------
// CAbkMutex()             Constructor of CAbkMutex
// -----------
// Input: rOther = copy source
// Return: 

CAbkMutex::CAbkMutex (const CAbkMutex &rOther)
  {
  AbkMutexInit(m_mutex);
  m_nCount=0;
  }



//--------------------------------------------------------------------------
// ~CAbkMutex()            Destructor of CAbkMutex
// ------------
// Input: -
// Return: -

CAbkMutex::~CAbkMutex()
  {
  assert(m_nCount==0);
  AbkMutexDestroy(m_mutex);
  }



//--------------------------------------------------------------------------
// Lock()                  locks the mutex
// ------
// Input: nTimeout = timeout value in ms. ABKMUTEX_INFINITE for infinite timeout
// Return: true on success, false on error or timeout

bool CAbkMutex::Lock (int nTimeout) const
  {
  bool bResult=AbkMutexLock(m_mutex, nTimeout);
  if(bResult)
    {
    (const_cast<CAbkMutex *>(this))->m_nCount++; // only for debugging purposes (violates the const rules)
    }
  return bResult;
  }



//--------------------------------------------------------------------------
// Unlock()                unlocks the mutex
// --------
// Input: -
// Return: true on success, false on error

bool CAbkMutex::Unlock (void) const
  {
  assert(m_nCount>0); // //#### 25.01.13: assert herausgenommen
    {
    (const_cast<CAbkMutex *>(this))->m_nCount--; // only for debugging purposes (violates the const rules)
    }
  bool bResult=AbkMutexUnlock(m_mutex);
  assert(bResult);
  return bResult;
  }



//--------------------------------------------------------------------------
// IsLocked()              returns true if mutex is locked
// ----------
// Input: -
// Return: true if mutex is locked

bool CAbkMutex::IsLocked (void) const
  {
  return m_nCount!=0;
  }







////////////////////////////////////////////////////////////////////////////
//
// Lock object class
//
////////////////////////////////////////////////////////////////////////////


//--------------------------------------------------------------------------
// CAbkSingleLock()        Constructor of CAbkSingleLock
// ----------------
// Input: pMutex = object used to lock
//        bInitialLock = initial locking state
// Return: -

CAbkSingleLock::CAbkSingleLock (const CAbkMutex *pMutex, bool bInitialLock/*=false*/, int nInitialLockTimeout/*=ABKMUTEX_INFINITE*/)
  {
  m_pMutex=pMutex;
  if(bInitialLock)
    {
    bool bResult=Lock(nInitialLockTimeout);
//    assert(bResult);
    }
  }



//--------------------------------------------------------------------------
// ~CAbkSingleLock()       Destructor of CAbkSingleLock
// -----------------
// Input: -
// Return: -

CAbkSingleLock::~CAbkSingleLock ()
  {
  assert(m_pMutex->IsLocked());
  Unlock();
  }


//--------------------------------------------------------------------------
// Lock()                  explicitely locks the object
// ------
// Input: nTimeout = timeout in ms
// Return: true on success, false if mutex could not be acquired after time-out

bool CAbkSingleLock::Lock (int nTimeout) const
  {
  return m_pMutex->Lock(nTimeout);  
  }




//--------------------------------------------------------------------------
// Unlock()                explicitely unlocks the object
// --------
// Input: -
// Return: 

bool CAbkSingleLock::Unlock (void) const
  {
  return m_pMutex->Unlock();
  }





////////////////////////////////////////////////////////////////////////////
//
// Event class
//
////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------
// CAbkEvent()             Constructor of CAbkEvent
// -----------
// Input: -
// Return: -

CAbkEvent::CAbkEvent ()
  {
  #if defined(PREFER_BOOST_PLATFORM)
	//NOTHING TO DO HERE
  //  std::cout << "cabkevent constructor called\n";
	m_eventCond = false;
  #elif defined(_WIN32)
    m_event=CreateEvent(NULL,TRUE,FALSE,NULL);
  #else
    #error Please define your target system
  #endif
  }

#ifndef PREFER_BOOST_PLATFORM
//--------------------------------------------------------------------------
// CAbkEvent()             Constructor of CAbkEvent
// -----------
// Input: rOther = copy source
// Return: 

CAbkEvent::CAbkEvent (const CAbkEvent &rOther)
  {
  #if defined(PREFER_BOOST_PLATFORM)
    //NOTHING TO DO HERE
    //std::cout << "cabkevent copy-constructor called\n";
  #elif defined(_WIN32)
    m_event=CreateEvent(NULL,TRUE,FALSE,NULL);
  #else
    #error Please define your target system
  #endif
  }
#endif


//--------------------------------------------------------------------------
// ~CAbkEvent()            Destructor of CAbkEvent
// ------------
// Input: -
// Return: 

CAbkEvent::~CAbkEvent ()
  {
  #if defined(PREFER_BOOST_PLATFORM)
    //NOTHING TO DO HERE
    //std::cout << "cabkevent destructor called\n";
  #elif defined(_WIN32)
    CloseHandle(m_event);
  #else
    #error Please define your target system
  #endif
  }


//--------------------------------------------------------------------------
// Set()                   sets the event to available
// -----
// Input: -
// Return: true on success, false on error

bool CAbkEvent::Set (void)
  {
  #if defined(PREFER_BOOST_PLATFORM)
	boost::lock_guard < boost::mutex > lock(m_eventMutex);
	m_eventCond = true;
	m_event.notify_all();
	return true;
  #elif defined(_WIN32)
    return SetEvent(m_event)!=0;
  #else
    #error Please define your target system
  #endif
  }


//--------------------------------------------------------------------------
// Reset()                 resets the event to non-available
// -------
// Input: -
// Return: true on success, false on error

bool CAbkEvent::Reset (void)
  {
  #if defined(PREFER_BOOST_PLATFORM)
	boost::lock_guard< boost::mutex > lock(m_eventMutex);
	m_eventCond = false;
	return true;
  #elif defined(_WIN32)
    return ResetEvent(m_event)!=0;
  #else
    #error Please define your target system
  #endif
  }


//--------------------------------------------------------------------------
// Wait()                  waits for the event to be fired
// ------
// Input: nTimeoutMs = time-out in ms
// Return: true if the event was fired, false if timed-out or error

bool CAbkEvent::Wait (int nTimeoutMs)
  {
  #if defined(PREFER_BOOST_PLATFORM)
	boost::posix_time::milliseconds duration(nTimeoutMs);

	{
		boost::unique_lock<boost::mutex> lock(m_eventMutex);
		bool bSuccess = false;
		// Make sure last event was sleep event
		while (!(m_eventCond))
		{
			// Waiting for signal (mutex is unlocked during wait, so m_lastEvent can update)
			if (m_event.timed_wait<boost::posix_time::milliseconds>(lock, duration))
			{
				bSuccess = true;
				m_eventCond = false;
				break;
			}
			else
			{
				bSuccess = false;
				break;
			}
		}

		return bSuccess;
	} // unlock mutex
  #elif defined(_WIN32)
    return WaitForSingleObject(m_event,nTimeoutMs)==WAIT_OBJECT_0;
  #else
    #error Please define your target system
  #endif
  }


//--------------------------------------------------------------------------
// IsSet()                 returns true if event is set
// -------
// Input: -
// Return: 

bool CAbkEvent::IsSet (void)
  {
  #if defined(PREFER_BOOST_PLATFORM)
	boost::lock_guard < boost::mutex > lock(m_eventMutex);
	return m_eventCond;
  #elif defined(_WIN32)
    return WaitForSingleObject(m_event,0)==WAIT_OBJECT_0; // returns true if event is set, tested with a zero time-out
  #else
    #error Please define your target system
  #endif    
  }



  } // namespace


