#pragma once

#ifdef NO_WINDOWS
// minwin
struct FILETIME
{
  DWORD dwLowDateTime;
  DWORD dwHighDateTime;
};

struct SYSTEMTIME
{
  WORD wYear;
  WORD wMonth;
  WORD wDay;

  WORD wHour;
  WORD wMinute;
  WORD wSecond;
  WORD wMilliseconds;

  WORD wDayOfWeek;
};

// Defined in Linux.cpp
BOOL FileTimeToSystemTime(const FILETIME *pFileTime, SYSTEMTIME *pSystemTime);

BOOL LocalFileTimeToFileTime(const FILETIME *pLocalFileTime, FILETIME *pFileTime);

BOOL SystemTimeToVariantTime(const SYSTEMTIME *pSystemTime, double *pVarTime);

BOOL SystemTimeToFileTime(const SYSTEMTIME *pSystemTime, FILETIME *pFileTime);

BOOL VariantTimeToSystemTime(double dbTime, SYSTEMTIME *pSystemTime);

BOOL SystemTimeToTzSpecificLocalTime(const void *pTimeZoneInformation, const SYSTEMTIME *pUniversalTime, SYSTEMTIME *pLocalTime);
BOOL TzSpecificLocalTimeToSystemTime(const void *pTimeZoneInformation, const SYSTEMTIME *pLocalTime, SYSTEMTIME *pUniversalTime);

BOOL SetFileTime(HANDLE hHandle, const FILETIME *pCreateTime, const FILETIME *pAccessTime, const FILETIME *pModifyTime);

BOOL GetSystemTime(SYSTEMTIME *pSystemTime);
BOOL GetSystemTimes(FILETIME *pIdleTime, FILETIME *pKernelTime, FILETIME *pUserTime);

BOOL SetSystemTime(const SYSTEMTIME *pSystemTime);

void GetLocalTime(SYSTEMTIME *pTime);

enum eLocales
{
  LOCALE_USER_DEFAULT,
  LOCALE_SYSTEM_DEFAULT,
};

int GetTimeFormat(eLocales Locale, DWORD dwFlags, const SYSTEMTIME *lpTime, LPCTSTR lpFormat, LPTSTR lpTimeStr, int cchTime);

ULONGLONG GetTickCount64();
DWORD GetTickCount();
#endif

#ifdef NO_ATL
class CTime
{
public:
  CTime()
  {
    STUBBED();
  }

  CTime(const FILETIME &ft)
  {
    STUBBED();
  }
};

struct VARIANT;

class COleDateTime
{
private:
  SYSTEMTIME m_st;

public:
  COleDateTime(SYSTEMTIME &st)
  {
    STUBBED();
    m_st = st;
  }

  COleDateTime(const VARIANT &var)
  {
    STUBBED();
  }

  CString Format(LPCTSTR pszFormat = NULL)
  {
    STUBBED();
    return CString();
  }
};
#endif