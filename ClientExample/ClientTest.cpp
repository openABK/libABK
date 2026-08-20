// SPDX-License-Identifier: MIT
// AbkClient.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#ifndef WINCE
#include <conio.h>
#endif
#include <iostream>
#include <string>
#include <sstream>
#include <time.h>
//#include "StopWatch.h"

#include "AbkClientVar.h"
#include "AbkClientDaq.h"
#include "AbkClient.h"
#include "AbkServerEvent.h"

#include "MyClient.h"
#include "MyClientVar.h"

#include "ValuesFromSpec.h"



#pragma comment(lib, "Rpcrt4.lib")  // used for UuidCreateSequential
#pragma warning(disable:4996) // unsiave functions are sufficient for the demo

using namespace std;
using namespace Abk;



int g_nValuesRx=0; // performance metering: number of values recieved



bool AbkBindSocketToAdapter (SOCKET pSockBind) {return false;} // [introduced Nov, 22th 2013]  if sending with a specific adapter is desired, this function must bind the socket to the adapters address. else this can be an empty function








void Test (void)
  {
  //CStopwatch watchRx;

  CMyClient client1;

  bool bSuccess=TRUE;

  //time_t tmServer;

  bSuccess&=client1.Create(_T("192.168.179.191"),8080);

  CMemFile fileDest(8192);
  bSuccess&=client1.DownloadFile(_T("/abk/jpegs/image1.jpg"),fileDest);
  int nFileLen=(int)fileDest.GetLength();
  BYTE *pRawData=fileDest.Detach();
  if(pRawData)
    free(pRawData);
  volatile int i=0;


#ifdef commented_out
  
  CTime tmModified;
  bSuccess&=client1.GetLastModified(_T("/index.html"),&tmModified);
  if(bSuccess)
    {
    CString strX;
    strX=tmModified.Format(_T("%A, %B %d, %Y, %H:%M:%S"));
    cout<<CT2A(strX)<<endl;
    }

  //client1.SendEvent(ABK_CLIENTEVENT_BUTTON,_T("Xx"),0,0);

  double dVarValue=0.;
  client1.GetVarValue(_T("Reifendruck"),&dVarValue);

  
  bSuccess=client1.SetVarValue(_T("Reifendruck"),10.);

  double dStorageTotal=-1.;
  double dStorageFree=-1.;
  bSuccess=client1.GetStorageInfo(unsigned long long &rTotal, unsigned long long &rFree);


  std::list<CString> lstVars;
  bSuccess=client1.GetVarList(&lstVars);
  
  CAbkClientDaq *pDaq1=new CAbkClientDaq(_T("DaqList1"),false);

  std::list<CString>::iterator iterVar;
  for(iterVar=lstVars.begin();iterVar!=lstVars.end();++iterVar)
    {
    CMyClientVar *pVarQuery=new CMyClientVar(pDaq1,*iterVar);
    pDaq1->AddVar(pVarQuery);
    }

  //CMyClientVar *pVar1=new CMyClientVar(pDaq1,_T("Reifendruck"));
  //CMyClientVar *pVar2=new CMyClientVar(pDaq1,_T("Variable2"));
  //pDaq1->AddVar(pVar1);
  //pDaq1->AddVar(pVar2);
  pDaq1->SetCycle(1);
  client1.AddDaq(pDaq1);

  watchRx.Start();

  std::list<CAbkClientMeta> lstMetaData;
  std::list<LPCTSTR> lstMetaQuery;
  //lstMetaQuery.push_back(pVar1->GetName());
  //lstMetaQuery.push_back(pVar2->GetName());
  bSuccess=client1.GetVarMeta(lstMetaQuery,&lstMetaData);




//for(int i=0;i<10;i++)
//  {
  bSuccess=client1.GetCurrentServerTime(&tmServer);
  if(bSuccess)
    {
    int nDaylightHours=0;
    _get_daylight(&nDaylightHours);
    tmServer-=nDaylightHours*3600;
    cout<<ctime(&tmServer)<<endl;
    }
//  }

  _getch();
  client1.DeleteDaq(pDaq1);
  client1.TidyUp();

  watchRx.Stop();
  cout<<"Elapsed: "<<watchRx.GetElapsedTimeSec()<<" s. Number of values: "<<g_nValuesRx<<". => "<<(double)g_nValuesRx/watchRx.GetElapsedTimeSec()<<" Vals/s."<<endl;
  _getch();
#endif

  // #### todo: error log const std::stringstream *pErrorLog=client1.GetErrorLog();
  // #### cout<< pErrorLog->str().c_str();

  //CAtlHttpClient client;
  //CAtlNavigateData navData;
  //bool bSuccess;

  //navData.SetPort(8080);
  //navData.SetMethod(_T("GET"));
  //bSuccess=client.Navigate(_T("192.168.178.22"),_T("/abk/current_time"),&navData);

  ///* START Code to Receive and Display Header */
  //DWORD sizehead;
  //client.GetRawResponseHeader( 0, &sizehead );
  //BYTE* bufhead = new BYTE[sizehead];
  //memset( bufhead, 0, sizehead );
  //if (client.GetRawResponseHeader( bufhead, &sizehead ))
  //  {
  //  cout << ("------- HTTP response headers  ----------") << endl;
  //  cout << bufhead << endl;
  //  cout << ("-----------------------------------------") << endl;
  //  }
  //delete [] bufhead;
  ///* END Code to Receive and Display Header */

  ///* Start code to retrive and display body */
  //DWORD sizebody;
  //const BYTE *bufbody;
  //sizebody = client.GetBodyLength();
  ////bufbody = new BYTE[sizebody];
  //if ( bufbody = client.GetBody() ) {
  //  cout << ("------- HTTP response BODY  ----------") << endl;
  //  cout << bufbody << endl;
  //  cout << ("-----------------------------------------") << endl;
  //  }
  /* END code to retrive and display body */
  //delete [] bufbody; // why is this not working?
  }








//--------------------------------------------------------------------------
// _tmain()                main
// --------
// Input: argc = 
//        argv = 
// Return: 

int _tmain(int argc, _TCHAR* argv[])
  {
  Test();
  return 0;
  }

