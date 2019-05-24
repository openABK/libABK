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
// |  __| |   ´   ||  -  /| |  | |  __  \___ \ | | | |/ __|
// | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
// |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
//                                              |___/      
// Filename:    JsonParser.cpp
// Created:     2012-07-19 (15:48)
// Author:      D. Burger
// Description: JSON parser
//------------------------------------------------------------------------------------------------




#include "stdafx.h"
#include "JsonParser.h"
#include <assert.h>
#include <time.h>
#include <limits>
#include <stdio.h>
#include <time.h>

#include "CrossPlatform.h"

#ifdef WINCE
#include <WceExtensions.h>
#endif


using namespace std;


//--------------------------------------------------------------------------
// CJsonParser()           Constructor of CJsonParser
// -------------
// Input: pStartOfExpression = start of json expression string
// Return: 

CJsonParser::CJsonParser (const char *pStartOfExpression)
  {
  m_pNameEnd=NULL;
  m_pValueEnd=NULL;
  Restart(pStartOfExpression);
  }



//--------------------------------------------------------------------------
// ~CJsonParser()          Destructor of CJsonParser
// --------------
// Input: -
// Return: 

CJsonParser::~CJsonParser ()
  {
  }


//--------------------------------------------------------------------------
// Restart()               restarts the parser
// ---------
// Input: pStartOfExpression = start of json expression string
// Return: -

void CJsonParser::Restart (const char *pStartOfExpression)
  {
  m_pPos=SkipWhite(pStartOfExpression);
  m_nNestingLevel=0;
  m_bObject[0]=false;
  if(*m_pPos=='{')
    {
    m_nType=CJsonParser::TYPE_OBJECT;
    m_pPos++;
    PushNesting(true);
    }
  else if(*m_pPos=='[') // if root is an array (
    {
    m_nType=CJsonParser::TYPE_ARRAY;
    m_pPos++;
    PushNesting(false);
    }
  else
    {
    m_nType=CJsonParser::TYPE_INVALID;
    m_nNestingLevel=0;
    }
  m_pPos=SkipWhite(m_pPos);
  m_pNameStart=""; // provide an empty name for the root object
  m_pNameEnd=NULL;
  m_pValueStart=NULL;
  m_pValueEnd=NULL;
  //m_pPos=SkipWhite(m_pPos);
  Iterate(); // jump to the first element
  }




//--------------------------------------------------------------------------
// operator++()            iterate (prefix ++)
// ------------
// Input: -
// Return: reference to this

CJsonParser& CJsonParser::operator++ ()
  {
  Iterate();
  return *this;
  }



//--------------------------------------------------------------------------
// Iterate()               iterates one step
// ---------
// Input: -
// Return: 

void CJsonParser::Iterate (void)
  {
  if(m_nType==CJsonParser::TYPE_INVALID)
    return;
  if(*m_pPos=='\0')
    {
    m_nType=CJsonParser::TYPE_END;
    return;
    }
  
  m_nType=CJsonParser::TYPE_INVALID;
  if(IsObject())
    {
    if(*m_pPos=='}') // end of object
      {
      m_pPos++;
      m_nType=CJsonParser::TYPE_END;
      }
    else
      {
      if(*m_pPos==',')
        m_pPos++;
      IsolateName(); // get a member name of the object
      if(*m_pPos=='{') // object in the object
        {
        m_pPos++;
        m_nType=CJsonParser::TYPE_OBJECT;
        }
      else if(*m_pPos=='[') // array in the object
        {
        m_pPos++;
        m_nType=CJsonParser::TYPE_ARRAY;
        }
      else
        {
        m_nType=IsolateValue(); // data member in the array
        }
      }
    }
  else
    {
    m_pNameStart=m_pNameEnd; // while in the array, no name is available
    if(*m_pPos==']') // end of array
      {
      m_pPos++;
      m_nType=CJsonParser::TYPE_END;
      }
    else
      {
      if(*m_pPos==',')
        m_pPos++;
      if(*m_pPos=='{') // object in the array
        {
        m_pPos++;
        //IsolateName();
        //m_nType=IsolateValue();
        m_nType=CJsonParser::TYPE_OBJECT;
        }
      else if(*m_pPos=='[') // array in the array
        {
        m_pPos++;
        m_nType=CJsonParser::TYPE_ARRAY;
        }
      else
        {
        m_nType=IsolateValue(); // value in the array
        }
      }
    }


  switch (m_nType)
    {
    case CJsonParser::TYPE_END:
      PopNesting();
      break;
    case CJsonParser::TYPE_OBJECT:
      PushNesting(true);
      break;
    case CJsonParser::TYPE_ARRAY:
      PushNesting(false);
      break;
    }
  m_pPos=SkipWhite(m_pPos);
  }




