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
// Filename:    AbkServerFinder.h
// Created:     2012-07-16 (11:13)
// Author:      D. Burger
// Description: ABK server discovery, class describing properties of a found server
//------------------------------------------------------------------------------------------------


#pragma once

#include <list>

//#include <conio.h>

#include "CrossPlatform.h"
#include "ValuesFromSpec.h"
#include "JsonFormatter.h"
#include "JsonParser.h"


#define ABK_DISCOVER_TIMEOUT_MS 1000     // timeout when searching for servers.

class CAbkDiscoveredServer;

typedef void (*PFN_ABK_DISCOVER_CALLBACK)(void *pThis, const std::list<CAbkDiscoveredServer> *pDiscovered, int nSuggestion); // callback when asynchronousely search for servers

class CAbkDiscoveredServer
  {
  public:
    enum {INVALID_PORT=0};
  // data members
  public:
    std::string m_strAddress;   // server address
    int m_nPortHttp;            // server HTTP port. INVALID_PORT designates invalid content
    std::string m_strClass;     // server class
    std::string m_strType;      // server type
    std::string m_strSerial;    // server serial
    std::string m_strName;      // unique name
    std::string m_strDescriptionUrl; // description of the server, optional
    bool m_bPreferred;          // if true: connection is preferred since it is held in a configuration database of the answering server

  // construction/destruction
  public:
    CAbkDiscoveredServer () {m_nPortHttp=INVALID_PORT; m_bPreferred=false;}
  
  // methods
  public:
    bool IsSameAddressAndPort (const CAbkDiscoveredServer &rOther) const;
    void CopyAddressAndPort (CAbkDiscoveredServer &rOther) {m_strAddress=rOther.m_strAddress; m_nPortHttp=rOther.m_nPortHttp;}
  };

bool operator == (CAbkDiscoveredServer const& lhs, CAbkDiscoveredServer const& rhs);
bool operator != (CAbkDiscoveredServer const& lhs, CAbkDiscoveredServer const& rhs);


int AbkFindServers (const char *pszClassName, const char *pszDeviceName, const char *pszSerial, std::list <CAbkDiscoveredServer> &lstResult, int nTimeoutMs=ABK_SERVERDISCOVER_TIMEOUT_MS); // fills list with all found ABK servers

void AbkFindServers (const char *pszClassName, const char *pszDeviceName, const char *pszSerial, PFN_ABK_DISCOVER_CALLBACK pfnOnReady, void *pObject, int nTimeoutMs=ABK_SERVERDISCOVER_TIMEOUT_MS); // search servers asynchronousely

bool AbkBindSocketToAdapter (SOCKET pSockBind); // [introduced Nov, 22th 2013]  if sending with a specific adapter is desired, this function must bind the socket to the adapters address. else this can be an empty function

