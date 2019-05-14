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
// Filename:    AbkClientVar.h
// Created:     2012-07-12 (18:37)
// Author:      D. Burger
// Description: Variable linking the client variables with the ABK http interface
//------------------------------------------------------------------------------------------------




#pragma once

#include "ValuesFromSpec.h"
#include "JsonParserAtl.h"
#include <list>

class CJsonParser;

namespace Abk
  {
  class CAbkClientDaq;

  // meta data for a variable/mailbox
  class CAbkClientMeta
    {
    public:
      class CEntity {CString m_strName; CComVariant m_varData;};  // a non-hardcoded meta information

    // data members
    public:
      CString m_strName; // name
      CString m_strDispName; // name used for user interface
      CString m_strComment; // comment
      CString m_strUnit; // unit string
      CString m_strSymbol; // symbolic name
      CString m_strTags; // tags for filtering
      double m_dRangeMin; // lower bound of range, already calculated with factor and offset
      double m_dRangeMax; // upper bound of range, already calculated with factor and offset
      double m_dFactor; // factor
      double m_dOffset; // offset
      int m_nFractionalDigits; // number of fractional digits
      double m_dThresholds[ABK_VALUE_THRESHOLD_COUNT]; // semantic thresholds, already calculated with factor and offset
      std::list<CEntity> m_lstEntities; // meta data not hard-coded
      bool m_bHasThresholds;
      bool m_bIsMailbox; // true for mailbox
      CString m_strObjUrl; // not empty if variable has an url where to download an object, e.g. an image. Url without host name/address
      CString m_strObjMime; // mime-type of the data on the object URL, e.g. image/jpeg

    // construction/destruction/setup
    public:
      CAbkClientMeta ();
      virtual ~CAbkClientMeta () {;}

    // member access
    public:
      const CString &GetName     (void) const {return m_strName;}
      const CString &GetDispName (void) const {return m_strDispName;}
      const CString &GetComment  (void) const {return m_strComment;}
      const CString &GetUnit     (void) const {return m_strUnit;}
      const CString &GetSymbol   (void) const {return m_strSymbol;}
      const CString &GetTags     (void) const {return m_strTags;}
      double GetRangeMin (void) const {return m_dRangeMin;}
      double GetRangeMax (void) const {return m_dRangeMax;}
      double GetFactor (void) const {return m_dFactor;}
      double GetOffset (void) const {return m_dOffset;}
      int GetFractDigits (void) const {return m_nFractionalDigits;}
      const CString &GetObjUrl (void) const {return m_strObjUrl;}
      const CString &GetObjMime (void) const {return m_strObjMime;}

    // methods and properties
    public:
      bool IsNeutralFactorAndOffset (void) const; // returns TRUE if factor==1 and offset==0
      bool HasThresholds (void) const; // returns true if thresholds are defined
      bool ExtractFromJson (CJsonParser &jpMeta); // extracts meta data from JSON parser
      bool IsObj (void) const {return !m_strObjUrl.IsEmpty();}

    // implementation
    protected:
      void ApplyFactorAndOffset (double *pValue); // applies factor and offset to a double location
      void ApplyFactorAndOffset (void); // applies factor and offset to all scalar meta data
    };




  class CAbkClientVar
    {
    // data members
    protected:
      CAbkClientDaq *m_pDaq; // owning daq

    // construction/destruction/setup
    public:
      CAbkClientVar (CAbkClientDaq *pDaq);
      virtual ~CAbkClientVar ();

    // methods and properties
    public:

    // overrideables
    public:
      virtual LPCTSTR GetName (void) const=0; // returns the name of the variable
      virtual void OnValueFromServer (const CJsonParserAtl *pSource)=0; // called when data arrived from server
      virtual void OnMinAvgMaxFromServer (CJsonParserAtl *pSource)=0; // called when min, avg, max arrived from server
    
    // implementation
    protected:

    };

  } // namespace