//--------------------------------------------------------------------------
// SkipWhite()             skips white characters
// -----------
// Input: pStart = starting point for scanning
// Return: pointer to the first character not being a white space

/*static*/ const char *CJsonParser::SkipWhite (const char * pStart)
  {
  for(;*pStart;pStart++)
    {
    char cCurrent=*pStart;
    if((cCurrent!=' ')&&(cCurrent!='\r')&&(cCurrent!='\n')&&(cCurrent!='\t'))
      break;
    }
  return pStart;
  }


//--------------------------------------------------------------------------
// SearchStringEnd()       searches the end of a string
// -----------------
// Input: pScan = pointer to firs char of a string (not the ")
// Return: pointer to the terminating '\"' or the terminating '\0' if error

/*static*/ const char *CJsonParser::SearchStringEnd (const char *pScan)
  {
  for(;*pScan;pScan++)
    {
    if(*pScan=='\"')
      break;
    if((pScan[0]=='\\')&&(pScan[1]!='\0')) // skip the escaped char
      pScan++;
    }
  return pScan;
  }


//--------------------------------------------------------------------------
// SearchValueEnd()        searches the end of a value
// ----------------
// Input: pScan = first character of a value
// Return: pointer to the first char bejond the value

/*static*/ const char *CJsonParser::SearchNumericEnd (const char *pScan)
  {
  for(;*pScan;pScan++)
    {
    char c=*pScan;
    if((c==',')||(c==']')||(c=='}')||(c=='\r')||(c=='\n')||(c=='\t')||(c==' '))
      break;
    }
  return pScan;
  }



//--------------------------------------------------------------------------
// IsolateName()           extracts a member name and skips the :
// -------------
// Input: -
// Return: -

void CJsonParser::IsolateName (void)
  {
  m_pPos=SkipWhite(m_pPos);
  if(*m_pPos=='\"')
    {
    m_pNameStart=m_pPos+1;
    m_pPos=SearchStringEnd(m_pNameStart);
    m_pNameEnd=m_pPos;
    m_pPos++; // jump behind the "
    }
  else
    {
    m_pNameStart=m_pPos;
    for(;*m_pPos;m_pPos++)
      {
      char c=*m_pPos;
      if((c==' ')||(c=='\r')||(c=='\n')||(c=='\t')||(c==':')||(c=='}')||(c==']'))
        break;
      }
    m_pNameEnd=m_pPos;
    }
  m_pPos=SkipWhite(m_pPos);
  if(*m_pPos==':')
    m_pPos++;
  m_pPos=SkipWhite(m_pPos);
  }


//--------------------------------------------------------------------------
// IsolateValue()          extracts a value of a member
// --------------
// Input: -
// Return: STRING or NUMERIC

