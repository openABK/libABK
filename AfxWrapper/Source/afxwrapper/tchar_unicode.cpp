#include "afxwrapper/platform_config.h"
#include "afxwrapper/tchar.h"
#include <string.h>

#include <wchar.h>
// For thread-safe wcstombs function
#include <stdlib.h>

#include <vector>

size_t _tcslen(const TCHAR *str)
{
    return wcslen(str);
}

int _tcscmp(const TCHAR *left, const TCHAR *right)
{
    return wcscmp(left, right);
}

int _tcsicmp(const TCHAR *left, const TCHAR *right)
{
    return wcscasecmp(left, right);
}

int _tcsncmp(const TCHAR *str1, const TCHAR *str2, size_t max)
{
    return wcsncmp(str1, str2, max);
}

int _tcsnicmp(const TCHAR *str1, const TCHAR *str2, size_t max)
{
    return wcsncasecmp(str1, str2, max);
}

const TCHAR *_tcsstr(const TCHAR *str, const TCHAR *strToFind)
{
    return wcsstr(str, strToFind);
}

TCHAR *_tcscpy(TCHAR *dest, const TCHAR *src)
{
    return wcscpy(dest, src);
}

size_t _tcscpy_s(TCHAR *pszDest, size_t szBytes, const TCHAR *pszSource)
{
    // TODO: Does this really do the same?
#ifdef NO_WINDOWS
    return wcsncpy(pszDest, pszSource, szBytes) ? 0 : -1;
#else
    return wcscpy_s(pszDest, szBytes, pszSource);
#endif
}

TCHAR *_tcsncpy(TCHAR *dest, const TCHAR *src, size_t max)
{
    return wcsncpy(dest, src, max);
}

TCHAR *_tcsnccpy(TCHAR *dest, const TCHAR *src, size_t max)
{
    // I don't know why this function protype exists, but it appears
    // to behave the same as _tcsncpy
    return wcsncpy(dest, src, max);
}

size_t _tcsncpy_s(TCHAR *dest, size_t dest_count, const TCHAR *src, size_t src_count)
{
    // WARNING: Source does not get checked! Permits OOB reads. Highly unlikely segfaults.
#ifdef NO_WINDOWS
    return _tcscpy_s(dest, dest_count, src);
#else
    return wcscpy_s(dest, dest_count, src, src_count);
#endif
}

TCHAR *_tcscat(TCHAR *dest, const TCHAR *src)
{
    return wcscat(dest, src);
}

const TCHAR *_tcschr(const TCHAR *str, int charToFind)
{
    return wcschr(str, charToFind);
}

TCHAR *_tcschr(TCHAR *str, int charToFind)
{
    return wcschr(str, charToFind);
}

const TCHAR *_tcsrchr(const TCHAR *str, int charToFind)
{
    return wcsrchr(str, charToFind);
}

TCHAR *_tcsrchr(TCHAR *str, int charToFind)
{
    return wcsrchr(str, charToFind);
}

int _stscanf(const TCHAR *source, const TCHAR *format, ...)
{
    va_list args;
    va_start(args, format);

    int ret = vswscanf(source, format, args);

    va_end(args);
    return ret;
}

int _stprintf_s(TCHAR *dest, size_t dest_count, const TCHAR *format, ...)
{
    va_list args;
    va_start(args, format);

    int ret = vswprintf(dest, dest_count, format, args);

    va_end(args);
    return ret;
}

FILE *_tfopen(const TCHAR *pszFilename, const TCHAR *pszModes)
{
#ifdef NO_WINDOWS
    // Consider using CW2A for this, to improve code reuse
    size_t szConverted = wcstombs(NULL, pszFilename, 0);
    if (szConverted < 0)
    {
        return NULL;
    }
    szConverted++; // Add one for null terminator, to be sure
    std::vector<char> pszFilenameA(szConverted);

    wcstombs(pszFilenameA.data(), pszFilename, szConverted);

    size_t szConvertedModes = wcstombs(NULL, pszModes, 0);
    if (szConvertedModes < 0)
    {
        return NULL;
    }
    szConvertedModes++; // Add one for null terminator, to be sure
    std::vector<char> pszModesA(szConvertedModes);

    wcstombs(pszModesA.data(), pszModes, szConvertedModes);

    return fopen(pszFilenameA.data(), pszModesA.data());
#else
    return _wfopen(pszFilename, pszModes);
#endif
}