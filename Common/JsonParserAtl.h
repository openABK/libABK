// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------------------------
//  _____  __   __  _____  _    _        ____              
// |  ___||  \ /  ||  _  \| |  | |      / ___|  _   _  ___ 
// |  __| |   V   ||  -  /| |  | |  __  \___ \ | | | |/ __|
// | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
// |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
//                                              |___/      
// Filename:    JsonParserAtl.h
// Created:     2012-07-19 (17:59)
// Author:      D. Burger
// Description: JSON parser extended to ATL string and OLE variant capabilities
//------------------------------------------------------------------------------------------------




#pragma once

#include "JsonParser.h"

class _variant_t;

class CJsonParserAtl : public CJsonParser
  {
  // data members
  protected:

  // construction/destruction/setup
  public:
    CJsonParserAtl (const char *pStartOfExpression);
    ~CJsonParserAtl (void);

  // methods and attributes
  public:
    bool ExtractValueAtl (const char *pszName, CString &rGet) const;   // extracts string from object
    bool ExtractValueAtl (const char *pszName, _variant_t &rGet) const;   // extracts any data type from object
    bool ExtractValueAtl (CString &rGet) const;                        // gets string from array
    bool ExtractValueAtl (_variant_t &rGet) const;                        // extracts any data type from array
    bool GetName (CString &strGet) const; // returns name of the object/array/value
    bool GetValueString (CString &strGet) const; // returns the string form of the value

  // implementation
  protected:
    static bool UnescapeString (const char *pStart, const char *pEnd, CString *pGet); // unescapes a string
  };