CJsonParser::TYPE CJsonParser::IsolateValue (void)
  {
  m_pPos=SkipWhite(m_pPos);
  if(*m_pPos=='\"')
    {
    m_pValueStart=m_pPos+1;
    m_pPos=SearchStringEnd(m_pValueStart);
    m_pValueEnd=m_pPos;
    m_pPos++; // jump behind the "
    m_pPos=SkipWhite(m_pPos);
    return CJsonParser::TYPE_STRING;
    }
  else
    {
    m_pValueStart=m_pPos;
    m_pPos=SearchNumericEnd(m_pPos);
    m_pValueEnd=m_pPos;
    m_pPos=SkipWhite(m_pPos);
    if(m_pValueEnd==m_pValueStart) // empty entity is invalid
      return CJsonParser::TYPE_INVALID;
    return CJsonParser::TYPE_NUMERIC;
    }
  return CJsonParser::TYPE_INVALID;
  }




//--------------------------------------------------------------------------
// PushNesting()           pushes an object/array to the nesting stack
// -------------
// Input: bObject = true for object, false for array
// Return: 

void CJsonParser::PushNesting (bool bObject)
  {
  assert(m_nNestingLevel<(JSONPARSER_MAXNESTING-1));
  m_nNestingLevel++;
  m_bObject[m_nNestingLevel]=bObject;
  }




//--------------------------------------------------------------------------
// PopNesting()            pops a nesting level
// ------------
// Input: -
// Return: 

void CJsonParser::PopNesting (void)
  {
  assert(m_nNestingLevel>0);
  m_nNestingLevel--;  
  }


//--------------------------------------------------------------------------
// IsObject()              returns true if currently parsing an object
// ----------
// Input: -
// Return: 

bool CJsonParser::IsObject (void)
  {
  return m_bObject[m_nNestingLevel];
  }




//--------------------------------------------------------------------------
// IsDone()                returns true if scanning done (either with or w/o success)
// --------
// Input: -
// Return: true if at end of the current level

bool CJsonParser::IsDone (void) const
  {
  if((m_nType==CJsonParser::TYPE_END)||(m_nType==CJsonParser::TYPE_INVALID)||(m_nNestingLevel<=0))
    return true;
  return false;
  }




//--------------------------------------------------------------------------
// GetName()               returns name of the object/array/value
// ---------
// Input: -
// Return: name of the object, empty string if item has no name
//         NULL if item is an array member or a closing tag

bool CJsonParser::GetName (std::string &strGet) const
  {
  return UnescapeString(m_pNameStart,m_pNameEnd,&strGet);
  }


//--------------------------------------------------------------------------
// GetValueString()        returns the string form of the value
// ----------------
// Input: -
// Return: true on success, false on error

bool CJsonParser::GetValueString (std::string &strGet) const
  {
  return UnescapeString(m_pValueStart,m_pValueEnd,&strGet); // copy the value to strGet
  }


//--------------------------------------------------------------------------
// GetValueDouble()        returns double precision form of the value
// ----------------
// Input: dValueGet = reference to return the result. will be set to NAN on error
// Return: true on success, false on error

