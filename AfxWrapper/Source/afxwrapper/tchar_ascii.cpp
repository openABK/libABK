#include "afxwrapper/tchar.h"
#include <string.h>

#include <boost/regex.hpp>
#include <stdarg.h>

size_t _tcslen(const TCHAR *str)
{
    return strlen(str);
}

int _tcscmp(const TCHAR *left, const TCHAR *right)
{
    return strcmp(left, right);
}

int _tcsicmp(const TCHAR *left, const TCHAR *right)
{
    return strcasecmp(left, right);
}

int _tcsncmp(const TCHAR *str1, const TCHAR *str2, size_t max)
{
    return strncmp(str1, str2, max);
}

int _tcsnicmp(const TCHAR *str1, const TCHAR *str2, size_t max)
{
    return strncasecmp(str1, str2, max);
}

const TCHAR *_tcsstr(const TCHAR *str, const TCHAR *strToFind)
{
    return strstr(str, strToFind);
}

TCHAR *_tcscpy(TCHAR *dest, const TCHAR *src)
{
    return strcpy(dest, src);
}

size_t _tcscpy_s(TCHAR *pszDest, size_t szBytes, const TCHAR *pszSource)
{
    return boost::BOOST_REGEX_DETAIL_NS::strcpy_s(pszDest, szBytes, pszSource);
}

TCHAR *_tcsncpy(TCHAR *dest, const TCHAR *src, size_t max)
{
    return strncpy(dest, src, max);
}

TCHAR *_tcsnccpy(TCHAR *dest, const TCHAR *src, size_t max)
{
    // I don't know why this function protype exists, but it appears
    // to behave the same as _tcsncpy
    return _tcsncpy(dest, src, max);
}

size_t _tcsncpy_s(TCHAR *dest, size_t dest_count, const TCHAR *src, size_t src_count)
{
    return strncpy(dest, src, dest_count) ? 0 : -1;
}

TCHAR *_tcscat(TCHAR *dest, const TCHAR *src)
{
    return strcat(dest, src);
}

const TCHAR *_tcschr(const TCHAR *str, int charToFind)
{
    return strchr(str, charToFind);
}

TCHAR *_tcschr(TCHAR *str, int charToFind)
{
    return strchr(str, charToFind);
}

const TCHAR *_tcsrchr(const TCHAR *str, int charToFind)
{
    return strrchr(str, charToFind);
}

TCHAR *_tcsrchr(TCHAR *str, int charToFind)
{
    return strrchr(str, charToFind);
}

int _stscanf(const TCHAR *source, const TCHAR *format, ...)
{
    va_list args;
    va_start(args, format);

    int ret = vsscanf(source, format, args);

    va_end(args);
    return ret;
}

int _stprintf_s(TCHAR *dest, size_t dest_count, const TCHAR *format, ...)
{
    va_list args;
    va_start(args, format);

    int ret = vsnprintf(dest, dest_count, format, args);

    va_end(args);
    return ret;
}

FILE *_tfopen(const TCHAR *pszFilename, const TCHAR *pszModes)
{
    return fopen(pszFilename, pszModes);
}
