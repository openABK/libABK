// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------------------------
//  _____  __   __  _____  _    _        ____              
// |  ___||  \ /  ||  _  \| |  | |      / ___|  _   _  ___ 
// |  __| |   V   ||  -  /| |  | |  __  \___ \ | | | |/ __|
// | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
// |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
//                                              |___/      
// Filename:    AbkServerFinder.cpp
// Created:     2012-07-16 (11:13)
// Author:      D. Burger
// Description: ABK server discovery, class describing properties of a found server
//------------------------------------------------------------------------------------------------



#include "stdafx.h"
#include <assert.h>
#include "AbkServerFinder.h"

#define ABK_DISCOVERY_ATTEMPTS 3 // number of broadcast requests to be sent
#define DELAY_PER_RX 100 // 100 ms timeout when waiting for response


using namespace Abk;

//--------------------------------------------------------------------------
// operator ==             tests for equality
// -----------
// Input: lhs = left hand operand
//        rhs = right hand operand
// Return: true if equal, false if different

bool operator == (CAbkDiscoveredServer const& lhs, CAbkDiscoveredServer const& rhs)
  {
  if(lhs.m_strClass.compare(rhs.m_strClass))
    return false;
  if(lhs.m_strName.compare(rhs.m_strName))
    return false;
  if(lhs.m_strSerial.compare(rhs.m_strSerial))
    return false;
  if(lhs.m_strType.compare(rhs.m_strType))
    return false;
  if(lhs.m_bPreferred!=rhs.m_bPreferred)
    return false;
  if(lhs.m_nPortHttp!=rhs.m_nPortHttp)
    return false;
  if(lhs.m_strAddress.compare(rhs.m_strAddress))
    return false;
  return true;
  }



//--------------------------------------------------------------------------
// operator !=             tests for difference
// -----------
// Input: lhs = left hand operand
//        rhs = right hand operand
// Return: false if equal, true if different

bool operator != (CAbkDiscoveredServer const& lhs, CAbkDiscoveredServer const& rhs)
  {
  return !(lhs==rhs);
  }




//--------------------------------------------------------------------------
// AbkFindServers()        fills list with all found ABK servers
// ----------------
// Input: strClassName = ABK conform device class name, e.g. ABK_CLASSNAME_DISPLAY
//        strDeviceName = name of the requesting device, e.g. "SuperDisplay3000"
//        strSerial = serial number string of requesting device, e.t. "123a"
//        lstResult = list to receive the discovered servers
//        nTimeoutMs = time to wait for answers in milli seconds, approximately
// Return: index of unique preferred connection, -1 if ambigious connections in list or if error

