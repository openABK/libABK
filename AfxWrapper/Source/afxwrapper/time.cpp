#include "afxwrapper.h"

#include <boost/date_time/local_time_adjustor.hpp>
#include <boost/date_time/c_local_time_adjustor.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

// Convert SYSTEMTIME to tm struct
static BOOL SystemTimeToTm(const SYSTEMTIME *pSystemTime, tm *pt_tm)
{
  pt_tm->tm_year = pSystemTime->wYear - 1900;
  pt_tm->tm_mon = pSystemTime->wMonth - 1;
  pt_tm->tm_wday = pSystemTime->wDayOfWeek;
  pt_tm->tm_mday = pSystemTime->wDay;
  pt_tm->tm_hour = pSystemTime->wHour;
  pt_tm->tm_min = pSystemTime->wMinute;
  pt_tm->tm_sec = pSystemTime->wSecond;
  pt_tm->tm_isdst = -1;

  return TRUE;
}

// Convert tm struct to SYSTEMTIME
static BOOL TmToSystemTime(const tm *pt_tm, SYSTEMTIME *pSystemTime)
{
  pSystemTime->wYear = pt_tm->tm_year + 1900;
  pSystemTime->wMonth = pt_tm->tm_mon + 1;
  pSystemTime->wDayOfWeek = pt_tm->tm_wday;
  pSystemTime->wDay = pt_tm->tm_mday;
  pSystemTime->wHour = pt_tm->tm_hour;
  pSystemTime->wMinute = pt_tm->tm_min;
  pSystemTime->wSecond = pt_tm->tm_sec;
  pSystemTime->wMilliseconds = 0;

  return TRUE;
}

BOOL FileTimeToSystemTime(const FILETIME *pFileTime, SYSTEMTIME *pSystemTime)
{
  // Convert to double and use VariantTimeToSystemTime
  double dbTime = (((int64_t)pFileTime->dwLowDateTime + (((int64_t)pFileTime->dwHighDateTime << 32)) - 116444736000000000) / 864000000000.0);
  return VariantTimeToSystemTime(dbTime, pSystemTime);
}

BOOL SystemTimeToVariantTime(const SYSTEMTIME *pSystemTime, double *pVarTime)
{
  // Convert SYSTEMTIME to tm struct
  tm pt_tm;
  SystemTimeToTm(pSystemTime, &pt_tm);

  try
  {
    // Convert to boost ptime
    boost::posix_time::ptime pt = boost::posix_time::ptime_from_tm(pt_tm);

    // Convert to FILETIME
    boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));
    auto diff = (pt - epoch).total_milliseconds();
    boost::posix_time::time_duration td = boost::posix_time::milliseconds(diff);
    // Add milliseconds to time duration
    td += boost::posix_time::milliseconds(pSystemTime->wMilliseconds);

    // Convert to double (86400 seconds in a day)
    *pVarTime = td.total_milliseconds() / 86400000.0;
  }
  catch (...)
  {
    return FALSE;
  }
  return TRUE;
}

BOOL VariantTimeToSystemTime(double dbTime, SYSTEMTIME *pSystemTime)
{
  // Convert double to boost ptime
  boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));
  boost::posix_time::time_duration td = boost::posix_time::milliseconds((long) (dbTime * 86400000.0));
  boost::posix_time::ptime pt = epoch + td;

  // Convert to tm struct
  tm pt_tm = boost::posix_time::to_tm(pt);

  // Convert to SYSTEMTIME
  TmToSystemTime(&pt_tm, pSystemTime);
  pSystemTime->wMilliseconds = pt.time_of_day().fractional_seconds() / 1000;

  return TRUE;
}

ULONGLONG GetTickCount64()
{
#ifndef LINUX
  // Use Boost time function to get the current uptime in milliseconds
  boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
  boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));
  auto diff = (now - epoch).total_milliseconds();
#else
  // Use clock_gettime to get the current uptime in milliseconds
  struct timespec ts;
  clock_gettime(CLOCK_BOOTTIME, &ts);
  ULONGLONG diff = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
  return (ULONGLONG)diff;
}

DWORD GetTickCount()
{
  // Use GetTickCount64
  ULONGLONG diff = GetTickCount64();

  return (DWORD)diff;
}