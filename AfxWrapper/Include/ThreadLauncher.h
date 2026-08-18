//------------------------------------------------------------------------------------------------
//  _____  __   __  _____  _    _        ____              
// |  ___||  \ /  ||  _  \| |  | |      / ___|  _   _  ___ 
// |  __| |   ´   ||  -  /| |  | |  __  \___ \ | | | |/ __|
// | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
// |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
//                                              |___/      
// Filename:    ThreadLauncher.h
// Created:     2014-04-09 (07:47)
// Author:      D. Burger
// Description: thread launching and terminating class
//------------------------------------------------------------------------------------------------




#pragma once

//#include <afxmt.h>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
// Under Linux, when building with Wine, __out is defined, conflicting with boost, so we briefly undefine it here
#pragma push_macro("__out")
#undef __out
#include <boost/thread.hpp>
#pragma pop_macro("__out")

#ifdef _WIN32
#define CALLBACK __stdcall
#else
#define CALLBACK
#endif
typedef DWORD (CALLBACK *THREAD_START_FNC)(LPVOID);

enum ConditionFlags : uint32_t {
  COND_NONE = 0,
  COND_SLEEP = 1,
  COND_TIMER = (1 << 1),
  COND_CUSTOM_0 = (1 << 2),
  COND_CUSTOM_1 = (1 << 3),
  COND_CUSTOM_2 = (1 << 4),
  COND_CUSTOM_3 = (1 << 5),
  COND_CUSTOM_4 = (1 << 6),
  COND_CUSTOM_5 = (1 << 7),
  COND_CUSTOM_6 = (1 << 8),
  COND_DONE = (1 << 9),
};

inline ConditionFlags operator~ (ConditionFlags a) { return (ConditionFlags) ~(uint32_t)a; }
inline ConditionFlags operator| (ConditionFlags a, ConditionFlags b) { return (ConditionFlags) ((uint32_t)a | (uint32_t)b); }
inline ConditionFlags operator& (ConditionFlags a, ConditionFlags b) { return (ConditionFlags) ((uint32_t)a & (uint32_t)b); }
inline ConditionFlags operator^ (ConditionFlags a, ConditionFlags b) { return (ConditionFlags) ((uint32_t)a ^ (uint32_t)b); }
inline ConditionFlags& operator|= (ConditionFlags &a, ConditionFlags b) { return (ConditionFlags&) ((uint32_t&)a |= (uint32_t)b); }
inline ConditionFlags& operator&= (ConditionFlags &a, ConditionFlags b) { return (ConditionFlags&) ((uint32_t&)a &= (uint32_t)b); }
inline ConditionFlags& operator^= (ConditionFlags &a, ConditionFlags b) { return (ConditionFlags&) ((uint32_t&)a ^= (uint32_t)b); }

class CThreadLauncher
  {
  public:


  // data members
  protected:
    THREAD_START_FNC m_pfnStartAddress; // thread function
    void *m_pThreadContext; // context pointer for the thread
    boost::condition_variable m_condEvent;
    boost::mutex m_mutexEvent;

    // Condition to be checked
    ConditionFlags m_nCondEvent;

   // ::CEvent m_evSleep; // event for timing and terminating the thread
    //::CEvent m_evDone; // signalled when thread has terminated (manual-reset-event)
    //HANDLE m_hThread; // !=NULL as long as the thread is running
		boost::thread m_thread;
    CString m_strName; // thread name
    enum {TERMINATE_TIMEOUT_MS=10000};
    void SetSleep();
    void ResetSleep();
    void SetDone();
    void ResetDone();

  // construction/destruction
  public:
    CThreadLauncher (THREAD_START_FNC lpStartAddress, void *pContext, LPCTSTR lpszName = NULL);
    ~CThreadLauncher ();

  // atributes and methods
  public:
    BOOL Start (void); // starts thread
    BOOL Stop (void); // terminates thread. return TRUE if succeeded, FALSE if thread did not terminate properly
    BOOL WaitMs (DWORD dwWaitMs); // waits. returns FALSE if thread shall terminate
    void SignalDone (void); // signals that the thread has terminated
    BOOL IsRunning (void) const; // returns TRUE if thread is running
    BOOL SetThreadPriority (int nPriority); // changes priority of running thread


    void SetConditionFlags(ConditionFlags flags);
    void ResetConditionFlags(ConditionFlags flags);
    ConditionFlags WaitForCondition(ConditionFlags filter, DWORD dwWaitMs = -1);

    //uint32_t& GetCondition();
    boost::condition_variable& GetCondVar();
    boost::mutex& GetMutex();
    //operator ::CEvent *() {return &m_evSleep;}
    //operator HANDLE() {return m_evSleep.m_hObject;}  // returns sleep event handle, allows to be used in WaitForMultipleObjects
  };
