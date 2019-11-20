//------------------------------------------------------------------------------------------------
// Author: D. Burger, Friedberg, Germany, <www.openABK.org>, <www.embu-sys.de>, <info@openABK.org>
//
// You are not allowed to remove this heading from the source code
// You are free to use this library under the terms of the
// Code Project Open Library, see <http://www.codeproject.com/info/cpol10.aspx>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//  _____  __   __  _____  _    _        ____              
// |  ___||  \ /  ||  _  \| |  | |      / ___|  _   _  ___ 
// |  __| |   V   ||  -  /| |  | |  __  \___ \ | | | |/ __|
// | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
// |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
//                                              |___/      
// Filename:    JsonFormatter.cpp
// Created:     2012-07-25 (08:19)
// Author:      D. Burger
// Description: JSON formatter for formatting data into a std::stringstream
//------------------------------------------------------------------------------------------------


#include "stdafx.h"
#include "JsonFormatter.h"
#include <assert.h>
#include <time.h>
#include <iomanip>
#include <limits>

#include <math.h>

#ifdef WINCE
#include <WceExtensions.h>
#endif


CJsonStreamBase &operator << (CJsonStreamBase &rDump, const char *pszValue)
  {
  // *(rDump.m_pDump)<<"\""<<pszValue<<"\"";
  *(rDump.m_pDump)<<'\"';
  for(;*pszValue;pszValue++)
    {
    char c=*pszValue;
    switch(c)
      {
    case '\"':
    case '\\':
    case '/':
      *(rDump.m_pDump)<<'\\'<<c;
      break;
    case '\b':
      *(rDump.m_pDump)<<'\\'<<'b';
      break;
    case '\f':
      *(rDump.m_pDump)<<'\\'<<'f';
      break;
    case '\n':
      *(rDump.m_pDump)<<'\\'<<'n';
      break;
    case '\r':
      *(rDump.m_pDump)<<'\\'<<'r';
      break;
    case '\t':
      *(rDump.m_pDump)<<'\\'<<'t';
      break;
    default:
      *(rDump.m_pDump)<<c;
      }
    }
  *(rDump.m_pDump)<<'\"';
  return rDump;
  }

CJsonStreamBase &operator << (CJsonStreamBase &rDump, int nValue)
  {
  *(rDump.m_pDump)<<nValue;
  return rDump;
  }

CJsonStreamBase &operator << (CJsonStreamBase &rDump, double dValue)
  {
  if(_isnan(dValue))
    {
    *(rDump.m_pDump)<<"null"; // *(rDump.m_pDump)<<"\"---.-\"";
    return rDump;
    }
  if(!_finite(dValue))
    {
    *(rDump.m_pDump)<<"null"; // *(rDump.m_pDump)<<"\"--\"";
    return rDump;
    }
  *(rDump.m_pDump)<<dValue;
  return rDump;
  }

CJsonStreamBase &operator << (CJsonStreamBase &rDump, const std::string &strValue)
  {
  //*(rDump.m_pDump)<<"\""<<strValue<<"\"";
  *(rDump.m_pDump)<<'\"';
  int nLength=(int)strValue.length();
  for(int nCol=0;nCol<nLength;nCol++)
    {
    char c=strValue[nCol];
    switch(c)
      {
    case '\"':
    case '\\':
    case '\'':
      *(rDump.m_pDump)<<'\\'<<c;
      break;
    case '\b':
      *(rDump.m_pDump)<<'\\'<<'b';
      break;
    case '\f':
      *(rDump.m_pDump)<<'\\'<<'f';
      break;
    case '\n':
      *(rDump.m_pDump)<<'\\'<<'n';
      break;
    case '\r':
      *(rDump.m_pDump)<<'\\'<<'r';
      break;
    case '\t':
      *(rDump.m_pDump)<<'\\'<<'t';
      break;
    default:
      *(rDump.m_pDump)<<c;
      }
    }
  *(rDump.m_pDump)<<'\"';
  return rDump;
  }


CJsonStreamBase &operator << (CJsonStreamBase &rDump, bool bValue)
  {
  if(bValue)
    *(rDump.m_pDump)<<"true";
  else
    *(rDump.m_pDump)<<"false";
  return rDump;
  }



//--------------------------------------------------------------------------
// CJsonStreamBase()       Constructor of CJsonStreamBase
// -----------------
// Input: pParent = parent object or array
// Return: 

