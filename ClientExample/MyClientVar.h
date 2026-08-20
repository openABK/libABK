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
// Filename:    MyClientVar.h
// Created:     2012-08-14 (07:48)
// Author:      D. Burger
// Description: customized client variable
//------------------------------------------------------------------------------------------------




#pragma once


#include "AbkClientAll.h"




class CMyClientVar : public Abk::CAbkClientVar
  {
  protected:
    CString m_strName;
  public:
    CMyClientVar (Abk::CAbkClientDaq *pDaqOwner, LPCTSTR pszName);
    /*virtual*/ LPCTSTR GetName (void) const; // returns the name of the variable
    /*virtual*/ void OnValueFromServer (CJsonParserAtl *pSource); // called when data arrived from server

  };


