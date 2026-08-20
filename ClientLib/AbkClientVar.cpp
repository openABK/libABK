// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------------------------
//  _____  __   __  _____  _    _        ____              
// |  ___||  \ /  ||  _  \| |  | |      / ___|  _   _  ___ 
// |  __| |   V   ||  -  /| |  | |  __  \___ \ | | | |/ __|
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

#if defined (WINCE)
#include <stdlib.h>
#define nextafter _nextafter
#else
#include <math.h>
#endif


#define DEFAULT_FRACTIONAL_DIGITS 2 // number of fractional digits when abk server does not specify em

namespace Abk
{



  /** Constructor of value to text table entity
  */
  CAbkClientMeta::VALUE_TO_TEXT::VALUE_TO_TEXT ()
  {
    dRangeLower = 0.;
    dRangeUpper = 0.;
    bTextValid = false;
  }




  /** extracts table entity from JSON parser
  @param jpSource JSON parser containing a table entity.
    The state of parser is just at the beginning of the entity object
  @return true on success, false on error
  */
  bool CAbkClientMeta::VALUE_TO_TEXT::ExtractFromJson (CJsonParser& jpSource)
  {
    bool bSuccess = false;
    bool bRangeLowerSuccess = false;
    double dRangeLowerTemp = 0.;
    bool bRangeUpperSuccess = false;
    double dRangeUpperTemp = 0.;
    bool bTextValid = false;
    std::string strTextTemp;
    for (++jpSource; !jpSource.IsDone (); ++jpSource)
    {
      bRangeLowerSuccess |= jpSource.ExtractValue (ABK_RSP_VALTBL_VALUE, &dRangeLowerTemp);
#if defined (ABK_RSP_VALTBL_VALUE_TO)
      bRangeUpperSuccess |= jpSource.ExtractValue (ABK_RSP_VALTBL_VALUE_TO, &dRangeUpperTemp);
#endif
      bTextValid |= jpSource.ExtractValue (ABK_RSP_VALTBL_TEXT, &strTextTemp);
      if (bRangeLowerSuccess && bTextValid)
      {
        dRangeLower = dRangeLowerTemp;
        dRangeUpper = nextafter (dRangeLower, dRangeLower + 1); // since we want to get a range, we build the smallest range from dRangeLower
        strText = CA2T (strTextTemp.c_str (), CP_UTF8);
        bSuccess = true;
      }
      jpSource.SkipItem (); // skip any non-known items
    }
    if (bRangeUpperSuccess)
      dRangeUpper = dRangeUpperTemp;
    return bSuccess;
  }












  /** constructor of lookup-table */
  CAbkClientMeta::CValueTable::CValueTable ()
  {
  }




  /** Returns number of table entities
  @return Number of table entities
  */
  size_t CAbkClientMeta::CValueTable::GetCount (void) const
  {
    return m_vectEntities.size ();
  }




  /** Returns entity of a certain index or the default entity  
  @param nIndex zero-based index of entity to be retrieved. Any invalid index returns the fall-back entity
  @return entity of specified index or the fall-back entity.
  */
  const CAbkClientMeta::VALUE_TO_TEXT& CAbkClientMeta::CValueTable::operator[](int nIndex) const
  {
    return CheckIndex (nIndex) ? m_vectEntities[nIndex] : m_entFallback;
  }

  CAbkClientMeta::VALUE_TO_TEXT& CAbkClientMeta::CValueTable::operator[](int nIndex)
  {
    return CheckIndex (nIndex) ? m_vectEntities[nIndex] : m_entFallback;
  }




  /** checks whether an index is a valid table index  
  @param nIndex Index to be tested
  @return true if the index addresses an item in the table. false if out-of-range
  */
  bool CAbkClientMeta::CValueTable::CheckIndex (int nIndex) const
  {
    return (nIndex >= 0) && (nIndex < (int)m_vectEntities.size ());
  }




  /** adds an item to the tail
  @param entAdd reference to entity to be added
  */
  void CAbkClientMeta::CValueTable::Add (const VALUE_TO_TEXT& entAdd)
  {
    m_vectEntities.push_back (entAdd);
  }




  /** sets the fall-back entity  
  @param entFallback Reference to an entity to be set as default. The range information will be ignored
  */
  void CAbkClientMeta::CValueTable::SetFallback (const VALUE_TO_TEXT& entFallback)
  {
    m_entFallback = entFallback;
    m_entFallback.dRangeLower = 0.;
    m_entFallback.dRangeUpper = 0.;
  }




  /** extracts table from JSON parser
  @param jpSource JSON parser containing the table.
    The state of parser is just at the beginning of the entity array
  @return true on success, false on error
  */
  bool CAbkClientMeta::CValueTable::ExtractFromJson (CJsonParser& jpSource)
  {
    bool bSuccess = false;
    if (!jpSource.IsObject ())
    {
      bSuccess = true;
      for (++jpSource; !jpSource.IsDone (); ++jpSource)
      {
        VALUE_TO_TEXT entNew;
        bSuccess &= entNew.ExtractFromJson (jpSource);
        if (bSuccess)
          m_vectEntities.push_back (entNew);
        jpSource.SkipItem (); // skip any non-known items
      }
    }
    return bSuccess;
  }









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
  m_dThresholds[0]=std::numeric_limits<double>::lowest();
  m_dThresholds[1]=std::numeric_limits<double>::lowest();
  m_dThresholds[2]=0.;
  m_dThresholds[3]=std::numeric_limits<double>::max();
  m_dThresholds[4]=std::numeric_limits<double>::max();

