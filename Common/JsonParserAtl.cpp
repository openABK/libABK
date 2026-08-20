// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------------------------
//  _____  __   __  _____  _    _        ____              
// |  ___||  \ /  ||  _  \| |  | |      / ___|  _   _  ___ 
// |  __| |   V   ||  -  /| |  | |  __  \___ \ | | | |/ __|
// | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
// |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
//                                              |___/      
// Filename:    JsonParserAtl.cpp
// Created:     2012-07-19 (17:59)
// Author:      D. Burger
// Description: JSON parser extended to ATL string and OLE variant capabilities
//------------------------------------------------------------------------------------------------

#include "stdafx.h"
#include "JsonParserAtl.h"
#include <assert.h>
//#include <afxdisp.h>
#ifdef WIN32
#include <comutil.h>
#endif

//--------------------------------------------------------------------------
// CJsonParserAtl()        Constructor of CJsonParserAtl
// ----------------
// Input: pStartOfExpression = 
// Return: 

CJsonParserAtl::CJsonParserAtl (const char *pStartOfExpression)
  : CJsonParser(pStartOfExpression)
  {
  }



//--------------------------------------------------------------------------
// ~CJsonParserAtl()       Destructor of CJsonParserAtl
// -----------------
// Input: -
// Return: 

CJsonParserAtl::~CJsonParserAtl (void)
  {
  }



//--------------------------------------------------------------------------
// ExtractValue()          extracts value from object
// --------------
// Input: pszName = name to test for
//        pGet = pointer to receive the value
//               for time, the local time is returned.
//               for the variant type, the content of the variant shall not
//               contain deep data since it will not be cleared when the
//               decoded value gets assigned to it.
// Return: true if value was extracted, false if name/type was not matching

bool CJsonParserAtl::ExtractValueAtl (const char *pszName, CString &rGet) const
  {
  if((m_pNameStart)&&(CompareUnescapedString(pszName,m_pNameStart,m_pNameEnd)))
    return ExtractValueAtl(rGet);
  return false;
  }

bool CJsonParserAtl::ExtractValueAtl (const char *pszName, _variant_t &rGet) const
  {
  if((m_pNameStart)&&(CompareUnescapedString(pszName,m_pNameStart,m_pNameEnd)))
    return ExtractValueAtl(rGet);
  return false;
  }


//--------------------------------------------------------------------------
// ExtractValue()          gets value from array
// --------------
// Input: pGet = pointer to get the data
//               for the variant type, the content of the variant shall not
//               contain deep data since it will not be cleared when the
//               decoded value gets assigned to it.
// Return: true on success, false on error

bool CJsonParserAtl::ExtractValueAtl (CString &rGet) const
  {
  if(m_nType!=CJsonParser::TYPE_STRING)
    return false;
  assert(m_pValueStart);
  if(!m_pValueStart)
    return false;
  return UnescapeString(m_pValueStart,m_pValueEnd,&rGet);
  }

bool CJsonParserAtl::ExtractValueAtl (_variant_t &rGet) const
  {
  assert(m_pValueStart);
  if(!m_pValueStart)
    return false;
  rGet.Clear();
  const char *pSrc;
  if(m_nType==CJsonParser::TYPE_NUMERIC) // numeric can either be int, double or bool
    {
    if(!strncmp("true",m_pValueStart,m_pValueEnd-m_pValueStart))
      {
      rGet.vt=VT_BOOL;
      rGet.boolVal=1;
      return true;
      }
    if(!strncmp("false",m_pValueStart,m_pValueEnd-m_pValueStart))
      {
      rGet.vt=VT_BOOL;
      rGet.boolVal=0;
      return true;
      }
    if(!strncmp("null",m_pValueStart,m_pValueEnd-m_pValueStart))
      {
      rGet.vt=VT_EMPTY;
      rGet.boolVal=0;
      return true;
      }
    for(pSrc=m_pValueStart;pSrc<m_pValueEnd;pSrc++)
      {
      char c=*pSrc;
      if((c=='.')||(c=='e')||(c=='E')||(c==',')) // these characters identify a floating point
        {
        double dValue;
        if(!CJsonParser::ExtractValue(&dValue))
          return false;
        rGet=dValue;
        return true;
        }
      }
    int nValue;
    if(!CJsonParser::ExtractValue(&nValue))
      return false;
    rGet.vt=VT_I4;
    rGet.lVal=nValue;
    return true;
    }
  else if(m_nType==CJsonParser::TYPE_STRING) // string can either be data or generic string
    {
    time_t tmValue;
    if(ScanDate(m_pValueStart,m_pValueEnd-m_pValueStart,&tmValue))
      { // see also http://www.codeproject.com/Messages/198678/Re-Converting-to-VARIANT-DATE-type-from-time_t.aspx
      FILETIME timeFile;
      LONGLONG ll;
      ll=Int32x32To64(tmValue,10000000)+116444736000000000;
      timeFile.dwLowDateTime = (DWORD)ll;
      timeFile.dwHighDateTime = ll >> 32;
      SYSTEMTIME timeSystem;
      FileTimeToSystemTime(&timeFile,&timeSystem);
      double dDate;
      SystemTimeToVariantTime(&timeSystem,&dDate);
      rGet=dDate; // results in a double ..
      rGet.vt=VT_DATE; // .. so cast it to date
      return true;
      }
    // decode string
    CString strGet;
    if(!ExtractValueAtl(strGet))
      return false;
    rGet=strGet;
    #ifndef OLE2ANSI
    if(_tcscmp(CW2T(rGet.bstrVal),(LPCTSTR)strGet))
    #else
    if(_tcscmp(CA2T(rGet.bstrVal),(LPCTSTR)strGet))
    #endif
      {
      ASSERT(FALSE);
      }

    return true;
    }
  return false;
  }