CJsonStreamBase::CJsonStreamBase (CJsonStreamBase *pParent)
  : m_pParent(pParent)
  , m_nMemberCount(0)
  , m_pDump(NULL)
  , m_pChild(NULL) // currently have no child formatting an object or array into me
  , m_strCloseTag(NULL)
  , m_bClosed(false)
  {
  if(m_pParent)
    {
    pParent->m_pChild=this; // register me, i am the child currently formatting an array/object member
    m_pDump=pParent->m_pDump;
    pParent->BeginMember(); // since array/object is a member of the parent, ocassionally have to place a separator
    }
  }




//--------------------------------------------------------------------------
// ~CJsonStreamBase()      Destructor of CJsonStreamBase
// ------------------
// Input: -
// Return: 

CJsonStreamBase::~CJsonStreamBase (void)
  {
  Close();
  }


//--------------------------------------------------------------------------
// BeginMember()        writes an array/member separator, shall be called before the array member is formatted
// ----------------
// Input: -
// Return: 

void CJsonStreamBase::BeginMember (void)
  {
  if(m_nMemberCount>0)
    *m_pDump<<",";
  m_nMemberCount++;
  }


//--------------------------------------------------------------------------
// FormatDate()            formats date. converts local to UTC
// ------------
// Input: tmDate = date to be formatted, local time
// Return: -

void CJsonStreamBase::FormatDate (time_t tmDate)
  {
  assert(m_pDump);
  struct tm *pTm;
  #if defined(WINCE)
    struct tm tmTemp;
    pTm=gmtime(&tmDate,tmTemp);
  #elif defined (WIN32) // warning C4996: 'gmtime': This function or variable may be unsafe. Consider using gmtime_s instead.
    struct tm tmTemp;
    pTm=&tmTemp;
    gmtime_s(&tmTemp,&tmDate);
  #else
    pTm=gmtime(&tmDate);
  #endif
  *m_pDump<<"\"\\\"";
  *m_pDump << pTm->tm_year+1900 << '-' << std::setfill('0') << std::setw(2) << pTm->tm_mon+1 << '-' << std::setw(2)<<pTm->tm_mday << 'T' << std::setw(2)<<pTm->tm_hour << ':' << std::setw(2)<<pTm->tm_min << ':' << std::setw(2)<< pTm->tm_sec << ".000Z";
  *m_pDump<<"\\\"\"";
  }


//--------------------------------------------------------------------------
// Close()                 closes the object
// -------
// Input: -
// Return: 

/*virtual*/ void CJsonStreamBase::Close (void)
  {
  if(m_bClosed)
    return;
  if(m_pChild) // if currently a child writes an object or array, close it first
    m_pChild->Close();
  m_bClosed=true;
  if(m_pParent)
    m_pParent->m_pChild=NULL; // unregister me at the parent
  if(m_pDump&&m_strCloseTag)
    *m_pDump<<m_strCloseTag; // write the close tag
  }









//--------------------------------------------------------------------------
// CJsonStreamArray()      Constructor of CJsonStreamArray
// ------------------
// Input: pParent = parent json object or array
//        pszName = name of the array
// Return: 

CJsonStreamArray::CJsonStreamArray (CJsonStreamObject *pParent, const char *pszName)
  : CJsonStreamBase(pParent)
  {
  m_strCloseTag="]";
  assert(pszName[0]!='\0');
  *this<<pszName;
  *m_pDump<<":[";
  }

CJsonStreamArray::CJsonStreamArray (CJsonStreamObject *pParent, const std::string &strName)
  : CJsonStreamBase(pParent)
  {
  m_strCloseTag="]";
  assert(!strName.empty());
  *this<<strName;
  *m_pDump<<":[";
  }

CJsonStreamArray::CJsonStreamArray (CJsonStreamArray *pParent)
  : CJsonStreamBase(pParent)
  {
  m_strCloseTag="]";
  *m_pDump<<"[";
  }



//--------------------------------------------------------------------------
// ~CJsonStreamArray()     Destructor of CJsonStreamArray
// -------------------
// Input: -
// Return: 

CJsonStreamArray::~CJsonStreamArray (void)
  {
  Close();
  }


//--------------------------------------------------------------------------
// WriteValue()            writes integer value
// ------------
// Input: nValue = value to be written
// Return: 

void CJsonStreamArray::WriteValue (int nValue)
  {
  BeginMember(); // conditionally write separator and increment array member counter
  *this<<nValue;
  }


