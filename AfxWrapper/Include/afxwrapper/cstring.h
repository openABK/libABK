#pragma once

#include <string.h>
#include <cstdarg>
#include <stdio.h>

#include <assert.h>
#include <string>

#if !defined(ASCII_CHAR) && !defined(ACKNOWLEDGE_UNTESTED_CSTRING)
#error "Custom CString implementation is not being tested with wide char support. \
				Please define ACKNOWLEDGE_UNTESTED_CSTRING to acknowledge that you are aware of this and want to proceed. \
				This will most likely not compile and require various fixes. But has been kept for documentation purposes."
#endif

// Empty translation macro
#ifndef _T
#ifdef ASCII_CHAR
#define _T(x) x
#else
#define _T(x) L ## x
#endif
#endif

#define STRING_LEN_MAX 4069
#define STRING_STACK_SIZE 128


#ifdef CP_UTF8
#define UTF8_PAGE E_UTF8_PAGE =
#else
#define UTF8_PAGE
#endif

#ifdef CP_ACP
#define ACP_PAGE E_ACP_PAGE =
#else
#define ACP_PAGE
#endif

enum eCodePages
{
	UTF8_PAGE CP_UTF8,
  ACP_PAGE CP_ACP
};

/** Copies const ascii string to new ascii string*/
class CA2A
{
public:
	CA2A(const char* str)
	{
		Init(str);
	}
	
	CA2A(const char* str, int codepage)
	{
		Init(str);
	}
	
	/** Return pointer to the buffer */
	operator char* ()
	{
		// C++17 is required to obtain non-const pointer from std::string
		return m_Str.data();
	}
	
private:
	void Init(const char* str)
	{
		m_Str = str;
	}
	
	std::string m_Str;
};

class CA2W
{
public:
	CA2W(const char* str)
	{
		Init(str);
	}
	
	CA2W(const char* str, int codepage)
	{
		Init(str);
	}
	
	/** Return pointer to the buffer */
	operator wchar_t* ()
	{
		return m_Buf;
	}
	
private:
	void Init(const char* str)
	{
		mbstowcs(m_Buf, str, STRING_STACK_SIZE);
	}
	
	wchar_t m_Buf[STRING_STACK_SIZE] = {0};
};

class CW2A
{
public:
	CW2A(const wchar_t* str)
	{
		Init(str);
	}
	
	CW2A(const wchar_t* str, int codepage)
	{
		Init(str);
	}
	
	~CW2A()
	{
		if(m_bUseHeap)
			delete[] m_BufHeap;
	}
	
	/** Return pointer to the buffer */
	operator char* ()
	{
		if(m_bUseHeap)
			return m_BufHeap;
		else
			return m_BufStack;
	}
	
private:
	
	void Init(const wchar_t* str)
	{
		m_bUseHeap = false;
		//memset(m_BufStack, 'A', STRING_STACK_SIZE);
		size_t length = wcstombs(NULL, str, STRING_LEN_MAX);
		if(length >= STRING_STACK_SIZE)
		{
			m_bUseHeap = true;
			m_BufHeap = new char[length+1]{0};
			length = wcstombs(m_BufHeap, str, length);
			m_BufStack[length] = 0;
		}
		else /*if (length <= STRING_STACK_SIZE-1) */
		{
			// length is the last accessable character
			// tested with memset
			length = wcstombs(m_BufStack, str, length);
			m_BufStack[length] = 0;
		}

	}

	union
	{
		char m_BufStack[STRING_STACK_SIZE];
		char *m_BufHeap;
	};
	
	// Declares whether m_BufStack or m_Buf is used
	bool m_bUseHeap;
};

class CW2W
{
public:
	CW2W(const wchar_t* str)
	{
		Init(str);
	}
	
	CW2W(const wchar_t* str, int codepage)
	{
		Init(str);
	}
	
	/** Return pointer to the buffer */
	operator wchar_t* ()
	{
		return m_Buf;
	}
	
private:
	void Init(const wchar_t* str)
	{
		size_t length = wcslen(str);
		if (length >= STRING_STACK_SIZE)
		{
			length = STRING_STACK_SIZE;
		}
		
		memcpy(m_Buf, str, length*sizeof(wchar_t));
	}
	
	wchar_t m_Buf[STRING_STACK_SIZE] = {0};
};

#ifdef ASCII_CHAR
typedef CA2A CA2T;
typedef CA2A CT2A;
typedef CA2A CT2T;
typedef CW2A CW2T;
#else
typedef CA2W CA2T;
typedef CW2A CT2A;
typedef CW2W CT2T;
typedef CW2W CW2T;
#endif


