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
// Filename:    main.cpp
// Created:     2012-09-24 (14:27)
// Author:      D. Burger
// Description: ABK server demo implementation
//------------------------------------------------------------------------------------------------

#include "stdafx.h"

#if defined(_WIN32)
#define _CRT_SECURE_NO_DEPRECATE 
#define _SCL_SECURE_NO_DEPRECATE 
#define _CRT_SECURE_NO_WARNINGS  // Disable deprecation warning in VS2005 
#pragma warning(disable : 4996)
#pragma comment(lib, "Ws2_32.lib")  // use sockets
#else
#define _XOPEN_SOURCE 600  // For PATH_MAX on linux
#endif


#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdarg.h>
#include <conio.h>
#include <time.h>
#include <iostream>
#include <sstream>

#include <HttpServer.h>

#include "MyLoggerInterface.h"
#include "MyFakeLogger.h"
#include "ValuesFromSpec.h"

#ifdef _WIN32
#include <winsvc.h>
#define PATH_MAX MAX_PATH
#define S_ISDIR(x) ((x) & _S_IFDIR)
#define DIRSEP '\\'
#define snprintf _snprintf
//#define vsnprintf _vsnprintf
#define sleep(x) Sleep((x) * 1000)
#define WINCDECL __cdecl
#else
#include <sys/wait.h>
#include <unistd.h>
#define DIRSEP '/'
#define WINCDECL
#endif // _WIN32


using namespace std;


#define HTML_DIR "X:\\Entwicklung\\SW\\Visualisierung\\Abk\\Server\\Browser\\"
#define JPEGS_BASE_URL "/abk/jpegs/"

//--------------------------------------------------------------------------
// EventHandlerAbk()       callback for a http-handler
// -----------------
// Input: pObject = general purpose object pointer, here used to identify the
//                  instance of the logger interface
//        rRequest = reference containing information to the request
//        rResponse = reference to be populated with the response data
//        strLocalDir = local directory, can be used for file accesses, ignored here
//        nUrlOptionFlags = options specific to the request url, ignored here
// Return: -

void EventHandlerAbk (void *pObject, const CHttpServer::REQUEST &rRequest, CHttpServer::RESPONSE &rResponse, const std::string &strLocalDir, CHttpServer::URLOPTIONS_FLAGS nUrlOptionFlags)
  {
  ASSERT(pObject); // forgotten to specify the logger interface instance
  CLoggerInterface *pIfLogger=reinterpret_cast<CLoggerInterface *>(pObject);

  // convert request data from http-server-format to openABK-interface-format
  CLoggerInterface::HTTP_REQUEST reqIf;
  reqIf.nHttpMethod=pIfLogger->DecodeHttpMethod(rRequest.strMethod.c_str()); // decode the method
  reqIf.pUrl=&rRequest.strUrl;
  reqIf.pQueryString=&rRequest.strQuery;
  reqIf.pPostData=&rRequest.strData;
  reqIf.pClientAddr=&rRequest.strClientAddr;

  CLoggerInterface::HTTP_RESPONSE respIf;
  std::string strResponse;
  respIf.pResponseData=&strResponse; // map the response strings
  respIf.pExtraHeaders=&rResponse.strExtraHeaders;
  respIf.nStatusCode=rResponse.nStatusCode;
  
  pIfLogger->HandleHttpRequest(reqIf,respIf); // let the logger interface handle the request

  // convert response from openABK-interface-format to http-server-format
  rResponse.nStatusCode=respIf.nStatusCode;
  int nResponseLen=strResponse.size(); 
  rResponse.vectData.resize(nResponseLen);
  memcpy(rResponse.vectData.data(),strResponse.c_str(),nResponseLen);
  }




//--------------------------------------------------------------------------
// EventHandlerImages()    handles image requests
// --------------------
// Input: pObject = general purpose object pointer, here used to identify the
//                  instance of the logger interface
//        rRequest = reference containing information to the request
//        rResponse = reference to be populated with the response data
//        strLocalDir = local directory, can be used for file accesses
//        nUrlOptionFlags = options specific to the request url, ignored here
// Return: -