bool CJsonParser::GetValueDouble (double &dValueGet) const
  {
  struct
    {
    const char *pszFormat; // format to be converted to the value
    int nLen; // length of the format (defined here to avoid use of strlen())
    double dValue; // value associated to the formatted string
    } static const s_abnormal[]=
    {
      {"null",    4,    numeric_limits<double>::quiet_NaN()   },
      {"1.#QNAN", 7,    numeric_limits<double>::quiet_NaN()   },
      {"1.#INF",  6,     numeric_limits<double>::infinity()   },
      {"-1.#INF", 7,    -numeric_limits<double>::infinity()   },
    };

  //static const char *pszAbnormalFormats[]={"null"                             , "1.#QNAN"                          , "1.#INF"                          , "-1.#INF"                          , NULL};
  //static double dAbnormalValues[]=        {numeric_limits<double>::quiet_NaN(), numeric_limits<double>::quiet_NaN(), numeric_limits<double>::infinity(), -numeric_limits<double>::infinity(), 0.  };
  //static int nAbnormalLen[]=              { 4                                 ,  7                                 , 6                                 , 7                                  , 0   }; // length of the formats
  assert(m_pValueStart);
  assert(m_pValueEnd);
  int nValueLen=(int)(m_pValueEnd-m_pValueStart);
  for(int nAbnormalCase=0;nAbnormalCase<_countof(s_abnormal);nAbnormalCase++)
    {
    if(nValueLen==s_abnormal[nAbnormalCase].nLen)
      {
      if(!strncmp(m_pValueStart,s_abnormal[nAbnormalCase].pszFormat,nValueLen)) // if decoded a special case
        {
        dValueGet=s_abnormal[nAbnormalCase].dValue;
        return true;
        }
      }
    }
#if defined(WIN32) && !defined(WINCE)
  int nConverted=_snscanf(m_pValueStart,nValueLen,"%lf",&dValueGet);
#else
  char cTemp=const_cast<char *>(m_pValueStart)[nValueLen];
  const_cast<char *>(m_pValueStart)[nValueLen]='\0';
  int nConverted=/*_snscanf*/sscanf(m_pValueStart/*,nValueLen*/,"%lf",&dValueGet);
  const_cast<char *>(m_pValueStart)[nValueLen]=cTemp;
#endif
  if(nConverted!=1)
    {
    dValueGet=numeric_limits<double>::quiet_NaN();
    return false;
    }
  return true;
  }


//--------------------------------------------------------------------------
// GetValueInt()           returns integer form of the value
// -------------
// Input: ref to return the result. will be left untouchen in case of an error
// Return: true on success, false on error

bool CJsonParser::GetValueInt (int &nValueGet) const
  {
  assert(m_pValueStart);
  int nValue=0;
  int nValueLen=(int)(m_pValueEnd-m_pValueStart);

#if defined(WIN32) && !defined(WINCE)
  int nConverted=_snscanf(m_pValueStart,nValueLen,"%ld",&nValue);
#else
  char cTemp=const_cast<char *>(m_pValueStart)[nValueLen];
  const_cast<char *>(m_pValueStart)[nValueLen]='\0';
  int nConverted=/*_snscanf*/sscanf(m_pValueStart/*,nValueLen*/,"%d",&nValue);
  const_cast<char *>(m_pValueStart)[nValueLen]=cTemp;
#endif

  if(nConverted!=1)
    return false;
  nValueGet=nValue;
  return true;
  }


//--------------------------------------------------------------------------
// GetValueBool()          returns boolean value
// --------------
// Input: ref to return the result. will be left untouched in case of an error
// Return: true on success, false on error

bool CJsonParser::GetValueBool (bool &bValueGet) const
  {
  assert(m_pValueStart);
  bool bSuccess=false;
  if(!strncmp("true",m_pValueStart,m_pValueEnd-m_pValueStart))
    {
    bValueGet=true;
    return true;
    }
  else if(!strncmp("false",m_pValueStart,m_pValueEnd-m_pValueStart))
    {
    bValueGet=false;
    return true;
    }
  return false;
  }



//--------------------------------------------------------------------------
// SkipItem()              skips the item (array or object)
// ----------
// Input: strItemName = name of item to be tested for. if name matches, item
//                      will be skipped. If NULL, item will be skipped always
// Return: true if no syntax error found, false on syntax error

bool CJsonParser::SkipItem (const char *pszItemName/*=NULL*/)
  {
  //assert((m_nType==ARRAY)||(m_nType==OBJECT));
  assert(m_nNestingLevel>0);
  if((m_nType==CJsonParser::TYPE_ARRAY)||(m_nType==CJsonParser::TYPE_OBJECT))
    {
    bool bSkip=true;
    if(pszItemName)
      {
      if(!CompareUnescapedString(pszItemName,m_pNameStart,m_pNameEnd)) // if(strncmp(strItemName,m_pNameStart,m_pNameEnd-m_pNameStart))
        bSkip=false;
      }
    if(!bSkip)
      return true;
    int nNestingLevel=m_nNestingLevel;
    while(m_nNestingLevel>=nNestingLevel)
      {
      Iterate();
      if(m_nType==CJsonParser::TYPE_INVALID)
        return false;
      }
    }
  return true;
  }