inline size_t GetStringLength(const char* str) { return strlen(str); }
//size_t GetStringLengthMax(const char* str);
inline const char* FindStringInString(const char* str1, const char* str2) { return strstr(str1, str2); }

inline size_t GetStringLength(const wchar_t* str) { return wcslen(str); }
inline const wchar_t* FindStringInString(const wchar_t* str1, const wchar_t* str2) { return wcsstr(str1, str2); }

inline int StringCompare(const char* str1, const char* str2) { return strcmp(str1, str2); }
inline int StringCompare(const wchar_t* str1, const wchar_t* str2) { return wcscmp(str1, str2); }

inline int StringCompareNoCase(const char* str1, const char* str2) { return strcasecmp(str1, str2); }
inline int StringCompareNoCase(const wchar_t* str1, const wchar_t* str2) { return wcscasecmp(str1, str2); }


/**Template for String functions
 *	BaseType is the type of characters to use
 *	for example char or wchar_t
 * 
 *	StringTraits is kept for compatibility with
 * 	MFC.
 * 	@todo Use StringTraits
 */
template<typename BaseType, class StringTraits>
class CStringT
{
public:
	CStringT(){
		m_bUseHeap = false;
		m_BufSize = STRING_STACK_SIZE;
		Empty();
	}

  CStringT(const CStringT& other)
  {
    m_bUseHeap = false;
    m_BufSize = STRING_STACK_SIZE;
    SetString(other.GetBuffer());
  }

  CStringT(CStringT&& other)
  {
    m_bUseHeap = other.m_bUseHeap;
    m_BufSize = other.m_BufSize;
    if (m_bUseHeap)
    {
      m_BufPoint = other.m_BufPoint;
      other.m_bUseHeap = false;
      other.m_BufSize = STRING_STACK_SIZE;
      other.m_Buf[0] = 0;
    }
    else
    {
      memcpy(m_Buf, other.m_Buf, m_BufSize);
      m_Buf[m_BufSize - 1] = 0;
    }
  }

	CStringT(const BaseType *str, ssize_t length = -1)
	{
		m_bUseHeap = false;
		m_BufSize = STRING_STACK_SIZE;
		SetString(str, length);
	}

	CStringT(BaseType ch)
	{
		m_bUseHeap = false;
		m_BufSize = STRING_STACK_SIZE;
		BaseType str[2] = {ch, 0};
		SetString(str);
	}

	CStringT &operator=(const BaseType* str)
	{
    if (m_bUseHeap)
      delete[] m_BufPoint;
		m_bUseHeap = false;
		m_BufSize = STRING_STACK_SIZE;
		SetString(str);

		return *this;
	}

	CStringT &operator=(const CStringT &str)
	{
    if (m_bUseHeap)
      delete[] m_BufPoint;
		m_bUseHeap = false;
		m_BufSize = STRING_STACK_SIZE;
		SetString(str);

		return *this;
	}
	
	~CStringT()
	{
		if (m_bUseHeap)
			delete[] m_BufPoint;
	}
	
	/** Return pointer to the buffer */
	operator const BaseType* () const
	{
		return GetBuffer();
	}

	/** Return point to the buffer */
	operator BaseType* ()
	{
		return GetBuffer();
	}
	
	/** Loads string using the id */
	bool LoadString(STRING_ID id)
	{
		void *ptr = StringTraits::GetStringById(id);
		
		if(ptr != nullptr)
		{
			SetString((const BaseType*)ptr);
			return true;
		}
    return false;
	}
	
	/** No conversion needed in case of CStringT<char>*/
	void SetString(const char *str, ssize_t signedLength = -1)
	{
		size_t length = strlen(str) + 1;
		signedLength = signedLength < 0 ? length : signedLength + 1;
		length = length <= signedLength ? length : signedLength;
		if(length > m_BufSize)
			SwitchToHeap(length);
		
		char *buf = GetBuffer();
		memcpy(buf, str, length);
		// Make sure last character is null-terminator
		buf[length-1] = 0;
	}

	void SetString(const wchar_t *str, ssize_t signedLength = -1)
	{
		// Copy & pasted from CW2A
		size_t length = wcstombs(NULL, str, m_BufSize);
		signedLength = signedLength < 0 ? length : signedLength + 1;
		length = length <= signedLength ? length : signedLength;
		size_t newBufSize = length + 1;
		if(newBufSize > m_BufSize)
		{
			SwitchToHeap(newBufSize);
		}
		
		char *buf = GetBuffer();
		length = wcstombs(buf, str, m_BufSize);
		buf[length] = 0;
	}