void EventHandlerImages (void *pObject, const CHttpServer::REQUEST &rRequest, CHttpServer::RESPONSE &rResponse, const std::string &strLocalDir, CHttpServer::URLOPTIONS_FLAGS nUrlOptionFlags)
  {
  if((rRequest.strMethod=="GET"))
    {
    std::string strPath=strLocalDir;
    ASSERT(strPath.size()>0); // you should have specified a non-empty local dir!

    static const size_t nBaseUrlLen=strlen(JPEGS_BASE_URL); // skip the url already defined in the options and leave the path relative to the options url
    size_t nCol=nBaseUrlLen;
    for(;nCol<rRequest.strUrl.size();nCol++)
      {
      char cCopy=rRequest.strUrl[nCol];
      if(cCopy=='/')
        cCopy='\\';
      strPath.append(1,cCopy);
      }

    // separate extension
    int nColExtension=strPath.rfind('.');
    std::string strExtension;
    if(nColExtension!=std::string::npos)
      {
      strExtension=strPath.c_str()+nColExtension;
      strPath.resize(nColExtension);
      }
    
    // make file index out of the system ticks, then append
    char cNumFormatted[10];
    int nImageIndex=(GetTickCount()/32)%128;
    sprintf(cNumFormatted," %03d",nImageIndex+1);
    strPath.append(cNumFormatted);

    strPath.append(strExtension);

    FILE *pFile=NULL;
    fopen_s(&pFile,strPath.c_str(),"r+b");
    if(pFile)
      {
      std::fseek(pFile,0,SEEK_END); // get file size
      fpos_t nFileLen;
      std::fgetpos(pFile,&nFileLen);
      std::fseek(pFile,0,SEEK_SET); // rewind
      rResponse.vectData.resize((int)nFileLen);
      int nReadLen=std::fread(rResponse.vectData.data(),1,(size_t)nFileLen,pFile); // read file
      fclose(pFile);
      rResponse.nStatusCode=200;
      }
    else
      {
      rResponse.nStatusCode=404;
      }
    }
  else
    {
    rResponse.nStatusCode=403; // forbidden
    }
  }


#ifdef USE_MONGOOSE_SERVER
//--------------------------------------------------------------------------
// event_handler()         handles events from mongoose http server
// ---------------
// Input: event = pointer to the event structure
//        conn = pointer to connection information
//        request_info = pointer to information of the request
// Return: non-NULL if event was handled, NULL if it was not handled

void *event_handler(enum mg_event event, struct mg_connection *conn, const struct mg_request_info *request_info)
  {
  static void* done = "done"; 
 
  //if (event == MG_NEW_REQUEST)
  //  {
  //  // pack the request info and connection info into one item
  //  MYREQINFO reqEvent;
  //  reqEvent.pConn=conn;
  //  reqEvent.pRequestInfo=request_info;

  //  // decode http method string into an enumerated value
  //  CLoggerInterface *pIfLogger=reinterpret_cast<CLoggerInterface *>(request_info->user_data);
  //  CLoggerInterface::HTTP_METHOD nMethod=pIfLogger->DecodeHttpMethod(request_info->request_method); // decode the method

  //  // let the interface handle the reuquest
  //  const char *pszQuery=request_info->query_string;
  //  if(!strQuery)
  //    strQuery="";
  //  bool bHandled=pIfLogger->HandleHttpRequest(request_info->uri,nMethod,(void *)&reqEvent,strQuery);
  //  
  //  if(bHandled)
  //    return done;
  //  return NULL;

/*
    if(!strcmp(request_info->uri,"/abk/storage_info"))
      {
      if(!strcmp(request_info->request_method,"GET"))
        {
        const char* mimeType = "application/json"; //"text/plain"; 
        mg_printf(conn, "HTTP/1.1 200 OK\r\n"
                        "Cache: no-cache\r\n"
                        "Content-Type: %s\r\n"
                        "Content-Length: 0\r\n"
                        "\r\n",
                        mimeType);
        mg_write(conn,"",0);
        return done; 
        }
      else
        {
        mg_printf(conn, "HTTP/1.1 %d Error (%s)\r\n\r\n%s",500,"we only care about HTTP GET","we only care about HTTP GET"); 
        return done; // return not null means we handled the request 
        }
      }

    if (!strcmp(request_info->uri,"/storage_info"))
      {
      if(!strcmp(request_info->request_method,"GET"))
        {
        int nTotal=1000;
        int nFree=200;
        char strValBuffer[100];
        int nContentLength=sprintf(strValBuffer,"{\r\n\"Total\":%d, \"Free\":%d\r\n}",nTotal,nFree);
        mg_printf(conn, "HTTP/1.1 200 OK\r\n"
                        "Cache: no-cache\r\n"
                        "Content-Type: application/json\r\n"
                        "Content-Length: %d\r\n"
                        "\r\n",
                        nContentLength); 
        mg_write(conn, strValBuffer, nContentLength); 
        return ""; 
        }
      else
        {
        mg_printf(conn, "HTTP/1.1 %d Error (%s)\r\n\r\n%s",500,"we only care about HTTP GET","we only care about HTTP GET"); 
        return ""; // return not null means we handled the request 
        }
      }

    else if (!strcmp(request_info->uri,"/SessionId"))
      {
      if(!strcmp(request_info->request_method,"GET"))
        {
        char strValBuffer[100];
        int nSessionId=ifLogger.GenerateSession();
        int nContentLength=sprintf(strValBuffer,"{\r\n\"SessionId\":%d\r\n}",nSessionId);
        g_nValue++;
        const char* mimeType = "application/json"; //"text/plain"; 
        mg_printf(conn, "HTTP/1.1 200 OK\r\n"
                        "Cache: no-cache\r\n"
                        "Content-Type: %s\r\n"
                        "Content-Length: %d\r\n"
                        "\r\n",
                        mimeType, 
                        nContentLength); 
        mg_write(conn, strValBuffer, nContentLength); 
        return done; 
        }
      else
        {
        mg_printf(conn, "HTTP/1.1 %d Error (%s)\r\n\r\n%s",500,"we only care about HTTP GET","we only care about HTTP GET"); 
        return done; // return not null means we handled the request 
        }
      }
    else
      return NULL;  */
//    } 
  return NULL; 
  } 