int AbkFindServers (const char *pszClassName, const char *pszDeviceName, const char *pszSerial, std::list <CAbkDiscoveredServer> &lstResult, int nTimeoutMs/*=ABK_SERVERDISCOVER_TIMEOUT_MS*/)
  {
  int nPreferred=-1; // >=0: index of one and only preferred entity, -1: no preferred, -2: more than one preferred
  lstResult.clear();

  // prepare Tx
  sockaddr_in sadrTx; // request address
  SOCKET pSockTx; // socket for sending broadcast
  int nBroadcast=1; // enable broadcast for the socket

  sadrTx.sin_family = AF_INET;
  sadrTx.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  sadrTx.sin_port=htons ((u_short)ABK_ENUM_PORT);
  pSockTx = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
  if(pSockTx==INVALID_SOCKET)
    return -1;
  setsockopt(pSockTx,SOL_SOCKET,SO_BROADCAST,(char*)&nBroadcast,sizeof(nBroadcast)); // allow broadcast
  AbkBindSocketToAdapter(pSockTx); // [introduced Nov, 22th 2013] if sending with a specific adapter is desired, this function must bind the socket to the adapters address. else this can be an empty function

  // prepare Rx
  sockaddr_in sadrRx; // request address
  char cRxBuf[ABK_SERVERDISCOVER_MAXPAYLOAD+1];

  // send the request
  CJsonFormatter jfRequest;
  jfRequest.WriteValue(ABK_DISCOVERY_REQ_KEYNAME,ABK_DISCOVERY_REQ_KEYVALUE);
  jfRequest.WriteValue(ABK_DISCOVERY_REQ_CLASS  ,pszClassName);
  jfRequest.WriteValue(ABK_DISCOVERY_REQ_TYPE   ,pszDeviceName);
  jfRequest.WriteValue(ABK_DISCOVERY_REQ_SERIAL ,pszSerial);
  jfRequest.Close();
  std::string strRequest=jfRequest.GetStream()->str();
  if(strRequest.length()<=ABK_SERVERDISCOVER_MAXPAYLOAD) // only send when size is within the allowed range
    {
    int nAttempts;
    for(nAttempts=0;nAttempts<ABK_DISCOVERY_ATTEMPTS;nAttempts++)
      {
      ssize_t nSent=sendto(pSockTx,strRequest.c_str(),(int)strRequest.length(),0,(sockaddr*)&sadrTx,sizeof(sadrTx));
      if(nSent!=strRequest.length())
        {
        const char *pError = strerror(errno);
        std::cerr << "Failed to broadcast UDP: " << pError;
        closesocket(pSockTx);
        return -1;
        }
      }

    int nEntity=0; // index counter within list
    for(;nTimeoutMs>0;nTimeoutMs-=DELAY_PER_RX)
      {
      fd_set fdRecv;
      timeval tmvRecv; // timeout 100ms
      tmvRecv.tv_sec=0;
      tmvRecv.tv_usec=DELAY_PER_RX*1000;
      FD_ZERO(&fdRecv);
      FD_SET(pSockTx,&fdRecv);
      if(select(pSockTx+1,&fdRecv,NULL,NULL,&tmvRecv)>0) // read with timeout if no answer received
        {
        if(FD_ISSET(pSockTx,&fdRecv))
          {
          unsigned int nSockAddrInSize=sizeof(sockaddr_in);
          int nreceived=recvfrom(pSockTx,cRxBuf,sizeof(cRxBuf)-1,0,(sockaddr*)&sadrRx,&nSockAddrInSize);
          if(nreceived>=0)
            {
            cRxBuf[nreceived]='\0';
            CJsonParser parsAnswer(cRxBuf);
            CAbkDiscoveredServer svrDecode;
            std::string strKey; // key the server marked his answer with
            for(;!parsAnswer.IsDone();++parsAnswer)
              {
              parsAnswer.ExtractValue(ABK_DISCOVERY_RSP_KEYNAME,&strKey);
              parsAnswer.ExtractValue(ABK_DISCOVERY_RSP_ADDRESS,&svrDecode.m_strAddress);
              parsAnswer.ExtractValue(ABK_DISCOVERY_RSP_PORT,&svrDecode.m_nPortHttp);
              parsAnswer.ExtractValue(ABK_DISCOVERY_RSP_CLASS,&svrDecode.m_strClass);
              parsAnswer.ExtractValue(ABK_DISCOVERY_RSP_TYPE,&svrDecode.m_strType);
              parsAnswer.ExtractValue(ABK_DISCOVERY_RSP_SERIAL,&svrDecode.m_strSerial);
              parsAnswer.ExtractValue(ABK_DISCOVERY_RSP_NAME,&svrDecode.m_strName);
              parsAnswer.ExtractValue(ABK_DISCOVERY_RSP_NAME,&svrDecode.m_bPreferred);
              parsAnswer.ExtractValue(ABK_DISCOVERY_RSP_DESCURL,&svrDecode.m_strDescriptionUrl);
              parsAnswer.SkipItem();
              }
            if(!strKey.compare(ABK_DISCOVERY_RSP_KEYVALUE)) // if the key matches
              {
              // find duplicates
              std::list<CAbkDiscoveredServer>::iterator iterFound;
              for(iterFound=lstResult.begin();iterFound!=lstResult.end();++iterFound)
                {
                CAbkDiscoveredServer *pSvrFound=&(*iterFound);
                if(*pSvrFound==svrDecode) // if duplicate
                  break;
                }
              if(iterFound==lstResult.end()) // no duplicates found
                {
                lstResult.push_back(svrDecode);
                if(svrDecode.m_bPreferred)
                  {
                  if(nPreferred==-1)
                    nPreferred=nEntity;
                  else if(nPreferred>=0) // if already a preferred connection indicated
                    nPreferred=-2; // indicate that more than one are marked as preferred
                  }
                nEntity++;
                }
              }
            }
          }
        AbkSleepMs(100); // no 100ms timeout from rx, make 100ms delay
        }
      }
    }
  closesocket(pSockTx);
  if(lstResult.size()==1) // if list contains only one server..
    return 0; // .. suggest this server for connection
  if(nPreferred>=0) // if only one is marked as preferred
    return nPreferred;
  return -1; // no or more than one preferred entity
  }