//--------------------------------------------------------------------------
// GetName()               returns name of the object/array/value
// ---------
// Input: strGet = 
// Return: 

bool CJsonParserAtl::GetName (CString &strGet) const
  {
  return UnescapeString(m_pNameStart,m_pNameEnd,&strGet);  
  }




//--------------------------------------------------------------------------
// GetValueString()        returns the string form of the value
// ----------------
// Input: strGet = 
// Return: 

bool CJsonParserAtl::GetValueString (CString &strGet) const
  {
  return UnescapeString(m_pValueStart,m_pValueEnd,&strGet);  
  }




//--------------------------------------------------------------------------
// UnescapeString()        unescapes a string
// ----------------
// Input: pStart = 
//        pEnd = 
//        pGet = 
// Return: true on success
//         false on sequence error, *pGet contains the string up to the detected error

/*static*/ bool CJsonParserAtl::UnescapeString (const char *pStart, const char *pEnd, CString *pGet)
  {
  // see http://scripts.sil.org/cms/scripts/page.php?site_id=nrsi&id=IWS-AppendixA
  assert(pStart);
  assert(pEnd);
  const char *pSrc;
  pGet->Empty();
  for(pSrc=pStart;pSrc<pEnd;)
    {
    TCHAR tChar=(*pSrc)&0xff;
    pSrc++;

    if((tChar&0x80)==0x00) // one-byte sequence
      {
      if(tChar=='\\')
        {
        tChar=*pSrc;
        pSrc++;
        switch(tChar)
          {
        case 'b':
          tChar='\b';
          break;
        case 'f':
          tChar='\f';
          break;
        case 'n':
          tChar='\n';
          break;
        case 'r':
          tChar='\r';
          break;
        case 't':
          tChar='\t';
          break;
        case 'u':
          assert(false); // \u not implemented
          break;
          }
        }
      pGet->AppendChar(tChar);
      }

    else // multi-byte sequence
      {
      TCHAR tcMask=0xe0;
      TCHAR tcPattern=0xc0;
      int nLen=2; // sequence length
      for(;nLen<=4;nLen++)
        {
        if((tChar&tcMask)==tcPattern)
          {
          DWORD dwResult=tChar&~tcMask;
          int nByte;
          for(nByte=1;nByte<nLen;nByte++)
            {
            dwResult<<=6;
            tChar=(*pSrc)&0xff;
            if((tChar&0xc0)!=0x80)
              return false;
            pSrc++;
            dwResult|=tChar&0x3f;
            }
          if(dwResult>0xffff)
            {
            pGet->AppendChar(0xd800 | (TCHAR)((dwResult>>16)-1) | (TCHAR)(dwResult>>10));
            pGet->AppendChar(0xdc00 | (TCHAR)(dwResult&0x3ff));
            }
          else
            pGet->AppendChar((TCHAR)dwResult);
          break;
          }
        tcMask=(tcMask>>1)|0x80;
        tcPattern=(tcPattern>>1)|0x80;
        }
      }

    }
  return true;    
  }

