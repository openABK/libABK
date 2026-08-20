// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------------------------
//  _____  __   __  _____  _    _        ____              
// |  ___||  \ /  ||  _  \| |  | |      / ___|  _   _  ___ 
// |  __| |   V   ||  -  /| |  | |  __  \___ \ | | | |/ __|
// | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
// |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
//                                              |___/      
// Filename:    AbkServerDiscovery.cpp
// Created:     2012-09-24 (14:18)
// Author:      D. Burger
// Description: ABK server discovery demo
//------------------------------------------------------------------------------------------------


#include "stdafx.h"
#include <conio.h>

#pragma warning(disable : 4996)
#pragma comment(lib, "Ws2_32.lib") // use the winsockets library


#include "AbkServerFinder.h"

using namespace std;


bool AbkBindSocketToAdapter (SOCKET pSockBind) {return false;} // [introduced Nov, 22th 2013]  if sending with a specific adapter is desired, this function must bind the socket to the adapters address. else this can be an empty function



//--------------------------------------------------------------------------
// _tmain()                main routine
// --------
// Input: argc = number of arguments including the path of the executable
//        argv = argments. argv[0] is the path of the executable
// Return: always 0

int _tmain (int argc, _TCHAR* argv[])
  {
  cout<<"AbkServerDiscovery - demonstrates how ABK servers can be found on the network"<<endl;
  cout<<"-------------------------------------------------------------------------------"<<endl;

  WSADATA data;
  WSAStartup(MAKEWORD(2,2), &data);

  std::list <CAbkDiscoveredServer> lstServers;

  int nPreferred=AbkFindServers(ABK_CLASSNAME_DISPLAY,"MyDevice","1234",lstServers);

  std::list <CAbkDiscoveredServer>::iterator iterServer;
  for(iterServer=lstServers.begin();iterServer!=lstServers.end();++iterServer)
    {
    CAbkDiscoveredServer *pServer=&(*iterServer);
    cout<<"Server found:"<<endl;
    cout<<"  Name:        "<<pServer->m_strName.c_str()<<endl;
    cout<<"  Class:       "<<pServer->m_strClass.c_str()<<endl;
    cout<<"  Type:        "<<pServer->m_strType.c_str()<<endl;
    cout<<"  Serial:      "<<pServer->m_strSerial.c_str()<<endl;
    cout<<"  Address:     "<<pServer->m_strAddress.c_str()<<endl;
    cout<<"  Port:        "<<pServer->m_nPortHttp<<endl;
    cout<<"  Preferred:   "<<pServer->m_bPreferred<<endl;
    cout<<"  Description: "<<pServer->m_strDescriptionUrl;
    cout<<endl;
    }

  if(nPreferred>=0) // no ambiguous selection possible
    {
    iterServer=lstServers.begin();
    std::advance(iterServer,nPreferred);
    CAbkDiscoveredServer *pServer=&(*iterServer);

    cout<<"Found one unambigious ABK server at address "<<pServer->m_strAddress<<":"<<pServer->m_nPortHttp<<"."<<endl;
    cout<<"Press (y) for opening your browser, (n) for exit"<<endl;
    while(true)
      {
      int nKey=getch();
      if(nKey=='y')
        break;
      else if(nKey=='n')
        exit(0);
      }
    char strPort[10];
    sprintf(strPort,"%d",pServer->m_nPortHttp);
    std::string strConnectAddr;
    strConnectAddr+="http://";
    strConnectAddr+=pServer->m_strAddress;
    strConnectAddr+=":";
    strConnectAddr+=strPort;
    ShellExecute(NULL,_T("open"),strConnectAddr.c_str(),NULL,NULL,SW_SHOWNORMAL);
    exit(0);
    }
  else
    {
    if(lstServers.size()==0)
      {
      cout<<"No server found"<<endl;
      }
    cout<<"Press any key to exit"<<endl;
    getch();
    }
  return 0;
  }


