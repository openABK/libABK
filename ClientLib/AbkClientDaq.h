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
      std::vector<CAbkClientVar *> m_vectVars; // list of variables in the daq
      int m_nCycleMs; // update cycle in ms
      size_t m_nVarCountAtServer; // backup of number of vars recently put to the server
      bool m_bCycleInvalid; // true if cycle is to be updated at the server
      bool m_bVarlistInvalid; // true if the variable list is to be updated at the server

    // construction/destruction/setup
    public:
      CAbkClientDaq (LPCTSTR pszName, LPCTSTR pszUrl);
      virtual ~CAbkClientDaq ();

    // methods and properties
    public:
      void SetCycle (int nCycleMs); // sets the cycle w/o sending it to server
      int GetCycle (void); // returns the cycle of the DAQ
      bool AddVar (CAbkClientVar *pVar); // adds a variable
      void DeleteAllVars (void); // deletes and removes all variables of this DAQ
      CAbkClientVar *FindVar (LPCTSTR pszName); // searches for a variable by name
      bool Update (void); // updates the daq list at the server

    // overrideables
    protected:
      virtual bool OnBeginDataFromServer (void); // called before the variables OnValueFromServer() calls for this DAQ start
      virtual void OnEndDataFromServer (void); // called when calls to variables OnValueFromServer() for this DAQ are done
      virtual bool OnValueFromServer (CAbkClientVar *pVarTarget, CJsonParserAtl *pSource) const=0; // called when value from server arrived
    
    // implementation
    protected:
      int GetSessionId (void); // returns the session id

    };


  }// namespace