#endif




//--------------------------------------------------------------------------
// PrintHelpScreen()       shows the help screen and the usage
// -----------------
// Input: -
// Return: 

void PrintHelpScreen (void)
  {
  cout<<"Logger Fake Program pretents to be a logger."<<endl;
  cout<<"Usage: AbkServer [Port Varcount AnimationCycle]"<<endl;
  cout<<"-------------------------------------------------------------------------------"<<endl;
  cout<<"Press 0..9 to setup a param, for later use on e.g. variable selection"<<endl;
  cout<<"Press a key to start an operation:"<<endl;
  cout<<"x: terminate program"<<endl;
  cout<<"?: show this help screen"<<endl;
  cout<<"l: simulate a variable limit violation"<<endl;
  cout<<"M: start measurement"<<endl;
  cout<<"m: end measurement"<<endl;
  cout<<"i: identify. A client shall identify itself (by flashing etc.)"<<endl;
  cout<<"a: app changed. All clients must reload app/config files"<<endl;
  cout<<"f: request client to open a form, F closes client form"<<endl;
  cout<<"v: fire event to all clients that variable list has changed"<<endl;
  cout<<"r: fire event to all clients for audio recording, R to stop"<<endl;
  }



//--------------------------------------------------------------------------
// _tmain()                main routine
// --------
// Input: argc = number of arguments including the path of the executable
//        argv = argments. argv[0] is the path of the executable
// Return: always 0

