
// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently

#ifndef _STDAFX_H
#define _STDAFX_H

#ifndef _SECURE_ATL
#define _SECURE_ATL 1
#endif

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN            // Exclude rarely-used stuff from Windows headers
#endif

//#include "targetver.h"

#ifndef NO_ATL
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // some CString constructors will be explicit

// turns off MFC's hiding of some common and often safely ignored warning messages
#define _AFX_ALL_WARNINGS

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions


#ifndef _AFX_NO_OLE_SUPPORT
#include <afxdtctl.h>           // MFC support for Internet Explorer 4 Common Controls
#endif

#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>             // MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT


#include <afxsock.h>            // MFC socket extensions

#include <MyLock.h>
#endif
// Start including Windows header here

#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif

#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <fstream>

#include <sstream>


#if !defined(NO_WINDOWS) && !defined(__WINDOWS__)
// This combination is used for detecting Winelib builds
#define WINELIB
#endif

// Boost should not include Windows headers on Linux
// Make sure to unset all Windows defines before including Boost
#ifdef WINELIB
#undef WIN32
#undef _WIN32
#undef __WIN32__
#undef BOOST_USE_WINDOWS_H
#endif

#include <boost/asio.hpp>
#include <boost/atomic.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/smart_ptr/make_unique.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/unordered_map.hpp>
#include <boost/thread.hpp>

#ifdef WINELIB
#define WIN32
#define _WIN32
#define __WIN32__
#define BOOST_USE_WINDOWS_H
#endif


#ifdef NO_ATL
// Windows.h is included implicitly by boost libraries
#include "afxwrapper.h"
#endif
//
//
//
//// stdafx.h : include file for standard system include files,
//// or project specific include files that are used frequently, but
//// are changed infrequently
////
//
//#pragma once
//
//#include "targetver.h"
//
//#include <stdio.h>
//#include <tchar.h>
//
//
//#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // some CString constructors will be explicit
//
//#include <atlbase.h>
//#include <atlstr.h>
////#include <atlsync.h>
////#include <afxmt.h>
//
//// TODO: reference additional headers your program requires here


#endif