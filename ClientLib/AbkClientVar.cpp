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
// Filename:    AbkClientVar.cpp
// Created:     2012-07-12 (18:36)
// Author:      D. Burger
// Description: Variable linking the client variables with the ABK http interface
//------------------------------------------------------------------------------------------------




#include "stdafx.h"


#include "AbkClientVar.h"
#include <limits>
#include <assert.h>

#define DEFAULT_FRACTIONAL_DIGITS 2 // number of fractional digits when abk server does not specify em

namespace Abk
  {


//--------------------------------------------------------------------------
// CAbkClientMeta()        Constructor of CAbkClientMeta
// ----------------
// Input: -
// Return: 

CAbkClientMeta::CAbkClientMeta ()
  {
  m_dRangeMin=0.; 
  m_dRangeMax=100.; 
  m_dFactor=1.;
  m_dOffset=0.;
  m_nFractionalDigits=DEFAULT_FRACTIONAL_DIGITS;

  // init thresholds
  //int nThreshold;
  //for(nThreshold=0;nThreshold<ABK_VALUE_THRESHOLD_COUNT;nThreshold++)
  //  m_dThresholds[nThreshold]=std::numeric_limits<double>::quiet_NaN();
  assert(ABK_VALUE_THRESHOLD_COUNT==5); // the following code expects this number of thresholds
  m_dThresholds[0]=-DBL_MAX; // std::numeric_limits<double>::lowest();
  m_dThresholds[1]=-DBL_MAX; // std::numeric_limits<double>::lowest();
  m_dThresholds[2]=0.;
  m_dThresholds[3]=DBL_MAX; // std::numeric_limits<double>::max();
  m_dThresholds[4]=DBL_MAX; // std::numeric_limits<double>::max();

  m_bHasThresholds=FALSE;
  m_bIsMailbox=FALSE;
  }


//--------------------------------------------------------------------------
// IsNeutralFactorAndOffset() returns TRUE if factor==1 and offset==0
// --------------------------
// Input: -
// Return: 

bool CAbkClientMeta::IsNeutralFactorAndOffset (void) const
  {
  if((m_dFactor==1.)&&(m_dOffset==0.))
    return TRUE;
  return FALSE;
  }

//--------------------------------------------------------------------------
// HasThresholds()         returns true if thresholds are defined
// ---------------
// Input: -
// Return: 

bool CAbkClientMeta::HasThresholds (void) const
  {
  return m_bHasThresholds;
  }



//--------------------------------------------------------------------------
// ExtractFromJson()       extracts metadata of a variable
// -----------------
// Input: jpMeta = json parser containing meta data. state of parser is
//                 just at the beginning of the array with meta data
// Return: TRUE on success, FALSE on error

bool CAbkClientMeta::ExtractFromJson (CJsonParser &jpMeta)
  {
  bool bExtracted=false;
  std::string strName;
  std::string strDispName;
  std::string strComment;
  std::string strUnit;
  std::string strSymbol;
  std::string strTags;
  std::string strObjUrl;
  std::string strObjMime;
  for(++jpMeta;!jpMeta.IsDone();++jpMeta)
    {
    bExtracted|=jpMeta.ExtractValue(ABK_RSP_VARMETA_NAME,&strName);
    bExtracted|=jpMeta.ExtractValue(ABK_RSP_VARMETA_DISPNAME,&strDispName);
    bExtracted|=jpMeta.ExtractValue(ABK_RSP_VARMETA_COMMENT,&strComment);
    bExtracted|=jpMeta.ExtractValue(ABK_RSP_VARMETA_UNIT,&strUnit);
    bExtracted|=jpMeta.ExtractValue(ABK_RSP_VARMETA_SYMBOL,&strSymbol);
    bExtracted|=jpMeta.ExtractValue(ABK_RSP_VARMETA_TAGS,&strTags);
    bExtracted|=jpMeta.ExtractValue(ABK_RSP_VARMETA_RANGEMIN,&m_dRangeMin);
    bExtracted|=jpMeta.ExtractValue(ABK_RSP_VARMETA_RANGEMAX,&m_dRangeMax);
    bExtracted|=jpMeta.ExtractValue(ABK_RSP_VARMETA_FACTOR,&m_dFactor);
    bExtracted|=jpMeta.ExtractValue(ABK_RSP_VARMETA_OFFSET,&m_dOffset);
    bExtracted|=jpMeta.ExtractValue(ABK_RSP_VARMETA_FRACTDIGITS,&m_nFractionalDigits);
    bExtracted|=jpMeta.ExtractValue(ABK_RSP_VARMETA_OBJ_URL,&strObjUrl);
    bExtracted|=jpMeta.ExtractValue(ABK_RSP_VARMETA_OBJ_MIME,&strObjMime);
    if(jpMeta.TestArray(ABK_RSP_VARMETA_THRESHOLDS)) // is there "Thresholds": [
      {
      int nThreshold=0;
      for(++jpMeta;!jpMeta.IsDone();++jpMeta)
        {
        if(nThreshold<ABK_VALUE_THRESHOLD_COUNT)
          {
          double dThreshold;
          if(jpMeta.GetValueDouble(dThreshold))
            m_dThresholds[nThreshold]=dThreshold;
          }
        nThreshold++;
        }
      if(nThreshold>=ABK_VALUE_THRESHOLD_COUNT)
        {
        m_bHasThresholds=TRUE;
        // check validity of thresholds
        for(nThreshold=0;nThreshold<(ABK_VALUE_THRESHOLD_COUNT-1);nThreshold++)
          {
          if(m_dThresholds[nThreshold]>m_dThresholds[nThreshold+1])
            {
            m_bHasThresholds=FALSE; // if lower threshold is higher then the higer threshold: set as no-threshold
            break;
            }
          }
        if(m_dThresholds[0]==m_dThresholds[ABK_VALUE_THRESHOLD_COUNT-1]) // if all thresholds have the same value
          m_bHasThresholds=FALSE; // .. mark as no-thresholds
        }
      }
    if(jpMeta.IsError())
      return FALSE;
    jpMeta.SkipItem(); // skip any non-known items
    }
  m_strName    =CA2T(strName.c_str(),CP_UTF8);
  m_strDispName=CA2T(strDispName.c_str(),CP_UTF8);
  m_strComment =CA2T(strComment.c_str(),CP_UTF8);
  m_strUnit    =CA2T(strUnit.c_str(),CP_UTF8);
  m_strSymbol  =CA2T(strSymbol.c_str(),CP_UTF8);
  m_strTags    =CA2T(strTags.c_str(),CP_UTF8);
  m_strObjUrl  =CA2T(strObjUrl.c_str(),CP_UTF8);
  m_strObjMime=CA2T(strObjMime.c_str(),CP_UTF8);
  if(m_strDispName.IsEmpty()) // use name for the display name, if it was not provided or invalidly provided as empty string
    m_strDispName=m_strName;
  if((!bExtracted)||(jpMeta.IsError()))
    return FALSE;
  ApplyFactorAndOffset(); // apply factor and offset to the scalar members
  return TRUE;
  }


//--------------------------------------------------------------------------
// ApplyFactorAndOffset()  applies factor and offset to a double location
// ----------------------
// Input: pValue = pointer to modify the data
// Return: -

void CAbkClientMeta::ApplyFactorAndOffset (double *pValue)
  {
  *pValue=*pValue *m_dFactor+m_dOffset;
  }


//--------------------------------------------------------------------------
// ApplyFactorAndOffset()  applies factor and offset to all scalar meta data
// ----------------------
// Input: -
// Return: -

void CAbkClientMeta::ApplyFactorAndOffset (void)
  {
  if(IsNeutralFactorAndOffset())
    return; // do nothing
  ApplyFactorAndOffset(&m_dRangeMin);
  ApplyFactorAndOffset(&m_dRangeMax);
  int nThreshold;
  for(nThreshold=0;nThreshold<ABK_VALUE_THRESHOLD_COUNT;nThreshold++)
    ApplyFactorAndOffset(&m_dThresholds[nThreshold]);
  }

















//--------------------------------------------------------------------------
// CAbkClientVar()         Constructor of CAbkClientVar
// ---------------
// Input: pDaq = daq the variable belongs to
// Return: 

CAbkClientVar::CAbkClientVar (CAbkClientDaq *pDaq)
  {
  m_pDaq=pDaq;
  }


//--------------------------------------------------------------------------
// ~CAbkClientVar()        Destructor of CAbkClientVar
// ----------------
// Input: -
// Return: 

/*virtual*/ CAbkClientVar::~CAbkClientVar ()
  {
  }



}// namespace