	void Truncate(size_t length)
	{
		if(length < m_BufSize)
		{
			GetBuffer()[length] = 0;
		}
	}

	BaseType *GetBufferSetLength(size_t length)
	{
		BaseType *buf = GetBuffer();

    // Trailing null-terminator is always appended
    length++;

		if (length > m_BufSize)
		{
			Resize(length);
		}
		else
		{
			Truncate(length);
		}
		return GetBuffer();
	}

	int GetLength() const
	{
		return strlen(GetBuffer());
	}

	void ReleaseBuffer(size_t length = -1)
	{
		if(length == -1)
		{
			length = strlen(GetBuffer());
		}
		Truncate(length);
	}
	
	void AppendFormat(const BaseType* str, ...)
	{
		va_list args;
		va_start(args,str);
		AppendFormatV(str,args);
		va_end(args);
	}
	
	void AppendFormatV(const BaseType* str, va_list args)
	{
		// TODO: Test
		// Aquire last possible position
		char *buf = GetBuffer();
		size_t length = strnlen(buf, m_BufSize);

		// Remaining amount of characters (possible characters in buf - actual characters in buf)
		size_t addLen = m_BufSize - 1 - length;

		va_list argsCopy;
		va_copy(argsCopy, args);

		// Calculate new possible size and offset, to append
		int lastChar = vsnprintf(buf+length, addLen + 1, str, args);
		if (lastChar > (int)addLen)
		{
			// If the string was truncated, we need to resize, make sure to have enough space for null-terminator
			Resize(length+lastChar+1);
			buf = GetBuffer();
			// Ensure that the possible characters in buffer are the same
      // as the characters by the format function
      assert((m_BufSize - 1 - length) == lastChar);
      // vsnprintf accepts the number of bytes with the null-terminator, therefore we need to add 1 to the lastChar
			lastChar = vsnprintf(buf+length, lastChar + 1, str, argsCopy);
		}
    assert(lastChar >= 0);

		va_end(argsCopy);

		// Make sure last character is null-terminator (this should be the case already)
		// buf[length+lastChar] = 0;
	}
	
	void AppendChar(const BaseType ch)
	{
		// Characters without null-terminator
		const size_t length = strnlen(GetBuffer(), m_BufSize);
		// New amount of characters
		const size_t newLen = length + 1;
		// Actual new buffer size
		const size_t newBufSize = newLen + 1;
		
		if(newBufSize > m_BufSize)
			Resize(newBufSize);

		char *buf = GetBuffer();
		buf[length] = ch;
		buf[length+1]  = 0;
	}
	
	void Append(const BaseType* str)
	{
		Append(str, strlen(str));
	}

	void Append(const BaseType* str, size_t lengthToAppend)
	{
		char *buf = GetBuffer();
		size_t length = strnlen(buf, m_BufSize);

		// Characters (not bytes) free in buffer:
		size_t addLen = m_BufSize - 1 - length;

		if (lengthToAppend > addLen)
		{
			// If the string was truncated, we need to resize, make sure to have enough space for null-terminator
			Resize(length+lengthToAppend+1);
			buf = GetBuffer();
			// And append again
			addLen = m_BufSize - 1 - length;
		}
    strncat(buf, str, addLen);
	}

	// CStringT operator+(const BaseType* str) const
	// {
	// 	CStringT newStr = *this;
	// 	newStr.Append(str);
	// 	return newStr;
	// }

	CStringT &operator+=(const BaseType* str)
	{
		Append(str);
		return *this;
	}
	
	/** Emtpies string */
	void Empty()
	{
		GetBuffer()[0] = 0;
	}
	
	void Format(const BaseType* str, ...)
	{
		// Clear first, then simply call AppendFormatV
		Empty();
		
		va_list args;
		va_start(args,str);
		AppendFormatV(str,args);
		va_end(args);
	}

	void FormatV(const BaseType* str, va_list args)
	{
		// Clear first, then simply call AppendFormatV
		Empty();
		AppendFormatV(str,args);
	}

	bool IsEmpty() const
	{
		if (strnlen(GetBuffer(), m_BufSize))
			return false;
		
		return true;
	}

	int Compare(const BaseType* str) const
	{
		return StringCompare(GetBuffer(), str);
	}

	int CompareNoCase(const BaseType* str) const
	{
		return StringCompareNoCase(GetBuffer(), str);
	}

	inline const char *GetBuffer(int nMax=-1) const
	{
		const char *buffer = &m_Buf[0];
		if (m_bUseHeap)
		{
			buffer = m_BufPoint;
		}
		return buffer;
	}

