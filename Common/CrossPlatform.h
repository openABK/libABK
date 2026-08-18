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
// Filename:    CrossPlatform.h
// Created:     2012-06-17 (05:08)
// Author:      D. Burger
// Description: cross platform abstraction
//------------------------------------------------------------------------------------------------



#if !defined(__CrossPlatform_h__)
#define __CrossPlatform_h__

#ifdef USE_BOOST
#define PREFER_BOOST_PLATFORM
#endif

#ifdef LINUX
#define PREFER_BOOST_PLATFORM
#endif


#if defined(PREFER_BOOST_PLATFORM)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#ifdef LINUX
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include <boost/functional.hpp>
#include <boost/function.hpp>
#include <boost/bind/bind.hpp>
#include <boost/thread.hpp>
#elif defined(_WIN32)
//#include <windows.h>
#if !defined(WINCE) && !defined(__WINE__)
#include <process.h>
#endif
#include <winsock2.h>
//#include <io.h>
#else
#error Please define your target system
#endif

#include <string>

//data types
#if defined(PREFER_BOOST_PLATFORM)
#define socket_t int
#define SOCKET int
typedef boost::recursive_mutex ABK_MUTEX;

typedef boost::condition_variable ABK_EVENT;
#define ABKMUTEX_INFINITE 0
#define ABK_THREAD_HANDLE boost::thread
#define ABK_INVALID_THREAD_HANDLE NULL


#ifdef LINUX
// Ensure that afxwrapper from MFC tools is used
#ifndef _T
#error "_T undefined"
#endif
#endif

#elif defined(_WIN32)
#define socket_t SOCKET
#define SYNC_USE_CS
#ifndef WINCE
#define USE_BEGINTHREADEX
#endif
#ifdef SYNC_USE_CS
  typedef LPCRITICAL_SECTION ABK_MUTEX;
#else
  typedef HANDLE ABK_MUTEX;
#endif
typedef HANDLE ABK_EVENT;
#ifndef __WINE__
typedef int socklen_t;
#endif
#define ABKMUTEX_INFINITE INFINITE
typedef uintptr_t ABK_THREAD_HANDLE;
#define ABK_INVALID_THREAD_HANDLE NULL
#ifndef _T
  #define _T(x)      L ## x
#endif
#else
#error Please define your target system
#endif

typedef ABK_MUTEX& ABK_MUTEX_REF;

#if defined(_WIN32)
#define THREAD_CALLCONV __stdcall
#else
#define THREAD_CALLCONV
#endif
typedef unsigned int (THREAD_CALLCONV *PFN_THREAD)(void *);


// function aliases
#if defined(LINUX)
  #define vsnprintf_tchar vsnprintf // vsnprintf on TCHAR type is same as vsnprintf
#elif defined(_WIN32)
  #define vsnprintf _vsnprintf_s
  #define _snscanf _snscanf_s
#else
#error Please define your target system
#endif


namespace Abk
  {

  ABK_THREAD_HANDLE AbkStartThread (PFN_THREAD pfnThread, void *pArgs);
  void AbkKillThread (ABK_THREAD_HANDLE hThread);
  void AbkSleepMs (int nDelayMs);
  const std::string &AbkGetOwnIpAddress (void);




  class CAbkMutex
    {
    private:
			mutable ABK_MUTEX m_mutex;
      int m_nCount;
    public:
      CAbkMutex();
	  CAbkMutex (const CAbkMutex &rOther); // copy constructor
      virtual ~CAbkMutex();
    public:
      bool Lock (int nTimeout) const;  // locks the mutex
      bool Unlock (void) const; // unlocks the mutex
      bool IsLocked (void) const; // returns true if mutex is locked
    };


  class CAbkEvent
    {
    private:
      mutable ABK_EVENT m_event;
#ifdef PREFER_BOOST_PLATFORM
			mutable boost::mutex m_eventMutex;
			mutable bool m_eventCond;
#endif
    public:
      CAbkEvent ();
#ifndef PREFER_BOOST_PLATFORM
      CAbkEvent (const CAbkEvent &rOther); // copy constructor
#endif
      virtual ~CAbkEvent ();
    public:
      bool Set (void); // sets the event to available
      bool Reset (void); // resets the event to non-available
      bool Wait (int nTimeoutMs); // waits for the event to be fired
      bool IsSet (void); // returns true if event is set
    };


  class CAbkSingleLock
    {
    private:
      mutable const CAbkMutex *m_pMutex;
    public:
      CAbkSingleLock (const CAbkMutex *pMutex, bool bInitialLock=false, int nInitialLockTimeout=ABKMUTEX_INFINITE);
      virtual ~CAbkSingleLock ();
    public:
      bool Lock (int nTimeout) const; // explicitely locks the object
      bool Unlock (void) const; // explicitely unlocks the object
    };


  } // namespace

#endif
