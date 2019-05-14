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
// Filename:    AbkVarRef.cpp
// Created:     2012-05-07 (09:22)
// Author:      D. Burger
// Description: Variable reference
//------------------------------------------------------------------------------------------------


#include "stdafx.h"
#include "VarRef.h"
#include "ValuesFromSpec.h"
#include "JsonFormatter.h"
#include "Daq.h"

#include <float.h>
#include <assert.h>
#include <iostream>
#include <string>
#include <sstream>

using namespace std;

namespace Abk {




//--------------------------------------------------------------------------
// CMeta()                  constructor, inits to default meta data
// ------
// Input: -
// Return: 

CVarRef::CMeta::CMeta ()
  {
  m_dwValidMemberFlags=0; // each member gets a valid flag. a valid member will be serialized to the metadata json
  m_dRangeMin=0.;
  m_dRangeMax=100.; 
  m_dFactor=1.;
  m_dOffset=0.;
  for(int nThreshold=0;nThreshold<SEVERITY_THRESHOLD_COUNT;nThreshold++)
    m_dThresholds[nThreshold]=0.;
  m_nFractDigits=2; // number of suggested fractional digits
  }


//--------------------------------------------------------------------------
// SetObject()             sets object url and mime-type
// -----------
// Input: pszUrl = [in] url without host name/address where clients can download an object
//                      NULL to delete the object url/mime type
//        pszMimeType = [in] MIME type of object
// Return: 

void CVarRef::CMeta::SetObject (const char *pszUrl, const char *pszMimeType)
  {
  if(pszUrl)
    {
    assert(pszMimeType); // if url specified, the mime type is mandatory
    m_dwValidMemberFlags|=(VARMETA_OBJ_URL|VARMETA_OBJ_MIME);
    m_strObjUrl=pszUrl;
    m_strObjMime=pszMimeType;
    }
  else // shall clear
    {
    assert(pszMimeType==NULL); // when clearing the object url, why there is a mime type??
    m_dwValidMemberFlags&=~(VARMETA_OBJ_URL|VARMETA_OBJ_MIME);
    m_strObjUrl.clear();
    m_strObjMime.clear();
    }
  }


//--------------------------------------------------------------------------
// FormatAsJson()          serializes meta data to JSON stream object
// --------------
// Input: joDump = [in] object where to place the meta data to
// Return: 

void CVarRef::CMeta::FormatAsJson (CJsonStreamObject &joDump) const
  {
  struct
    {
    const DWORD dwMask;
    const char *pszFieldName;
    const std::string *pString;
    const double *pDouble;
    const double *pDblArray;
    const int nArrayCount;
    const int *pInt;
    } const seritable[]=
  {  // dwMask             pszFieldNamepString                 pString          pDouble         pDblArray      nArrayCount             pInt
    { VARMETA_DISPNAME   , ABK_RSP_VARMETA_DISPNAME          , &m_strDispName,  NULL,           NULL,          0,                      NULL,             },
    { VARMETA_COMMENT    , ABK_RSP_VARMETA_COMMENT           , &m_strComment,   NULL,           NULL,          0,                      NULL,             },
    { VARMETA_UNIT       , ABK_RSP_VARMETA_UNIT              , &m_strUnit,      NULL,           NULL,          0,                      NULL,             },
    { VARMETA_SYMBOL     , ABK_RSP_VARMETA_SYMBOL            , &m_strSymbol,    NULL,           NULL,          0,                      NULL,             },
    { VARMETA_TAGS       , ABK_RSP_VARMETA_TAGS              , &m_strTags,      NULL,           NULL,          0,                      NULL,             },
    { VARMETA_RANGE      , ABK_RSP_VARMETA_RANGEMIN          , NULL,            &m_dRangeMin,   NULL,          0,                      NULL,             },
    { VARMETA_RANGE      , ABK_RSP_VARMETA_RANGEMAX          , NULL,            &m_dRangeMax,   NULL,          0,                      NULL,             },
    { VARMETA_FACTOFFS   , ABK_RSP_VARMETA_FACTOR            , NULL,            &m_dFactor,     NULL,          0,                      NULL,             },
    { VARMETA_FACTOFFS   , ABK_RSP_VARMETA_OFFSET            , NULL,            &m_dOffset,     NULL,          0,                      NULL,             },
    { VARMETA_THRESHOLDS , ABK_RSP_VARMETA_THRESHOLDS        , NULL,            NULL,           m_dThresholds, _countof(m_dThresholds),NULL,             },
    { VARMETA_FRACTDIGITS, ABK_RSP_VARMETA_FRACTDIGITS       , NULL,            NULL,           NULL,          0,                      &m_nFractDigits   },
    { VARMETA_OBJ_URL    , ABK_RSP_VARMETA_OBJ_URL           , &m_strObjUrl,    NULL,           NULL,          0,                      NULL              },
    { VARMETA_OBJ_MIME   , ABK_RSP_VARMETA_OBJ_MIME          , &m_strObjMime,   NULL,           NULL,          0,                      NULL              },
  };
  
  // serialize the meta data
  for(int nMember=0;nMember<_countof(seritable);++nMember)
    {
    if(seritable[nMember].dwMask & m_dwValidMemberFlags) // if the appropriate member is valid, serialize it. Otherwise do not place it into the JSON metadata
      {
      if(seritable[nMember].pString)
        {
        joDump.WriteValue(seritable[nMember].pszFieldName,*seritable[nMember].pString);
        }
      else if(seritable[nMember].pDouble)
        {
        joDump.WriteValue(seritable[nMember].pszFieldName,*seritable[nMember].pDouble);
        }
      else if(seritable[nMember].pDblArray)
        {
        const int nArrayCount=seritable[nMember].nArrayCount;
        const char *pszArrayName=seritable[nMember].pszFieldName;
        CJsonStreamArray jaArray(&joDump,pszArrayName);
        for(int nArrayMember=0;nArrayMember<nArrayCount;++nArrayMember)
          jaArray.WriteValue(seritable[nMember].pDblArray[nArrayMember]);
        }
      else if(seritable[nMember].pInt)
        {
        joDump.WriteValue(seritable[nMember].pszFieldName,*seritable[nMember].pInt);
        }
      }
    }
  }


















//--------------------------------------------------------------------------
// CVarRef()               Constructor of CVarRef
// ---------
// Input: -
// Return: 

//CVarRef::CVarRef (void)
//  {
//  m_dRangeMin=0.; // expected range of value
//  m_dRangeMax=100.; 
//  m_dFactor=1.; // physical value = intval * factor + offset
//  m_dOffset=0.;
//  }


//--------------------------------------------------------------------------
// CVarRef()               constructor
// ---------
// Input: pszName = name of variable
// Return: 

CVarRef::CVarRef (const char *pszName)
  {
  m_strName=pszName;
  // when adding new members, dont forget to
  // a) extend the copy operator

  // plausibility checks
  assert(ABK_VALUE_THRESHOLD_COUNT==SEVERITY_THRESHOLD_COUNT); // the enum differs from the specification
  }


//--------------------------------------------------------------------------
// ~CVarRef()              Destructor of CVarRef
// ----------
// Input: -
// Return: 


/*virtual*/ CVarRef::~CVarRef ()
  {
  Lock();
  m_lstTrends.clear();
  Unlock();
  }



//--------------------------------------------------------------------------
// GetName()               returns name of the variable
// ---------
// Input: -
// Return: 

const char *CVarRef::GetName (void) const
  {
  return m_strName.c_str();
  }




//--------------------------------------------------------------------------
// GetNameString()         returns reference to name string
// ---------------
// Input: -
// Return: 

const std::string &CVarRef::GetNameString (void) const
  {
  return m_strName;
  }


//--------------------------------------------------------------------------
// Lock()                  locks the object
// ------
// Input: nTimeoutMs = timeout in ms
// Return: true on success, false on timeout

bool CVarRef::Lock (int nTimeoutMs/*=ABK_VAR_TIMEOUT*/)
  {
  return m_mutex.Lock(nTimeoutMs);
  }




//--------------------------------------------------------------------------
// Unlock()                unlocks the object
// --------
// Input: -
// Return: 

void CVarRef::Unlock (void)
  {
  m_mutex.Unlock();
  }



//--------------------------------------------------------------------------
// operator=()             copy operator
// -----------
// Input: rOther = copy source
// Return: reference to this object

CVarRef &CVarRef::operator = (CVarRef &rOther)
  {
  m_strName=rOther.m_strName;
  m_lstTrends=rOther.m_lstTrends;
  return *this;
  }



//--------------------------------------------------------------------------
// FormatMetaAsJson()      formats meta data to JSON string
// ------------------
// Input: joDump = formatting object to place the data into
// Return: -

void CVarRef::FormatMetaAsJson (CJsonStreamObject &joDump) const
  {
  CMeta metaQuery; // virtual override shall put its metadata here
  OnGetMeta(metaQuery); // get the meta data from the logger
  joDump.WriteValue(ABK_RSP_VARMETA_NAME,m_strName); // write the name
  metaQuery.FormatAsJson(joDump); // write all other meta data
  }





//--------------------------------------------------------------------------
// AddTrend()              adds a trend to be maintained on every logger cycle or data change
// ----------
// Input: pAdd = 
// Return: 

bool CVarRef::AddTrend (CDaqTrend *pAdd)
  {
  CAbkSingleLock guard(&m_mutex,true,ABK_VAR_TIMEOUT);
  std::list<CDaqTrend *>::iterator iterCheck; // check for duplicates
  for(iterCheck=m_lstTrends.begin();iterCheck!=m_lstTrends.end();++iterCheck)
    {
    CDaqTrend *pExisting=*iterCheck;
    if(pExisting==pAdd)
      {
      assert(false); // you tried to add the same daq trend twice!
      return false;
      }
    }
  m_lstTrends.push_back(pAdd);
  OnTrendDependenciesChanged(m_lstTrends.size());
  return true;
  }


//--------------------------------------------------------------------------
// RemoveTrend()           removes a trend from maintainence list
// -------------
// Input: pRemove = 
// Return: 

bool CVarRef::RemoveTrend (CDaqTrend *pRemove)
  {
  CAbkSingleLock guard(&m_mutex,true,ABK_VAR_TIMEOUT);
  // consider to use list::remove instead. disadvantage: provides no error info if element not found
  std::list<CDaqTrend *>::iterator iterSearch; // check for duplicates
  for(iterSearch=m_lstTrends.begin();iterSearch!=m_lstTrends.end();++iterSearch)
    {
    CDaqTrend *pExisting=*iterSearch;
    if(pExisting==pRemove)
      {
      m_lstTrends.erase(iterSearch);
      OnTrendDependenciesChanged(m_lstTrends.size());
      return true;
      }
    }
  return false;
  }



//--------------------------------------------------------------------------
// FeedTrend()              feeds all dependent daq trend with actual value
// ----------
// Input: -
// Return: 

void CVarRef::FeedTrend (void)
  {
  Lock();
  std::list<CDaqTrend *>::iterator iterFeed; // check for duplicates
  for(iterFeed=m_lstTrends.begin();iterFeed!=m_lstTrends.end();++iterFeed)
    {
    CDaqTrend *pTrend=*iterFeed;
    assert(pTrend);
    pTrend->Lock(); // lock the trend
    CJsonStreamArray *pQueue=pTrend->GetQueue();
    pQueue->BeginMember();
    OnFormatValue(*pQueue);
    pTrend->Unlock();
    }
  Unlock();
  }


//--------------------------------------------------------------------------
// OnTrendDependenciesChanged() called when daq trend was added/removed
// ----------------------------
// Input: nDependants = number of daq trends dependend on this variable
//                      if 0, you can stop to call FeedTrend()
//                      if >0, you have to call FeedTrend() whenever
//                             a cycle in the logger expires
// Return: -

/*virtual*/ void CVarRef::OnTrendDependenciesChanged (int nDependants) const
  {
  ;  
  }







} // namespace