	inline char *GetBuffer(int nMax=-1)
	{
		// TODO: Resize if nMax > m_BufSize
		char *buffer = &m_Buf[0];
		if (m_bUseHeap)
		{
			buffer = m_BufPoint;
		}
		return buffer;
	}

	int Replace(const BaseType *strOld, const BaseType *strNew)
	{
		// Create a new string, where occurrences of strOld are replaced by strNew
		std::basic_string<BaseType> newStr;
		const BaseType *buf = GetBuffer();
		while(*buf)
		{
			const BaseType *found = FindStringInString(buf, strOld);
			if(found)
			{
				// Append everything before the found string
				newStr.append(buf, found - buf);
				// Append the new string
				newStr.append(strNew);
				// Skip the old string
				buf = found + strlen(strOld);
			}
			else
			{
				// Append the rest of the string
				newStr.append(buf);
				break;
			}
		}
		SetString(newStr.c_str());
		return 0;
	}

	int Replace(BaseType chOld, BaseType chNew)
	{
		char *buf = GetBuffer();
		for (size_t i = 0; i < m_BufSize; i++)
		{
			if (buf[i] == chOld)
			{
				buf[i] = chNew;
			}
		}
		return 0;
	}

	CStringT Left(int length) const
	{
		CStringT str;
		str.SetString(GetBuffer(), length);
		return str;
	}

	CStringT Mid(int pos, int length = -1) const
	{
		CStringT str;
		str.SetString(GetBuffer()+pos, length);
		return str;
	}

	CStringT Right(int length) const
	{
		CStringT str;
		str.SetString(GetBuffer()+GetLength()-length, length);
		return str;
	}

	int ReverseFind(BaseType ch) const
	{
		STUBBED();
		const char *buf = GetBuffer();
		const char *found = strrchr(buf, ch);
		if(found)
		{
			return found - buf;
		}
		return -1;
	}

	// Trim
	CStringT& TrimLeft(const BaseType *charsToRemove)
	{
		// Not actually stubbed, but assumed buggy, because untested
		TRACE("CStringT::TrimLeft is untested\n");
		const char *buf = GetBuffer();
		size_t index = 0;
		// no matching characters (end of trim)
		BOOL bNoMatch = FALSE;
		for (; buf[index] != '\0' && !bNoMatch; index++)
		{
			bNoMatch = TRUE;
			for (size_t i = 0; charsToRemove[i] != '\0'; i++)
			{
				if (buf[index] == charsToRemove[i])
				{
					bNoMatch = FALSE;
					break;
				}
			}
		}

		if (index > 1)
		{
			SetString(&buf[index]);
		}
		return *this;
	}

	CStringT& TrimLeft()
	{
		TrimLeft(" \t\n\r");
		return *this;
	}

	CStringT& TrimRight(const BaseType *charsToRemove)
	{
		// Not actually stubbed, but assumed buggy, because untested
		TRACE("CStringT::TrimRight is untested\n");
		char *buf = GetBuffer();
		size_t index = strlen(buf);
		BOOL bNoMatch = FALSE;
		for (; index != 0 && !bNoMatch; index--)
		{
			bNoMatch = TRUE;
			for (size_t i = 0; charsToRemove[i] != '\0'; i++)
			{
				if (buf[index] == charsToRemove[i])
				{
					bNoMatch = FALSE;
					break;
				}
			}
		}

		buf[index + 1] = '\0';
		return *this;
	}

	CStringT &TrimRight()
	{
		TrimRight(" \t\n\r");
		return *this;
	}

	CStringT& Trim()
	{
		TrimLeft();
		TrimRight();
		return *this;
	}

	int Find(BaseType charToFind, int pos = 0) const
	{
		const char *buf = GetBuffer();
		size_t length = GetLength();
		if (pos < 0 || pos >= (int)length)
			return -1;

		for (int i = pos; i < (int)length; i++)
		{
			if (buf[i] == charToFind)
				return i;
		}

		return -1;
	}

	int Find(const BaseType *strToFind, int pos = 0) const
	{
		STUBBED();
		return -1;
	}

	int FindOneOf(const BaseType *strCharSet, int pos = 0) const
	{
		STUBBED();
		return -1;
	}

	int Remove(BaseType charToRemove)
	{
		STUBBED();
		return 0;
	}

	BaseType GetAt(int index) const
	{
		return GetBuffer()[index];
	}

	const BaseType *GetString() const
	{
		return GetBuffer();
	}

	void SetAt(int index, BaseType ch)
	{
		GetBuffer()[index] = ch;
	}

