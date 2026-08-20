//------------------------------------------------------------------------------------------------
//  _____  __   __  _____  _    _        ____              
// |  ___||  \ /  ||  _  \| |  | |      / ___|  _   _  ___ 
// |  __| |   V   ||  -  /| |  | |  __  \___ \ | | | |/ __|
// | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
// |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
//                                              |___/      
// Filename:    ThreadLauncher.cpp
// Created:     2014-04-09 (07:47)
// Author:      D. Burger
// Description: thread launching and terminating class
//------------------------------------------------------------------------------------------------


#include "stdafx.h"

#include "ThreadLauncher.h"




/** Sets the last Event and notifies every thread waiting for the sleep condition */
void CThreadLauncher::SetSleep()
{
  {
    boost::lock_guard lock(m_mutexEvent);
    m_nCondEvent |= COND_SLEEP;
  }
  m_condEvent.notify_all();
}

void CThreadLauncher::ResetSleep()
{
  {
    boost::lock_guard lock(m_mutexEvent);
    m_nCondEvent &= ~COND_SLEEP;
  }
  // No notify, as condition will not be true, so a wakeup is not required
}

void CThreadLauncher::SetDone()
{
  {
    boost::lock_guard lock(m_mutexEvent);
    m_nCondEvent |= COND_DONE;
  }
  m_condEvent.notify_all();
}

void CThreadLauncher::ResetDone()
{
  {
    boost::lock_guard lock(m_mutexEvent);
    m_nCondEvent &= ~COND_DONE;
  }
  // Don't wake threads up, condition won't be true anyway
}

//--------------------------------------------------------------------------
// CThreadLauncher()       Constructor of CThreadLauncher
// -----------------
// Input: lpStartAddress = thread to be started when Start() is called
//        pContext = context for the thread
// Return: 

CThreadLauncher::CThreadLauncher (THREAD_START_FNC lpStartAddress, void *pContext, LPCTSTR pszName)
  {
  m_pfnStartAddress = lpStartAddress;
  m_pThreadContext = pContext;
  m_nCondEvent = COND_NONE;
  if (pszName)
    m_strName = pszName;
  }




//--------------------------------------------------------------------------
// ~CThreadLauncher()      Destructor of CThreadLauncher
// ------------------
// Input: -
// Return: 

 CThreadLauncher::~CThreadLauncher ()
   {
   Stop();
   }


//--------------------------------------------------------------------------
// Start()                 starts thread
// -------
// Input: -
// Return: TRUE if thread is running (could be started or was already running)

BOOL CThreadLauncher::Start (void)
  {
  if(IsRunning())
    return TRUE; // successfully done nothing, thread is running

  ResetDone();
  ResetSleep();

	m_thread = boost::thread(m_pfnStartAddress, m_pThreadContext);
#ifdef LINUX
  // Set Thread name if applicable
  if (!m_strName.IsEmpty())
  {
    pthread_t thread = m_thread.native_handle();
    pthread_setname_np(thread, m_strName);
  }
#endif

	// Make sure this is a valid id
  return m_thread.get_id() != boost::thread::id();
  }




//--------------------------------------------------------------------------
// Stop()             terminates thread. return TRUE if succeeded, FALSE if thread did not terminate properly
// ------
// Input: -
// Return: TRUE on success, FALSE on error

BOOL CThreadLauncher::Stop (void)
  {
  BOOL bSuccess=TRUE;
  //m_evSleep.SetEvent();
  if (!IsRunning())
  {
    return bSuccess;
  }
  SetSleep();
  #if !defined(WINCE) && !defined(NO_ATL)
  bSuccess&=CancelSynchronousIo(m_thread.native_handle());
  #endif

    boost::posix_time::milliseconds duration(10000);

    {
      boost::unique_lock lock(m_mutexEvent);
      // Wait until thread signals it's done
      while (!(m_nCondEvent & COND_DONE))
      {
        // Waiting for thread to terminate
        if (m_condEvent.timed_wait<boost::posix_time::milliseconds>(lock, duration))
        {
          bSuccess = TRUE;
        }
        else // if timeout
        {
          // Apparently the thread is no longer alive
          // Can't use IsRunning, because mutex is already locked
          //if (IsRunning())
          if (!m_nCondEvent)
            m_thread.detach();

          bSuccess = FALSE;
          break;
        }
      }
    } // unlock mutex

  return bSuccess;
  }




