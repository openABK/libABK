// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------------------------
//  _____  __   __  _____  _    _        ____              
// |  ___||  \ /  ||  _  \| |  | |      / ___|  _   _  ___ 
// |  __| |   V   ||  -  /| |  | |  __  \___ \ | | | |/ __|
// | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
// |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
//                                              |___/      
// Filename:    MyClientVar.cpp
// Created:     2012-08-14 (07:48)
// Author:      D. Burger
// Description: customized client variable
//------------------------------------------------------------------------------------------------

#include "stdafx.h"

//#include <conio.h>
#include <iostream>
#include <iomanip>

#include "MyClientVar.h"


using namespace std;
using namespace Abk;


extern int g_nValuesRx; // performance metering: number of values recieved



//--------------------------------------------------------------------------
// CMyClientVar()          Constructor of CMyClientVar
// --------------
// Input: pDaqOwner = owning daq list of the daq variable
//        strName = name of the variable
// Return: 

CMyClientVar::CMyClientVar (Abk::CAbkClientDaq *pDaqOwner, LPCTSTR pszName)
  : m_strName(pszName)
  , CAbkClientVar(pDaqOwner)
  {
     
  }


//--------------------------------------------------------------------------
// OnValueFromServer()     called when data arrived from server
// -------------------
// Input: pSource = 
// Return: 

/*virtual*/ void CMyClientVar::OnValueFromServer (CJsonParserAtl *pSource)
  {
  g_nValuesRx++; // count number of variables recieved

  //VARIANT varValue;
  //pSource->ExtractValueAtl(&varValue);
  //cout<<varValue.lVal<<endl;
  }



//--------------------------------------------------------------------------
// GetName()               returns the name of the variable
// ---------
// Input: -
// Return: the name of the variable

/*virtual*/ LPCTSTR CMyClientVar::GetName (void) const
  {
  return m_strName;
  }

