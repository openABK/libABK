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
// Filename:    MyClient.cpp
// Created:     2012-08-14 (07:38)
// Author:      D. Burger
// Description: customized ABK client class
//------------------------------------------------------------------------------------------------


#include "stdafx.h"

//#include <conio.h>
#include <iostream>
#include <iomanip>

#include "MyClient.h"

#ifdef WINCE
#include <iphlpapi.h>
#pragma comment(lib, "IPHLPAPI.lib")
#endif

using namespace std;
using namespace Abk;



//--------------------------------------------------------------------------
// GetMacAddress()         formats the mac address into a string
// ---------------
// Input: -
// Return: formatted mac address, empty string on error

CString GetMacAddress (void)
  {
  static CString strResult;
  if(!strResult.IsEmpty())
    return strResult;

#ifdef WINCE
  IP_ADAPTER_INFO info;
  memset(&info,0,sizeof(info));
  IP_ADAPTER_INFO *pInfo=&info;
  ULONG ulSize=sizeof(IP_ADAPTER_INFO);
  DWORD dwResult=GetAdaptersInfo(&info,&ulSize);
  if(dwResult==ERROR_BUFFER_OVERFLOW) // if not sufficient space for adapter info
    {
    pInfo=reinterpret_cast<IP_ADAPTER_INFO *>(new BYTE[ulSize]);
    ASSERT(pInfo);
    memset(pInfo,0,ulSize);
    dwResult=GetAdaptersInfo(pInfo,&ulSize);
    }
  if(dwResult==0) // if successfully
    {
    unsigned int nSection;
    LPCTSTR pszFormat=_T("%02x");
    for(nSection=0;nSection<pInfo->AddressLength;nSection++)
      {
      strResult.AppendFormat(strFormat,pInfo->Address[nSection]);
      strFormat=_T("-%02x");
      }
    }
  else
    strResult=_T("");
  if(pInfo!=&info)
    delete pInfo;
  return strResult;
#else
  TCHAR strBuffer[20];
  // unsigned char MACData[6];
  UUID uuid;
  UuidCreateSequential( &uuid );    // Ask OS to create UUID
  int nCol=0;
  for(int i=0;i<6;i++)  // Bytes 2 through 7 inclusive are MAC address
    {
    int nWritten=_stprintf_s(strBuffer+nCol,sizeof(strBuffer)/sizeof(TCHAR)-nCol,_T("%02X"),(int)uuid.Data4[i+2]);
    if(nWritten<0)
      return strResult;
    nCol+=nWritten;
    if(i<5)
      {
      strBuffer[nCol++]='-';
      strBuffer[nCol]='\0';
      }
    }
  strResult=strBuffer;
  return strResult;
#endif
  }







//--------------------------------------------------------------------------
// OnServerEvent()         called when server sent an event
// ---------------
// Input: pEventData = pointer to data of recieved event
// Return: pointer to object to recieve data for next event

/*virtual*/ CAbkServerEvent *CMyClient::OnServerEvent (CAbkServerEvent *pEventData)
  {
  if(1) // this block can be handled in a deferred manner
    {
    CAbkServerEvent::CData dataEvent;
    pEventData->GetEvent(dataEvent); // get and release the event object
    }
  return pEventData; // use the same
  }



//--------------------------------------------------------------------------
// OnLogAdded()         notifies that the log queue has got new entities. may be called in any thread context!
// ---------
// Input: -
// Return: 

/*virtual*/ void CMyClient::OnLogAdded (void)
  {
  static std::map<LOGSEVERITY,LPCTSTR> s_mapSeverityTokens=
    {
      {LOGSEVERITY_INFO    ,_T("Info")},
      {LOGSEVERITY_DEBUG   ,_T("Debug")},
      {LOGSEVERITY_TRACE   ,_T("Trace")},
      {LOGSEVERITY_WARNING ,_T("Warning")},
      {LOGSEVERITY_ERROR   ,_T("Error")},
    };
  for(;;)
    {
    LOGSEVERITY nSeverity;
    CString strLogMessage;
    if(!PopLog(nSeverity,strLogMessage)) // get one message
      break;
    std::map<LOGSEVERITY,LPCTSTR>::const_iterator itSeverityToken=s_mapSeverityTokens.find(nSeverity);
    LPCTSTR pszSeverityToken=_T("[undefined]");
    if(itSeverityToken!=s_mapSeverityTokens.end())
      pszSeverityToken=itSeverityToken->second;
    CString strOutput;
    strOutput.Format(_T("%s %s"),pszSeverityToken,(LPCTSTR)strLogMessage);
    OutputDebugString(strOutput);
    }
  }







//--------------------------------------------------------------------------
// Create()                creates client
// --------
// Input: strServerAddress = server address
//        nPort = port to connect to
// Return: 

bool CMyClient::Create (LPCTSTR pszServerAddress, int nPort)
  {
  return __super::Create(pszServerAddress,8080,&m_seRx,_T("Display"),_T("Demo"),GetMacAddress());
  }