int _tmain(int argc, _TCHAR* argv[])
  {
  for(;;) // endless loops, allows to test shutting-down
    {
    if(1) // scope block allows http server and logger and logger interface to fall out-of-scope
      {
      int nPortHttp=8080;
      int nVarCount=10;
      int nAniCycle=100;
      if(argc>=4)
        {
        swscanf(argv[1],_T("%d"),&nPortHttp);
        swscanf(argv[2],_T("%d"),&nVarCount);
        if(nVarCount<10)
          nVarCount=10;
        if(nVarCount>FAKE_VARCOUNT_MAX)
          nVarCount=FAKE_VARCOUNT_MAX;
        swscanf(argv[3],_T("%d"),&nAniCycle);
        }

      char strPort[10];
      sprintf(strPort,"%d",nPortHttp);
      WSADATA data;
      WSAStartup(MAKEWORD(2,2), &data);

      CMyFakeLogger loggerFake(nVarCount,nAniCycle); // demo logger generating faked data
      sockaddr_in addrBindLocal;
      addrBindLocal.sin_port=htons(nPortHttp);
      addrBindLocal.sin_addr.S_un.S_addr=INADDR_ANY;
      CMyLoggerInterface ifLogger(&loggerFake,addrBindLocal); // interface to the loggers internals

      loggerFake.Init(&ifLogger);

      // start http server
      CHttpServer svrHttp;
      svrHttp.AddUrlOption(_T("/abk/"),&ifLogger,EventHandlerAbk,NULL,NULL,CHttpServer::URLO_DEFAULT);
      svrHttp.AddUrlOption(_T(ABK_SERVICE_CLIENTSTATES),  NULL,NULL,_T(LOCDIR_CLIENTSTATES),  NULL,(CHttpServer::URLOPTIONS_FLAGS)(CHttpServer::URLO_FILE_ALLOW_READ|CHttpServer::URLO_FILE_ALLOW_WRITE));
      svrHttp.AddUrlOption(_T(ABK_SERVICE_CLIENTCONFIG),  NULL,NULL,_T(LOCDIR_CLIENTCONFIG),  NULL,(CHttpServer::URLOPTIONS_FLAGS)(CHttpServer::URLO_FILE_ALLOW_READ));
      svrHttp.AddUrlOption(_T(ABK_SERVICE_CLIENTFIRMWARE),NULL,NULL,_T(LOCDIR_CLIENTFIRMWARE),NULL,(CHttpServer::URLOPTIONS_FLAGS)(CHttpServer::URLO_FILE_ALLOW_READ));
      svrHttp.AddUrlOption(_T("/abk/browser/"),           NULL,NULL,_T(HTML_DIR),             NULL,(CHttpServer::URLOPTIONS_FLAGS)(CHttpServer::URLO_FILE_ALLOW_READ));


      // svrHttp.AddUrlOption(_T(JPEGS_BASE_URL),NULL,NULL,_T("c:\\abk\\jpegs\\"),NULL,(CHttpServer::URLOPTIONS_FLAGS)(CHttpServer::URLO_FILE_ALLOW_READ));
      svrHttp.AddUrlOption(_T(JPEGS_BASE_URL),&ifLogger,EventHandlerImages,_T("c:\\abk\\jpegs\\"),NULL,CHttpServer::URLO_FILE_ALLOW_READ);
      svrHttp.Run(addrBindLocal);
      cout<<"ABK server started, press x to terminate. Port: "<<nPortHttp<<" Vars: "<<nVarCount<<" Cycle: "<<nAniCycle<<" ms."<<endl;
      PrintHelpScreen();

    //TestUdp();
      int nParam=0; // 0..9, param entered by keys '0'..'9'
      for(;;)
        {
        int nKey=_getch();
        if((nKey>='0')&&(nKey<='9'))
          {
          nParam=nKey-'0';
          cout<<"You have selected parameter value of "<<nParam<<"."<<endl;
          continue;
          }
        if(nKey=='x')
          break;
        switch(nKey)
          {
        case '?':
          PrintHelpScreen();
          break;
        case 'l': // variable limit
          cout<<"Limit alert for Variable "<< nParam << "." <<endl;
          loggerFake.DoVarLimit(nParam,nParam,"DummyLimit",true);
          break;
        case 'M': // start measurement
          loggerFake.DoStartMeasurement();
          break;
        case 'm': // end measurement
          loggerFake.DoEndMeasurement();
          break;
        case 'i': // client idedtification
          ifLogger.Identify(nParam);
          break;
        case 'a':
          ifLogger.NotifyAppChanged();
          break;
        case 'f':
          ifLogger.OpenForm("demo");
          break;
        case 'F':
          ifLogger.CloseForm("demo");
          break;
        case 'v':
          ifLogger.FireEventToAllClientsVarlistChanged();
          break;
        case 'r':
          ifLogger.RequestAudioRec(nParam,0);
          break;
        case 'R':
          ifLogger.StopAudioRec(nParam);
          break;
          }

        }
      loggerFake.Shutdown();
      ifLogger.Shutdown(); // shutdown the logger interface before the destructor gets called when falling out of scope
      fflush(stdout);
      svrHttp.Stop();
      cout<<" done."<<endl;
      break;

      }
    Sleep(2000);
    }

  return EXIT_SUCCESS;
  }
