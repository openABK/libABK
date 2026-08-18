#pragma once

#define USE_PTHREADS
#ifdef USE_PTHREADS
// Custom types
// Used to store a pthread_t
typedef unsigned long THREAD_ID;
#else // Windows handle
typedef DWORD THREAD_ID;
#endif

void Sleep(int milliseconds);
THREAD_ID GetCurrentThreadId();
HANDLE GetCurrentThread();
DWORD SetThreadAffinityMask(HANDLE hThread, DWORD dwThreadAffinityMask);
DWORD SetThreadIdealProcessor(HANDLE hThread, DWORD dwIdealProcessor);
BOOL SetThreadPriority(HANDLE hThread, int nPriority);