typedef struct tagABKDISCOVERCALLBACK
  {
  PFN_ABK_DISCOVER_CALLBACK pfnOnReady; // function
  void *pObject; // object
  int nTimeoutMs; // timeout when waiting for server answers
  const char *pszClassName;
  const char *pszDeviceName;
  const char *pszSerial;
  } ABKDISCOVERCALLBACK;


//--------------------------------------------------------------------------
// AbkDiscoveryThread()    thread searching for servers
// --------------------
// Input: pvCallbackInfo = void pointer to callback function and object
// Return: always 0

static unsigned int THREAD_CALLCONV AbkDiscoveryThread (void *pvCallbackInfo)
  {
  ABKDISCOVERCALLBACK *pCallbackInfo=reinterpret_cast<ABKDISCOVERCALLBACK *>(pvCallbackInfo);
  assert(pCallbackInfo->pfnOnReady); // no function specified?
  std::list <CAbkDiscoveredServer> lstResult; // list recieving the found servers
  int nSuggestion=AbkFindServers(pCallbackInfo->pszClassName,pCallbackInfo->pszDeviceName,pCallbackInfo->pszSerial,lstResult,pCallbackInfo->nTimeoutMs); // start the discovery, function blocks
  pCallbackInfo->pfnOnReady(pCallbackInfo->pObject,&lstResult,nSuggestion);
  return 0;
  }


//--------------------------------------------------------------------------
// AbkFindServers()        search servers asynchronousely
// ----------------
// Input: strClassName = ABK conform device class name, e.g. ABK_CLASSNAME_DISPLAY
//        strDeviceName = name of the requesting device, e.g. "SuperDisplay3000"
//        strSerial = serial number string of requesting device, e.t. "123a"
//        pfnOnReady = callback when ready. the callback will be called
//                     nTimeoutMs milliseconds after the call of this
//                     function.
//        pObject = object passed to the callback function
//        nTimeoutMs = time to search for servers
// Return: -

void AbkFindServers (const char *pszClassName, const char *pszDeviceName, const char *pszSerial, PFN_ABK_DISCOVER_CALLBACK pfnOnReady, void *pObject, int nTimeoutMs/*=1000*/)
  {
  ABKDISCOVERCALLBACK callbackInfo;
  callbackInfo.pfnOnReady=pfnOnReady;
  callbackInfo.pObject=pObject;
  callbackInfo.pszClassName=pszClassName;
  callbackInfo.pszDeviceName=pszDeviceName;
  callbackInfo.pszSerial=pszSerial;
  AbkStartThread(AbkDiscoveryThread,&callbackInfo);
  }



//--------------------------------------------------------------------------
// IsSameAddressAndPort()  returns true if port and address are identical
// ----------------------
// Input: rOther = 
// Return: true if port and address are identical
//         false if different address or different port

bool CAbkDiscoveredServer::IsSameAddressAndPort (const CAbkDiscoveredServer &rOther) const
  {
  if((!m_strAddress.compare(rOther.m_strAddress)) && (m_nPortHttp==rOther.m_nPortHttp))
    return true;
  return false;
  }