//--------------------------------------------------------------------------
// WriteValue()            writes bool value
// ------------
// Input: bValue = value to be written
// Return: 

void CJsonStreamArray::WriteValue (bool bValue)
  {
  BeginMember(); // conditionally write separator and increment array member counter
  *this<<bValue;
  }


//--------------------------------------------------------------------------
// WriteValue()            writes double precision value
// ------------
// Input: dValue = value to be written
// Return: 

void CJsonStreamArray::WriteValue (double dValue)
  {
  BeginMember(); // conditionally write separator and increment array member counter
  *this<<dValue;
  }


//--------------------------------------------------------------------------
// WriteValue()            writes string value
// ------------
// Input: pszValue = value to be written
// Return: 

void CJsonStreamArray::WriteValue (const char *pszValue)
  {
  BeginMember(); // conditionally write separator and increment array member counter
  *this<<pszValue;
  }


//--------------------------------------------------------------------------
// WriteValue()            writes string value
// ------------
// Input: strValue = value to be written
// Return: 

void CJsonStreamArray::WriteValue (const std::string &strValue)
  {
  BeginMember(); // conditionally write separator and increment array member counter
  *this<<strValue;
  }


//--------------------------------------------------------------------------
// WriteValue()            writes date and time
// ------------
// Input: tmDateLocal = local time to be written
// Return: 

void CJsonStreamArray::WriteValue (time_t tmDateLocal)
  {
  BeginMember(); // conditionally write separator and increment array member counter
  FormatDate(tmDateLocal);    
  }


//--------------------------------------------------------------------------
// WriteValue()            writes object
// ------------
// Input: objPut = object to be put to the array
// Return: 

void CJsonStreamArray::WriteValue (const CJsonFormatter &objPut)
  {
  assert(objPut.m_bClosed); // you have to put a formatter in its closed state! Pls close the formatter before putting it here!
  BeginMember(); // conditionally write separator and increment array member counter
  *m_pDump<<objPut.m_pDump->rdbuf();
  }















//--------------------------------------------------------------------------
// CJsonStreamObject()     Constructor of CJsonStreamObject
// -------------------
// Input: pParent = parent json object or array
//        pszName = name of the object
// Return: 

CJsonStreamObject::CJsonStreamObject (CJsonStreamObject *pParent, const char *pszName)
  : CJsonStreamBase(pParent)
  {
  m_strCloseTag="}";
  if(m_pDump)
    {
    assert(pszName[0]!='\0');
    *this<<pszName;
    *m_pDump<<":{";
    }
  }

CJsonStreamObject::CJsonStreamObject (CJsonStreamObject *pParent, const std::string &strName)
  : CJsonStreamBase(pParent)
  {
  m_strCloseTag="}";
  if(m_pDump)
    {
    assert(!strName.empty());
    *this<<strName;
    *m_pDump<<":{";
    }
  }

CJsonStreamObject::CJsonStreamObject (CJsonStreamArray *pParent)
  : CJsonStreamBase(pParent)
  {
  m_strCloseTag="}";
  if(m_pDump)
    *m_pDump<<"{";
  }


//--------------------------------------------------------------------------
// ~CJsonStreamObject()    Destructor of CJsonStreamObject
// --------------------
// Input: -
// Return: 

CJsonStreamObject::~CJsonStreamObject (void)
  {
  Close();
  }


//--------------------------------------------------------------------------
// BeginMember()           writes name of a member and conditionally a comma
// -------------
// Input: pszName = name of the member
// Return: -

void CJsonStreamObject::BeginMember (const char *pszName)
  {
  CJsonStreamBase::BeginMember(); // conditionally write separator and increment object member counter
  *this<<pszName;
  *m_pDump<<":";
  }

void CJsonStreamObject::BeginMember (void)
  {
  assert(false); // tried to introduce a member without name in an object. call BeginMember(const char *) instead
  }



//--------------------------------------------------------------------------
// WriteValue()            writes integer value
// ------------
// Input: pszName = name of the value
//        nValue = value to be written
// Return: -

void CJsonStreamObject::WriteValue (const char *pszName, int nValue)
  {
  BeginMember(pszName); // conditionally write separator and increment object member counter
  *this<<nValue;
  }


//--------------------------------------------------------------------------
// WriteValue()            writes integer value
// ------------
// Input: pszName = name of the value
//        nValue = value to be written
// Return: 

