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
// Filename:    MyClient.h
// Created:     2012-08-14 (07:37)
// Author:      D. Burger
// Description: customized ABK client class
//------------------------------------------------------------------------------------------------



#pragma once


#include "AbkClient.h"
#include "AbkServerEvent.h"


class CMyClient : public Abk::CAbkClient
  {
  protected:
    Abk::CAbkServerEvent m_seRx; // event to rx event params
  public:
    bool Create (LPCTSTR pszServerAddress, int nPort);
    /*virtual*/ Abk::CAbkServerEvent *OnServerEvent (Abk::CAbkServerEvent *pEventData); // called when server sent an event
    /*virtual*/ void OnLogAdded (void); // notifies that the log queue has got new entities. may be called in any thread context!
  };