//--------------------------------------------------------------------------
// GetPosition()           returns the actual scanning position, e.g. for error reporting
// -------------
// Input: -
// Return: string from the current position

const char *CJsonParser::GetPosition (void) const
  {
  return m_pPos;
  }




//--------------------------------------------------------------------------
// ExtractValue()          extracts string
// --------------
// Input: strName = name to test for
//        pGet = pointer to receive the value
//               for time, the local time is returned
// Return: true if value was extracted, false if name/type was not matching

bool CJsonParser::ExtractValue (const char *pszName, std::string *pGet) const
  {
  if(TestForName(pszName))
    return ExtractValue(pGet);
  return false;
  }

bool CJsonParser::ExtractValue (const char *pszName, int *pGet) const
  {
  if(TestForName(pszName))
    return ExtractValue(pGet);
  return false;
  }

bool CJsonParser::ExtractValue (const char *pszName, double *pGet) const
  {
  if(TestForName(pszName))
    return ExtractValue(pGet);
  return false;
  }

bool CJsonParser::ExtractValue (const char *pszName, bool *pGet) const
  {
  if(TestForName(pszName))
    return ExtractValue(pGet);
  return false;
  }

bool CJsonParser::ExtractValue (const char *pszName, time_t *pGet) const
  {
  if(TestForName(pszName))
    return ExtractValue(pGet);
  return false;
  }



//--------------------------------------------------------------------------
// ExtractValue()          gets value out of an array
// --------------
// Input: pGet = pointer to return the value
// Return: 

bool CJsonParser::ExtractValue (std::string *pGet) const
  {
  if(m_nType!=CJsonParser::TYPE_STRING)
    return false;
  assert(m_pValueStart);
  if(!m_pValueStart)
    return false;
  return UnescapeString(m_pValueStart,m_pValueEnd,pGet);
  }

bool CJsonParser::ExtractValue (int *pGet) const
  {
  if(m_nType!=CJsonParser::TYPE_NUMERIC)
    return false;
  assert(m_pValueStart);
  if(!m_pValueStart)
    return false;
  bool bSuccess=GetValueInt(*pGet);
  return bSuccess;
  }

bool CJsonParser::ExtractValue (double *pGet) const
  {
  if(m_nType!=CJsonParser::TYPE_NUMERIC)
    return false;
  assert(m_pValueStart);
  if(!m_pValueStart)
    return false;
  bool bSuccess=GetValueDouble(*pGet);
  return bSuccess;
  }

bool CJsonParser::ExtractValue (bool *pGet) const
  {
  if(m_nType!=CJsonParser::TYPE_NUMERIC)
    return false;
  assert(m_pValueStart);
  if(!m_pValueStart)
    return false;
  if(!strncmp("true",m_pValueStart,m_pValueEnd-m_pValueStart))
    {
    *pGet=true;
    return true;
    }
  if(!strncmp("false",m_pValueStart,m_pValueEnd-m_pValueStart))
    {
    *pGet=false;
    return true;
    }
  return false;
  }

bool CJsonParser::ExtractValue (time_t *pGet) const
  {
  if(m_nType!=CJsonParser::TYPE_STRING)
    return false;
  assert(m_pValueStart);
  if(!m_pValueStart)
    return false;
  return ScanDate(m_pValueStart,m_pValueEnd-m_pValueStart,pGet);
  }



//--------------------------------------------------------------------------
// TestForName()           tests if current position matches the name
// -------------
// Input: strName = name to be tested for
// Return: true if current position has the name

bool CJsonParser::TestForName (const char *pszName) const
  {
  if((m_pNameStart) && (CompareUnescapedString(pszName,m_pNameStart,m_pNameEnd)))
    return true;
  return false;
  }


//--------------------------------------------------------------------------
// TestArray()             returns true if current is start of array with specified name
// -----------
// Input: strName = name to test for
// Return: true if current position is start of array with the specified name

