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
// Filename:    CrossPlatform.h
// Created:     2012-06-17 (05:08)
// Author:      D. Burger
// Description: cross platform abstraction
//------------------------------------------------------------------------------------------------



#if !defined(__CrossPlatform_h__)
#define __CrossPlatform_h__


#if defined(linux)
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <boost/functional.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#elif defined(_WIN32)
//#include <windows.h>
#ifndef WINCE
#include <process.h>
#endif
#include <winsock.h>
//#include <io.h>
#else
#error Please define your target system
#endif

#include <string>

//data types
#if defined(linux)
#define socket_t int
#define SOCKET int
#define ABK_MUTEX boost::recursive_mutex*
#define ABK_EVENT boost::timed_mutex*
#define ABKMUTEX_INFINITE 0
#define ABK_THREAD_HANDLE boost::thread*
#define ABK_INVALID_THREAD_HANDLE NULL
typedef char TCHAR;   // native character type
typedef const char * LPCTSTR; // pointer to null-terminated const c-style string
#define _T(x) ## x // feeding-trouhg text as we do not have to convert it to UTF-16

#elif defined(_WIN32)
#define socket_t SOCKET
#define SYNC_USE_CS
#ifndef WINCE
#define USE_BEGINTHREADEX
#endif
#define THREAD_CALLCONV __stdcall
typedef unsigned int (THREAD_CALLCONV *PFN_THREAD)(void *);
#ifdef SYNC_USE_CS
  typedef LPCRITICAL_SECTION ABK_MUTEX;
#else
  typedef HANDLE ABK_MUTEX;
#endif
typedef HANDLE ABK_EVENT;
typedef int socklen_t;
#define ABKMUTEX_INFINITE INFINITE
typedef uintptr_t ABK_THREAD_HANDLE;
#define ABK_INVALID_THREAD_HANDLE NULL
#ifndef _T
  #define _T(x)      L ## x
#endif
#else
#error Please define your target system
#endif


// function aliases
#if defined(linux)
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
      ABK_MUTEX m_mutex;
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
      ABK_EVENT m_event;
    public:
      CAbkEvent ();
      CAbkEvent (const CAbkEvent &rOther); // copy constructor
      virtual ~CAbkEvent ();
    public:
      bool Set (void); // sets the event to available
      bool Reset (void); // resets the event to non-available
      bool Wait (int nTimeoutMs) const; // waits for the event to be fired
      bool IsSet (void) const; // returns true if event is set
    };


  class CAbkSingleLock
    {
    private:
      const CAbkMutex *m_pMutex;
    public:
      CAbkSingleLock (const CAbkMutex *pMutex, bool bInitialLock=false, int nInitialLockTimeout=ABKMUTEX_INFINITE);
      virtual ~CAbkSingleLock ();
    public:
      bool Lock (int nTimeout) const; // explicitely locks the object
      bool Unlock (void) const; // explicitely unlocks the object
    };


  } // namespace

#endif