	void Insert(int index, BaseType ch)
	{
		char *buf = GetBuffer();
		size_t length = strlen(buf);

		// Check if index is within bounds
		if (index < 0 || index > length)
		{
			// Invalid index, do nothing
			return;
		}

		CStringT str;
		str.SetString(buf, index);
		str.AppendChar(ch);
		str.Append(buf + index);

		SetString(str);
	}

private:
	// Call only when not enough
	inline void Resize(size_t newSize) {
		char *oldBuf = GetBuffer();
		size_t oldSize = m_BufSize;
		// If this already was on heap, we need to clear up afterwards
		bool needDelete = m_bUseHeap;

		ASSERT(oldSize < newSize);
		ASSERT(newSize > STRING_STACK_SIZE);

		char *pNewBuf = new char[newSize]{0};
		m_BufSize = newSize;
		m_bUseHeap = true;

		memcpy(pNewBuf, oldBuf, oldSize);
		// Can only be assigned after memcpy, as m_BufPoint is in a union with m_Buf
		m_BufPoint = pNewBuf;

		if(needDelete)
		{
			delete[] oldBuf;
		}
	}
	
	inline void SwitchToHeap(size_t newSize) {
		// We're just resizing, delete old memory first
		if(m_bUseHeap)
			delete[] m_BufPoint;
		
		m_BufPoint = new char[newSize]{0};
		m_bUseHeap = true;
		m_BufSize = newSize;
	}
	
	union {
		// TODO: This is ugly, polish it later
		char m_Buf[STRING_STACK_SIZE] = {0};
		char *m_BufPoint;
	};
	
	// Amount of accessable bytes (m_BufSize-1 = lastIndex)
	size_t m_BufSize;
	
	bool m_bUseHeap;
};

// Operator for concatenation
template<typename BaseType, typename Trait>
CStringT<BaseType, Trait> operator+(const CStringT<BaseType, Trait> &str1, const CStringT<BaseType, Trait> &str2)
{
	CStringT<BaseType, Trait> str;
	str.SetString(str1);
	str.Append(str2);
	return str;
}

template<typename BaseType, typename Trait>
CStringT<BaseType, Trait> operator+(const BaseType *str1, const CStringT<BaseType, Trait> &str2)
{
	CStringT<BaseType, Trait> str;
	str.SetString(str1);
	str.Append(str2);
	return str;
}

template<typename BaseType, typename Trait>
CStringT<BaseType, Trait> operator+(const CStringT<BaseType, Trait> &str1, const BaseType *str2)
{
  CStringT<BaseType, Trait> str;
  str.SetString(str1);
  str.Append(str2);
  return str;
}

template<typename BaseType, typename Trait>
bool operator==(const CStringT<BaseType, Trait> &str1, const CStringT<BaseType, Trait> &str2)
{
	return str1.Compare(str2) == 0;
}

template<typename BaseType, typename Trait>
bool operator<(const CStringT<BaseType, Trait> &str1, const CStringT<BaseType, Trait> &str2)
{
  return str1.Compare(str2) < 0;
}

template<typename BaseType, typename Trait>
bool operator>(const CStringT<BaseType, Trait> &str1, const CStringT<BaseType, Trait> &str2)
{
  return str1.Compare(str2) > 0;
}

template<typename BaseType, typename Trait>
bool operator!=(const CStringT<BaseType, Trait> &str1, const CStringT<BaseType, Trait> &str2)
{
  return str1.Compare(str2) != 0;
}

template<typename BaseType, typename Trait>
bool operator<=(const CStringT<BaseType, Trait> &str1, const CStringT<BaseType, Trait> &str2)
{
  return str1.Compare(str2) <= 0;
}

template<typename BaseType, typename Trait>
bool operator>=(const CStringT<BaseType, Trait> &str1, const CStringT<BaseType, Trait> &str2)
{
  return str1.Compare(str2) >= 0;
}


/** Template to describe the traits for a certain base type */
template<typename BaseType = char>
class CStringTrait
{
	friend class CStringT<BaseType, CStringTrait>;
	/** Search resource file for a String of a certain ID */
	static void * GetStringById(STRING_ID id)
	{
		// On Linux, we made the string IDs simply consist of the pointer to the string.
    // Thus we can just return the id as pointer.
		return (void*)id;
	}
};

typedef CStringTrait<char> CStringTraitA;
typedef CStringTrait<wchar_t> CStringTraitW;

#ifndef UNICODE
typedef CStringT<char, CStringTraitA> CString;
#else
typedef CStringT<wchar_t, CStringTraitW> CString;
#endif