bool CJsonParser::TestArray (const char *pszName) const
  {
  if((m_nType==CJsonParser::TYPE_ARRAY)&&(TestForName(pszName)))
    return true;
  return false;
  }

//--------------------------------------------------------------------------
// TestArray()             test for array and returns name of it
// -----------
// Input: pNameGet = pointer to return the name of the array
// Return: true if current position is start of array and the name was 
//              copied to strNameGet

bool CJsonParser::TestArray (std::string *pNameGet) const
  {
  if(m_nType==CJsonParser::TYPE_ARRAY)
    {
    return UnescapeString(m_pNameStart,m_pNameEnd,pNameGet); // copy the name to pNameGet
    }
  return false;
  }


//--------------------------------------------------------------------------
// TestArray()             test for array within an array
// -----------
// Input: -
// Return: 

bool CJsonParser::TestArray (void) const
  {
  return m_nType==CJsonParser::TYPE_ARRAY;
  }



//--------------------------------------------------------------------------
// TestObject()            returns true if current is start of object with specified name
// ------------
// Input: strName = name to test for
// Return: true if current position is start of object with the specified name

bool CJsonParser::TestObject (const char *pszName) const
  {
  if(m_nType==CJsonParser::TYPE_OBJECT)
    {
    if(!m_pNameStart) // an object within an array, no name to be compared
      return true;
    return TestForName(pszName);
    }
  return false;
  }

//--------------------------------------------------------------------------
// TestObject()            test for object and returns name of it
// ------------
// Input: pNameGet = pointer to return the name of the object
// Return: true if current position is start of object and the name was
//              copied to strNameGet

bool CJsonParser::TestObject (std::string *pNameGet) const
  {
  if(m_nType==CJsonParser::TYPE_OBJECT)
    {
    return UnescapeString(m_pNameStart,m_pNameEnd,pNameGet); // copy the name to pNameGet
    }
  return false;
  }


//--------------------------------------------------------------------------
// TestObject()            test for object within an array
// ------------
// Input: -
// Return: true if current position is start of an object

bool CJsonParser::TestObject (void) const
  {
  return m_nType==CJsonParser::TYPE_OBJECT;
  }




//--------------------------------------------------------------------------
// MyLocalTime()           converts linear time into component time with locale and daylight saving offset (UTC to local)
// -------------
// Input: pDest = [out] local time as discrete components
//        pSrc = [in] UTC, linear time
// Return: -

static void MyLocalTime (struct tm *pDest, const time_t *pSrc)
  {
#if defined(WIN32) && !defined(WINCE)
  localtime_s(pDest,pSrc);
#else
  struct tm *pTmTemp=localtime(pSrc);
  memcpy(pDest,pTmTemp,sizeof(struct tm));
#endif
  }


//--------------------------------------------------------------------------
// MyGmTime()              converts linear time into component time without locale and daylight saving offset
// ----------
// Input: pDest = [out] component date
//        pSrc = [in] linear date
// Return: -

static void MyGmTime (struct tm *pDest, const time_t *pSrc)
  {
#ifdef WINCE
  gmtime(pSrc,*pDest);
#elif defined(WIN32)
  gmtime_s(pDest,pSrc);
#else
  struct tm *pTmTemp=gmtime(pSrc);
  assert(pTmTemp);
  memcpy(pDest,pTmTemp,sizeof(struct tm));
#endif
  }


//--------------------------------------------------------------------------
// ScanDate()              scans date into time_t
// ----------
// Input: pszDate = string containing the formatted date
//        nLen = length of pszDate
//        pReturn = pointer to return the decoded date. Result is for local time zone
// Return: true on success, false on error