  m_bHasThresholds=FALSE;
  m_bIsMailbox=FALSE;
  }




/** returns a reference to the value table
@return A reference to the value table
*/
const CAbkClientMeta::CValueTable& CAbkClientMeta::GetValueTable (void) const
{
  return m_tblValToText;
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

bool CAbkClientMeta::ExtractFromJson (CJsonParser& jpMeta)
{
  bool bExtracted = false;
  bool bTextFallbackExtracted = false;
  std::string strName;
  std::string strDispName;
  std::string strComment;
  std::string strUnit;
  std::string strSymbol;
  std::string strTags;
  std::string strObjUrl;
  std::string strObjMime;
  std::string strTextFallback;
  for (++jpMeta; !jpMeta.IsDone (); ++jpMeta)
  {
    bExtracted |= jpMeta.ExtractValue (ABK_RSP_VARMETA_NAME, &strName);
    bExtracted |= jpMeta.ExtractValue (ABK_RSP_VARMETA_DISPNAME, &strDispName);
    bExtracted |= jpMeta.ExtractValue (ABK_RSP_VARMETA_COMMENT, &strComment);
    bExtracted |= jpMeta.ExtractValue (ABK_RSP_VARMETA_UNIT, &strUnit);
    bExtracted |= jpMeta.ExtractValue (ABK_RSP_VARMETA_SYMBOL, &strSymbol);
    bExtracted |= jpMeta.ExtractValue (ABK_RSP_VARMETA_TAGS, &strTags);
    bExtracted |= jpMeta.ExtractValue (ABK_RSP_VARMETA_RANGEMIN, &m_dRangeMin);
    bExtracted |= jpMeta.ExtractValue (ABK_RSP_VARMETA_RANGEMAX, &m_dRangeMax);
    bExtracted |= jpMeta.ExtractValue (ABK_RSP_VARMETA_FACTOR, &m_dFactor);
    bExtracted |= jpMeta.ExtractValue (ABK_RSP_VARMETA_OFFSET, &m_dOffset);
    bExtracted |= jpMeta.ExtractValue (ABK_RSP_VARMETA_FRACTDIGITS, &m_nFractionalDigits);
    bExtracted |= jpMeta.ExtractValue (ABK_RSP_VARMETA_OBJ_URL, &strObjUrl);
    bExtracted |= jpMeta.ExtractValue (ABK_RSP_VARMETA_OBJ_MIME, &strObjMime);
    bTextFallbackExtracted |= jpMeta.ExtractValue (ABK_RSP_VARMETA_TEXTFALLBACK, &strTextFallback);
    
    if (jpMeta.TestArray (ABK_RSP_VARMETA_THRESHOLDS)) // is there "Thresholds": [
    {
      int nThreshold = 0;
      for (++jpMeta; !jpMeta.IsDone (); ++jpMeta)
      {
        if (nThreshold < ABK_VALUE_THRESHOLD_COUNT)
        {
          double dThreshold;
          if (jpMeta.GetValueDouble (dThreshold))
            m_dThresholds[nThreshold] = dThreshold;
        }
        nThreshold++;
      }
      if (nThreshold >= ABK_VALUE_THRESHOLD_COUNT)
      {
        m_bHasThresholds = TRUE;
        // check validity of thresholds
        for (nThreshold = 0; nThreshold < (ABK_VALUE_THRESHOLD_COUNT - 1); nThreshold++)
        {
          if (m_dThresholds[nThreshold] > m_dThresholds[nThreshold + 1])
          {
            m_bHasThresholds = FALSE; // if lower threshold is higher then the higer threshold: set as no-threshold
            break;
          }
        }
        if (m_dThresholds[0] == m_dThresholds[ABK_VALUE_THRESHOLD_COUNT - 1]) // if all thresholds have the same value
          m_bHasThresholds = FALSE; // .. mark as no-thresholds
      }
    }
    
    if (jpMeta.TestArray (ABK_RSP_VARMETA_TEXT)) // is there "Text": [
    {
      m_tblValToText.ExtractFromJson (jpMeta);
    }
    
    if (jpMeta.IsError ())
      return FALSE;
    jpMeta.SkipItem (); // skip any non-known items
  }
  m_strName = CA2T (strName.c_str (), CP_UTF8);
  m_strDispName = CA2T (strDispName.c_str (), CP_UTF8);
  m_strComment = CA2T (strComment.c_str (), CP_UTF8);
  m_strUnit = CA2T (strUnit.c_str (), CP_UTF8);
  m_strSymbol = CA2T (strSymbol.c_str (), CP_UTF8);
  m_strTags = CA2T (strTags.c_str (), CP_UTF8);
  m_strObjUrl = CA2T (strObjUrl.c_str (), CP_UTF8);
  m_strObjMime = CA2T (strObjMime.c_str (), CP_UTF8);
  if (bTextFallbackExtracted)
  {
    m_tblValToText[-1].strText = CA2T(strTextFallback.c_str(), CP_UTF8);
    m_tblValToText[-1].bTextValid = true;
  }
  if (m_strDispName.IsEmpty ()) // use name for the display name, if it was not provided or invalidly provided as empty string
    m_strDispName = m_strName;
  if ((!bExtracted) || (jpMeta.IsError ()))
    return FALSE;
  ApplyFactorAndOffset (); // apply factor and offset to the scalar members
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


















}// namespace