//--------------------------------------------------------------------------
// WaitMs()                waits. returns FALSE if thread shall terminate
// --------
// Input: dwWaitMs = amount of time (ms) to wait
// Return: TRUE if successfully waited
//         FALSE if thread shall terminate

BOOL CThreadLauncher::WaitMs (DWORD dwWaitMs)
  {
  return WaitForCondition(COND_SLEEP, dwWaitMs) != COND_SLEEP;
  }




//--------------------------------------------------------------------------
// SignalDone()            signals that the thread has terminated
// ------------
// Input: -
// Return: 

void CThreadLauncher::SignalDone (void)
  {
  SetDone();
  }




//--------------------------------------------------------------------------
// IsRunning()             returns TRUE if thread is running
// -----------
// Input: -
// Return: 

BOOL CThreadLauncher::IsRunning (void) const
  {
  if (m_thread.get_id() != boost::thread::id())
  {
    boost::lock_guard lock(const_cast<boost::mutex &>(m_mutexEvent));
    return !(m_nCondEvent & COND_DONE);
  }
  return FALSE;
  }



//--------------------------------------------------------------------------
// SetThreadPriority()     changes priority of running thread
// -------------------
// Input: nPriority = new priority
// Return: TRUE on success, FALSE on error

BOOL CThreadLauncher::SetThreadPriority (int nPriority)
  {
#ifdef _WIN32
  ASSERT(m_thread.get_id() != boost::this_thread::get_id()); // only on current running thread please
  if(m_thread.get_id() == boost::thread::id())
    return FALSE;
	boost::thread_attributes attrs;
  return ::SetThreadPriority(m_thread.native_handle(),nPriority);
#else
  return FALSE;
#endif
  }


void CThreadLauncher::SetConditionFlags(ConditionFlags flags)
{
  {
    boost::lock_guard lock(m_mutexEvent);
    m_nCondEvent |= flags;
  }
  m_condEvent.notify_all();
}

void CThreadLauncher::ResetConditionFlags(ConditionFlags flags)
{
  {
    boost::lock_guard lock(m_mutexEvent);
    m_nCondEvent &= ~flags;
  }
}

/** Waits for a condition specified by bit mask in filter

  @return
    Bit-mask of the events that triggered this wait to wake up
*/
ConditionFlags CThreadLauncher::WaitForCondition(ConditionFlags filter, DWORD dwWaitMs)
{
  boost::posix_time::time_duration duration = boost::posix_time::pos_infin;

  if(dwWaitMs >= 0)
    duration = boost::posix_time::milliseconds (dwWaitMs);

  {
    boost::unique_lock<boost::mutex> lock(m_mutexEvent);
    bool bTimeout = false;
    // Make sure last event was part of the filter
    while (!(m_nCondEvent & (filter | COND_DONE)))
    {
      // Cond done will always abort the wait
      if (m_nCondEvent & COND_DONE)
      {
        bTimeout = false;
        break;
      }
      // Waiting for signal (mutex is unlocked during wait, so m_lastEvent can update)
      if (m_condEvent.timed_wait<boost::posix_time::time_duration>(lock, duration))
      {
        // Event triggered normally
        bTimeout = false;
      }
      else // if timeout
      {
        bTimeout = true;
        break;
      }
    }

    ConditionFlags result = (m_nCondEvent & filter);
    if (bTimeout)
      result |= COND_TIMER;

    return result;
  } // unlock mutex
}

boost::condition_variable & CThreadLauncher::GetCondVar()
{
  return m_condEvent;
}

boost::mutex & CThreadLauncher::GetMutex()
{
  return m_mutexEvent;
}


