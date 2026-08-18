#pragma once
#ifndef NO_WINDOWS
#include <oleauto.h>
#else
// If TCHAR is char so is OLECHAR
#ifdef ASCII_CHAR
#define OLE2ANSI
typedef char OLECHAR;
#else
typedef wchar_t OLECHAR;
#endif

typedef TCHAR *bstr_t;
typedef TCHAR *BSTR;
extern "C"
{
	BSTR SysAllocStringLen(const OLECHAR *str, unsigned int len);
	BSTR SysAllocString(const OLECHAR *str);
	void SysFreeString(BSTR str);
	BSTR SysAllocStringByteLen(const char *str, unsigned int len);
	size_t SysStringLen(BSTR bstr);
	unsigned int SysStringByteLen(BSTR str);
}
#endif