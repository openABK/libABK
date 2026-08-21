#pragma once

#include <stdint.h>

#include "afxwrapper/tchar.h"

typedef const TCHAR *LPCTSTR;
typedef TCHAR *LPTSTR;

#define _T(x) x
#define _countof(x) (sizeof(x)/sizeof(x[0]))

typedef int BOOL;
typedef long long LONGLONG;
typedef unsigned long long ULONGLONG;
typedef unsigned int UINT;

typedef int16_t SHORT;
typedef uint16_t USHORT;
typedef uint16_t WORD;

typedef uint32_t DWORD;
typedef uint8_t BYTE;

typedef DWORD *DWORD_PTR;

typedef void *LPVOID;
typedef void *HANDLE;

typedef int HRESULT;

static const int TRUE = 1;
static const int FALSE = 0;

typedef const char *STRING_ID;

#define S_OK TRUE

#define INFINITE -1


#define WINAPI

#define MAX_PATH 4096

#include "afxwrapper/debug.h"
#include "afxwrapper/networking.h"

#include "afxwrapper/variant_types.h"
#include "afxwrapper/bstr.h"
#include "afxwrapper/cstring.h"
#include "afxwrapper/variant.h"
#include "afxwrapper/time.h"
#include "afxwrapper/math.h"
#include "afxwrapper/thread.h"

#include "ThreadLauncher.h"

static void ZeroMemory(void *dest, size_t count)
{
  memset(dest, '\0', count);
}