bool CJsonParser::ScanDate (const char *pszDate, size_t nLen, time_t *pReturn) const
  {
  bool bSuccess=false;
  time_t ttResult=0;
  static const char *pszMonthNames[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
  
  if((nLen>2)&&(pszDate[0]=='\\')&&(pszDate[1]=='\"')) // suppress leading \" and ". Dont accept invalid date formats to speed-up destinguishing date from general purpose string
    {
    pszDate+=2;
    nLen-=2;
    struct tm tmScan;
    memset(&tmScan,0,sizeof(tmScan));
    char cZ;
    char cTemp=const_cast<char *>(pszDate)[nLen];
    const_cast<char *>(pszDate)[nLen]='\0';
    const char *pszScanFmt="%04d-%02d-%02dT%02d:%02d:%02d.000Z";
#if defined WIN32 && !defined(WINCE)
    _snscanf_s(pszDate,nLen,pszScanFmt,&tmScan.tm_year,&tmScan.tm_mon,&tmScan.tm_mday,&tmScan.tm_hour,&tmScan.tm_min,&tmScan.tm_sec,&cZ);
#else
    /*_snscanf*/sscanf(pszDate/*,nLen*/,pszScanFmt,&tmScan.tm_year,&tmScan.tm_mon,&tmScan.tm_mday,&tmScan.tm_hour,&tmScan.tm_min,&tmScan.tm_sec,&cZ);
#endif
    const_cast<char *>(pszDate)[nLen]=cTemp;
    tmScan.tm_year-=1900;
    tmScan.tm_mon--;
    tmScan.tm_isdst=false;
    ttResult=mktime(&tmScan); // convert to calendar UTC time. Since mktime() converts from local to UTC, the result has an offset which will be compenstated later. The offset does not contain daylight saving effect
    if(ttResult==-1)
      {
      char strDay[21];
      char strMonth[21];
      char strUtc[21];
      // "\"Tue May 22 10:30:48 UTC+0200 2012\""
      int nConverted;
      char cTemp=const_cast<char *>(pszDate)[nLen];
      const_cast<char *>(pszDate)[nLen]='\0';
#if defined WIN32 && !defined(WINCE)
      nConverted=        _snscanf_s(pszDate,  nLen  ,"%20s %20s %d %d:%d:%d %8s %d",strDay,20,strMonth,20,&tmScan.tm_mday,&tmScan.tm_hour,&tmScan.tm_min,&tmScan.tm_sec,&strUtc,20,&tmScan.tm_year);
#else
      nConverted=/*_snscanf*/sscanf(pszDate/*,nLen*/,"%20s %20s %d %d:%d:%d %8s %d",strDay,strMonth,&tmScan.tm_mday,&tmScan.tm_hour,&tmScan.tm_min,&tmScan.tm_sec,&strUtc,&tmScan.tm_year);
#endif
      const_cast<char *>(pszDate)[nLen]=cTemp;
      if(nConverted==8)
        {
        int nMonth;
        for(nMonth=0;nMonth<12;nMonth++)
          {
          if(!strcmp(pszMonthNames[nMonth],strMonth))
            break;
          }
        if(nMonth<12)
          {
          tmScan.tm_mon=nMonth;
          if(!strncmp(strUtc,"UTC+",4)) // if UTC+xxx, it is likely to be a local time
            {
            tmScan.tm_year-=1900;
            tmScan.tm_isdst=false;
            ttResult=mktime(&tmScan); // convert to calendar UTC time
            if(ttResult>0)
              {
              if(tmScan.tm_isdst)
                ttResult-=3600;
              bSuccess=true;
              }
            }
          }
        }
      }
    else // mktime succeeded
      {
      // evaluate offset between UTC and local
      time_t tmUtcToLocalZone=0; // offset in seconds to be added when converting from UTC to local without daylight saving effects. I.e. for Germany, this is 3600
      time_t tmNormalToDaylightSaving=0; // offset in seconds to be added when converting from normal time to daylight saving time
      if(1)
        {
        const time_t ttDummy=ttResult/* -(3600*24*31*8)*/;
        struct tm tmTemp;
        MyLocalTime(&tmTemp,&ttDummy); // convert something from UTC to local. This is done only to find out whether daylight saving is active at the given time
        if(tmTemp.tm_isdst>0)
          tmNormalToDaylightSaving=3600;
        MyGmTime(&tmTemp,&ttDummy);
        tmTemp.tm_isdst=false;
        time_t ttUtc=mktime(&tmTemp); // convert back, but now with offset from local to UTC
        tmUtcToLocalZone=ttDummy-ttUtc;
        }
      ttResult+=tmUtcToLocalZone; // compensate the unwanted effect of mktime
      ttResult+=tmUtcToLocalZone; // convert to local zone
      ttResult+=tmNormalToDaylightSaving; // convert to daylight saving
      bSuccess=true;
      }

    }
  *pReturn=ttResult;
  return bSuccess;
  }





//--------------------------------------------------------------------------
// UnescapeString()        unescapes a string
// ----------------
// Input: pStart = [in] start of the escaped source string
//        pEnd = [in] end of the escaped source string. It points to the fist position behind the unescaped string
//        pGet = [out] string to receive the result
// Return: true if succeeded, false if unsupported character encountered

/*static*/ bool CJsonParser::UnescapeString (const char *pStart, const char *pEnd, std::string *pGet)
  {
  assert(pStart);
  assert(pEnd);
  assert(pGet);
  bool bSuccess=true;
  if(pStart==pEnd)
    pGet->clear();
  else
    {
    int nJunkCount=0;
    const char *pSrc;
    char cBuf[256]; // temporary buffer in order to avoid frequent append() operations
    size_t nBufEntities=0; // number of entities in the buffer
    for(pSrc=pStart;pSrc<pEnd;pSrc++)
      {
      char c=*pSrc;
      if(c=='\\')
        {
        pSrc++;
        c=*pSrc;
        switch(c)
          {
        case 'b':
          c='\b';
          break;
        case 'f':
          c='\f';
          break;
        case 'n':
          c='\n';
          break;
        case 'r':
          c='\r';
          break;
        case 't':
          c='\t';
          break;
        case 'u':
          assert(false); // \u not implemented
          bSuccess=false;
          break;
          }
        }
      cBuf[nBufEntities++]=c;
      if(nBufEntities>=_countof(cBuf))
        {
        if(nJunkCount==0)
          pGet->assign(cBuf,nBufEntities);
        else
          pGet->append(cBuf,nBufEntities);
        nBufEntities=0;
        ++nJunkCount;
        }
      }
    if(nBufEntities)
      {
      if(nJunkCount==0)
        pGet->assign(cBuf,nBufEntities);
      else
        pGet->append(cBuf,nBufEntities);
      }
    }
  return bSuccess;
  }


//--------------------------------------------------------------------------
// CompareUnescapedString() compares unescaped string with escaped string
// ------------------------
// Input: strUnescaped = an unescaped string
//        pStart = pointer to first character in the escaped string
//        pEnd = pointer to char after the last in the escaped string
// Return: true if equal, false if different

/*static*/ bool CJsonParser::CompareUnescapedString (const char *pszUnescaped, const char *pStart, const char *pEnd)
  {
  assert(pStart);
  assert(pEnd);
  const char *pSrc;
  for(pSrc=pStart;pSrc<pEnd;pSrc++)
    {
    char c=*pSrc;
    if(c=='\\')
      {
      pSrc++;
      c=*pSrc;
      switch(c)
        {
      case 'b':
        c='\b';
        break;
      case 'f':
        c='\f';
        break;
      case 'n':
        c='\n';
        break;
      case 'r':
        c='\r';
        break;
      case 't':
        c='\t';
        break;
      case 'u':
        assert(false); // \u not implemented
        break;
        }
      }
    if(*pszUnescaped!=c)
      return false; // differences
    pszUnescaped++;
    }
  if(pszUnescaped[0]=='\0') // if same length
    return true; // no differences found
  return false; // strunescaped started the same but is longer
  }

