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
// Filename:    AbkClientDaq.h
// Created:     2012-07-12 (18:37)
// Author:      D. Burger
// Description: Data Acqusition List enabling interfacing to the ABK http interface
//------------------------------------------------------------------------------------------------



#pragma once


#include <vector>

#include "AbkClientVar.h"

namespace Abk
  {
  class CAbkClient;

  class CAbkClientDaq
    {
    friend class CAbkClient;

    // data members
    protected:
      CString m_strName; // name of the DAQ list
      CString m_strUrl; // url to maintain DAQ list at the server (either variables or mailbox url)
      CAbkClient *m_pOwner; // owning abk http client
      size_t m_nVarCountAtServer; // backup of number of vars recently put to the server

    // construction/destruction/setup
    public:
      CAbkClientDaq (LPCTSTR pszName, LPCTSTR pszUrl);
      virtual ~CAbkClientDaq ();

    // methods and properties
    public:
      bool Update(const std::vector<LPCTSTR>& vectVarNames, int nCycleMs); // updates the daq list at the server
      static void DiscardJson(CJsonParserAtl& jpSkip); // skips the complete DAQ in the JSON parser

    // overrideables
    protected:
      virtual bool OnDataFromServer(CJsonParserAtl& jpEvent) const = 0; // called when data arrived
    
    // implementation
    protected:
      int GetSessionId (void); // returns the session id

    };


  }// namespace