void CJsonStreamObject::WriteValue (const char *pszName, bool bValue)
  {
  BeginMember(pszName); // conditionally write separator and increment object member counter
  *this<<bValue;
  }


//--------------------------------------------------------------------------
// WriteValue()            writes double precision value
// ------------
// Input: pszName = name of the value
//        nValue = value to be written
// Return: -

void CJsonStreamObject::WriteValue (const char *pszName, double dValue)
  {
  BeginMember(pszName); // conditionally write separator and increment object member counter
  *this<<dValue;
  }


//--------------------------------------------------------------------------
// WriteValue()            writes string value
// ------------
// Input: pszName = name of the value
//        nValue = value to be written
// Return: -

void CJsonStreamObject::WriteValue (const char *pszName, const char *pszValue)
  {
  BeginMember(pszName); // conditionally write separator and increment object member counter
  *this<<pszValue;
  }


//--------------------------------------------------------------------------
// WriteValue()            writes string value
// ------------
// Input: pszName = name of the value
//        nValue = value to be written
// Return: -

void CJsonStreamObject::WriteValue (const char *pszName, const std::string &strValue)
  {
  BeginMember(pszName); // conditionally write separator and increment object member counter
  *this<<strValue;
  }


//--------------------------------------------------------------------------
// WriteValue()            writes date and time
// ------------
// Input: pszName = name of entity
//        tmDateLocal = local time to be output
// Return: 

void CJsonStreamObject::WriteValue (const char *pszName, time_t tmDateLocal)
  {
  BeginMember(pszName); // conditionally write separator and increment array member counter
  FormatDate(tmDateLocal);
  }


//--------------------------------------------------------------------------
// WriteValue()            writes object
// ------------
// Input: pszName = name of the object
//        objPut = object to be written
// Return: 

void CJsonStreamObject::WriteValue (const char *pszName, const CJsonFormatter &objPut)
  {
  assert(objPut.m_bClosed); // you have to put a formatter in its closed state! Pls close the formatter before putting it here!
  BeginMember(pszName); // conditionally write separator and increment array member counter
  *m_pDump<<objPut.m_pDump->rdbuf();
  }


//--------------------------------------------------------------------------
// WriteValue()            writes members of an object into the object
// ------------
// Input: objPut = object whos members shall be written to the this object
// Return: 

void CJsonStreamObject::WriteValue (const CJsonFormatter &objPut)
  {
  assert(objPut.m_bClosed); // you have to put a formatter in its closed state! Pls close the formatter before putting it here!
  CJsonStreamBase::BeginMember(); // conditionally write separator and increment array member counter
  std::stringstream *pSource; // source string stream
  pSource=objPut.m_pDump;
  std::streampos spRestore=pSource->tellg();
  char cDummy;
  *pSource>>cDummy; // consume the leading {
  assert(cDummy=='{');
  *m_pDump << pSource->rdbuf(); // copy object, with its trailing }
  m_pDump->seekp(-1,std::ios::end); // cut the trailing }
  pSource->seekg(spRestore);
  }












//--------------------------------------------------------------------------
// CJsonFormatter()        Constructor of CJsonFormatter
// ----------------
// Input: -
// Return: 

CJsonFormatter::CJsonFormatter (void)
  : CJsonStreamObject(NULL) // NULL due to no parent
  {
  m_strFormat.precision(10); // set the output precision somewhat higher than default
  m_pDump=&m_strFormat;
  InitNew();
  }


//--------------------------------------------------------------------------
// ~CJsonFormatter()       Destructor of CJsonFormatter
// -----------------
// Input: -
// Return: 

/*virtual*/ CJsonFormatter::~CJsonFormatter (void)
  {
  m_pDump=NULL;
  }


//--------------------------------------------------------------------------
// InitNew()               inits to virgin state
// ---------
// Input: -
// Return: 

void CJsonFormatter::InitNew (void)
  {
  m_strFormat.clear(); // 20.Feb.15 D. Burger added, see also http://stackoverflow.com/questions/2848087/how-to-clear-stringstream
  m_strFormat.str(std::string()); // empty the buffer
  m_nMemberCount=0;
  m_bClosed=false;
  m_pChild=NULL;
  *m_pDump<<"{"; // do what the base class constructor usually does
  }


//--------------------------------------------------------------------------
// GetStream()             closes the object and returns the string stream
// -----------
// Input: -
// Return: 

std::stringstream *CJsonFormatter::GetStream (void)
  {
  Close();
  return &m_strFormat;
  }

