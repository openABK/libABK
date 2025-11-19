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
// Filename:    AbkVarRef.h
// Created:     2012-05-07 (09:22)
// Author:      D. Burger
// Description: Variable reference
//------------------------------------------------------------------------------------------------

#pragma once

#include <iostream>
#include <list>
#include <string.h>
#include <sstream>
#include <vector>
#include "CrossPlatform.h"
#include "JsonFormatter.h"


#define ABK_VAR_TIMEOUT 1000

namespace Abk
{

  class CDaqTrend;

  class CVarRef
  {
    friend class CDaqTrend;

  public:
    // thresholds for visualizing the value e.g. colorized
    enum SEVERITY_THRESHOLD
    {
      LOW_CRITICAL,             // lower critical limit, e.g. -40°C
      LOW_MARGINAL,             // lower margin, e.g. -20 °C
      CENTER,                   // center, typically splitting positive from negative, e.g. 0°C
      HIGH_MARGINAL,            // higher margin, e.g. +70°C
      HIGH_CRITICAL,            // higher critial limit, e.g. +90°C
      SEVERITY_THRESHOLD_COUNT  // number of thresholds
    };

    class CMeta // meta data of variable/mailbox. generally used to query via override and then format it to the client
    {
      enum // each defines a flag indicating that the meta data is valid and shall be serialized to JSON
      {
        VARMETA_DISPNAME = 0x0001,
        VARMETA_COMMENT = 0x0002,
        VARMETA_UNIT = 0x0004,
        VARMETA_SYMBOL = 0x0008,
        VARMETA_TAGS = 0x0010,
        VARMETA_RANGE = 0x0020,
        VARMETA_FACTOFFS = 0x0040,
        VARMETA_THRESHOLDS = 0x0080,
        VARMETA_FRACTDIGITS = 0x0100,
        VARMETA_OBJ_URL = 0x0200, // the variable has an url where clients can download the object
        VARMETA_OBJ_MIME = 0x0400, // MIME type of the object
        VARMETA_VALUETABLE = 0x0800,
      };

    public:
      struct VALUE_TO_TEXT ///< table entity for translating a value into a text
      {
        std::string strText; // text output if this entity matches a received value
        bool bTextValid; // true, if the text is valid. Used to qualify the fall-back item
        double dRangeLower; // lower end of range, included
        double dRangeUpper; // upper end of range, non-included
        VALUE_TO_TEXT ();
        void FormatAsJson (CJsonStreamObject& joDump) const; // formats to JSON string
      };

    public:
      class CValueTable ///< a lookup table for converting a value into text
      {
      protected:
        std::vector<VALUE_TO_TEXT> m_vectEntities; // all entities in the order as they came from the server
        VALUE_TO_TEXT m_entFallback; // to be applied if no entity matches
      public:
        CValueTable ();
        size_t GetCount (void) const;
        const VALUE_TO_TEXT& operator[](int nIndex) const;
        VALUE_TO_TEXT& operator[](int nIndex);
        bool CheckIndex (int nIndex) const; // checks whether an index is a valid table index
        void Add (const VALUE_TO_TEXT& entAdd); // adds an item to the tail
        void Add (double dRangeLower, double dRangeUpper, const char *pszText); // adds an item to the tail
        void Add (double dValue, const char *pszText); // adds an item to the tail
        void SetFallback (const VALUE_TO_TEXT& entFallback); // sets the fall-back entity
        void SetFallback (const char *pszFallbackText); // sets the fall-back text
        void FormatAsJson (CJsonStreamObject& joDump, const char *pszName) const; // formats to JSON string
      };

    public:
      DWORD m_dwValidMemberFlags;   // each member gets a valid flag. a valid member will be serialized to the metadata json
      std::string m_strDispName;    // name how to display the variable
      std::string m_strComment;     // comment to variable
      std::string m_strUnit;        // unit string
      std::string m_strSymbol;      // short symbol of variable
      std::string m_strTags;        // tags provinding information to semantic of variable
      double m_dRangeMin;           // expected range of value
      double m_dRangeMax;
      double m_dFactor;             // physical value = intval * factor + offset
      double m_dOffset;
      double m_dThresholds[SEVERITY_THRESHOLD_COUNT]; // thresholds defining severity ranges
      int m_nFractDigits; // number of suggested fractional digits
      std::string m_strObjUrl;      // if not empty, the variable provides an client-downloadable object, e.g. an image
      std::string m_strObjMime;     // mime type of the object
      CValueTable m_tblValToText; // value-to-text lookup table

    public:
      CMeta (); // constructor, initialized to defaults
    public:
      void SetDispName (const char* pszDispName);
      void SetComment (const char* pszComment);
      void SetUnit (const char* pszUnit);
      void SetSymbol (const char* pszSymbol);
      void SetTags (const char* pszTags);
      void SetRange (double dMin, double dMax);
      void SetFactorOffset (double dFactor, double dOffset);
      void SetThresholds (const double dThresholds[]);
      void SetFractDigits (int nFractDigits);
      void SetObject (const char* pszUrl, const char* pszMimeType); // sets object url and mime-type
      void SetValueTable (const CValueTable& rTable); // sets the value table
    public:
      void FormatAsJson (CJsonStreamObject& joDump) const; // serializes meta data to JSON stream object
    };

    // data members
  protected:
    std::string m_strName;    // name of variable
    std::list<CDaqTrend*> m_lstTrends; // list of trends to be serviced on every logger data cycle
    CAbkMutex m_mutex;        // mutex protecting the daq trend list

    // construction/destruction
  public:
    CVarRef (const char* pszName);
    virtual ~CVarRef ();

    // member access
  public:
    const char* GetName (void) const; // returns name of the variable
    const std::string& GetNameString (void) const; // returns reference to name string

  // methods and attributes
  public:
    void FormatMetaAsJson (CJsonStreamObject& joDump) const; // formats meta data to JSON string
    void FeedTrend (void); // feeds all dependent daq trend with actual value

  // implementation
  private:
    bool AddTrend (CDaqTrend* pAdd); // adds a trend to be maintained on every logger cycle or data change
    bool RemoveTrend (CDaqTrend* pRemove); // removes a trend from maintainence list
    bool Lock (int nTimeoutMs = ABK_VAR_TIMEOUT); // locks the object
    void Unlock (void); // unlocks the object

  // overridables
  public:
    virtual void OnGetMeta (CMeta& rMetaData) const = 0; // called when variables meta data are needed by the interface
    virtual void OnFormatValue (CJsonStreamBase& rFormatterOut) const = 0; // called when a variable shall be formatted to an array
    virtual void OnFormatMAM (CJsonStreamArray& rFormatterOut) const = 0; // called when a variable min-max-average shall be formatted
    virtual void OnSetValue (const std::string& strSet) const = 0; // called when string value is set from the ABK interface
    virtual void OnSetValue (double dSet) const = 0;               // called when double value is set from the ABK interface
    virtual void OnSetValue (bool bSet) const = 0;                 // called when boolean value is set from the ABK interface
    virtual void OnTrendDependenciesChanged (int nDependants) const; // called when daq trend was added/removed

    // operators
  public:
    CVarRef& operator = (CVarRef& rOther);
  };


} // namespace

