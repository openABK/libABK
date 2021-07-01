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
// Filename:    MyLoggerInterface.cpp
// Created:     2012-05-16 (15:18)
// Author:      D. Burger
// Description: demo interface adapting the http interface to an arbitrary data logger
//------------------------------------------------------------------------------------------------



#include "stdafx.h"
#include <assert.h>

#include "MyLoggerInterface.h"
#include "MyFakeLogger.h"
#include "JsonParser.h"
//#include "mongoose.h" // in this demo, we use the mongoose http server
#include "ValuesFromSpec.h"
#include <wincrypt.h>
#include <Msi.h> // used to query version information out of a microsoft installer module
#include <ifdef.h>


#define DISCOVERY_TRACE_RX
#undef DISCOVERY_TRACE_RX
#define DISCOVERY_TRACE_TX
#undef DISCOVERY_TRACE_TX

#define ADDITIONALHEADERS "" // removed due to performance losses "Connection: Close\r\n" // headers used with mongoose server

#pragma warning(disable : 4996)

#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Version.lib")
#pragma comment(lib, "Msi.lib")

using namespace std;

//--------------------------------------------------------------------------
// CMyVarRef()             Constructor of CMyVarRef
// -----------
// Input: strName = name of the variable
// Return: 

CMyVarRef::CMyVarRef (const char *pszName)
  : CVarRef(pszName)
  {
  m_pLogger=NULL;
  m_nIndex=-1;
  m_bMailbox=false;
  }


//--------------------------------------------------------------------------
// SetLocation()           sets the location where to find the real variable
// -------------
// Input: pLogger = pointer to logger where the variable can be found
//        bMailbox: true if mailbox rather than measurement variable
//        nIndex = index of the variable within the logger
// Return: -

void CMyVarRef::SetLocation (CMyFakeLogger *pLogger, bool bMailbox, int nIndex)
  {
  m_pLogger=pLogger;
  m_nIndex=nIndex;
  m_bMailbox=bMailbox;
  }


//--------------------------------------------------------------------------
// OnFormatValue()         called when a variable shall be formatted
// ---------------
// Input: rFormatterOut = stream to format the value to. stream must not be emptied
// Return: -

/*virtual*/ void CMyVarRef::OnFormatValue (CJsonStreamBase &rFormatterOut) const
  {
  assert(m_pLogger); // do we have initialized the location of the real variable
  assert(m_nIndex>=0);

  if(IsMailbox())
    {
    const FAKEMAILBOX *pLoggerMailbox=m_pLogger->GetMailbox(m_nIndex);
    if(pLoggerMailbox->nDataType==CMyFakeLogger::DATATYPE_STRING)
      rFormatterOut<<pLoggerMailbox->strValue;
    else if(pLoggerMailbox->nDataType==CMyFakeLogger::DATATYPE_DOUBLE)
      rFormatterOut<<pLoggerMailbox->dValue;
    else
      rFormatterOut<<pLoggerMailbox->bValue;
    }
  else
    {
    const FAKEVAR *pLoggerVar=m_pLogger->GetVar(m_nIndex); // get the logger variable
    rFormatterOut<<pLoggerVar->dValue;
    }
  }


//--------------------------------------------------------------------------
// OnFormatMAM()           called when a variable min-max-average shall be formatted
// -------------
// Input: rFormatterOut = stream to format the min/average/max value to. stream must not be emptied
// Return: -

/*virtual*/ void CMyVarRef::OnFormatMAM (CJsonStreamArray &rFormatterOut) const
  {
  if(!IsMailbox())
    {
    const FAKEVAR *pLoggerVar=m_pLogger->GetVar(m_nIndex); // get the logger variable
    rFormatterOut.WriteValue(pLoggerVar->dMinOccured);
    rFormatterOut.WriteValue(pLoggerVar->dAvg);
    rFormatterOut.WriteValue(pLoggerVar->dMaxOccured);
    }
  }


//--------------------------------------------------------------------------
// OnSetValue()            sets string value
// ------------
// Input: strSet = value to be set
// Return: -

/*virtual*/ void CMyVarRef::OnSetValue (const std::string &strSet) const
  {
  if(IsMailbox())
    {
    FAKEMAILBOX *pLoggerMailbox=m_pLogger->GetMailbox(m_nIndex);
    if(pLoggerMailbox->nDataType==CMyFakeLogger::DATATYPE_STRING)
      {
      strncpy(pLoggerMailbox->strValue,strSet.c_str(),sizeof(pLoggerMailbox->strValue)-1);
      std::cout<<"Client wrote string \""<<strSet <<"\" to mailbox "<<m_strName<<"\n";
      }
    }
  else
    {
    assert(false); // fake logger does not support string values
    }
  }


//--------------------------------------------------------------------------
// OnSetValue()            sets numeric value
// ------------
// Input: dSet = value to be set
// Return: -

/*virtual*/ void CMyVarRef::OnSetValue (double dSet) const
  {
  assert(m_pLogger); // do we have initialized the location of the real variable
  assert(m_nIndex>=0);

  if(!m_pLogger->LockVars())
    {
    assert(false); // maybe a deadlock? could not lock the variables
    return;
    }
  if(IsMailbox())
    {
    FAKEMAILBOX *pLoggerMailbox=m_pLogger->GetMailbox(m_nIndex);
    pLoggerMailbox->nDataType=CMyFakeLogger::DATATYPE_DOUBLE;
      pLoggerMailbox->dValue=dSet;
    }
  else
    {
    FAKEVAR *pLoggerVar=m_pLogger->GetVar(m_nIndex); // get the logger variable
    pLoggerVar->dValue=dSet;
    }
  m_pLogger->UnlockVars();
  }


//--------------------------------------------------------------------------
// OnSetValue()            sets boolean value
// ------------
// Input: bSet = value to be set
// Return: 

/*virtual*/ void CMyVarRef::OnSetValue (bool bSet) const
  {
  assert(m_pLogger); // do we have initialized the location of the real variable
  assert(m_nIndex>=0);

  if(!m_pLogger->LockVars())
    {
    assert(false); // maybe a deadlock? could not lock the variables
    return;
    }
  if(IsMailbox())
    {
    FAKEMAILBOX *pLoggerMailbox=m_pLogger->GetMailbox(m_nIndex);
    if(pLoggerMailbox->nDataType==CMyFakeLogger::DATATYPE_BOOL)
      pLoggerMailbox->bValue=bSet;
    }
  else
    {
    FAKEVAR *pLoggerVar=m_pLogger->GetVar(m_nIndex); // get the logger variable
    pLoggerVar->dValue=bSet; // logger doesnt support bool, so write 0. or 1. to the double value
    }
  m_pLogger->UnlockVars();
  }



//--------------------------------------------------------------------------
// OnGetMeta()             called when variables meta data are needed by the interface
// -----------
// Input: rMetaData = [out] structure to put meta data into
// Return: -

/*virtual*/ void CMyVarRef::OnGetMeta (CMeta &rMetaData) const
  {
  if(!IsMailbox())
    {
    const FAKEVAR *pLoggerVar=m_pLogger->GetVar(m_nIndex); // get the variable of the logger
    rMetaData.SetDispName(pLoggerVar->szDisplayName);
    rMetaData.SetComment(pLoggerVar->szComment);
    rMetaData.SetUnit(pLoggerVar->szUnit);
    rMetaData.SetRange(pLoggerVar->dRangeMin,pLoggerVar->dRangeMax);
    rMetaData.SetFactorOffset(pLoggerVar->dFactor,pLoggerVar->dOffset);
    rMetaData.SetFractDigits(pLoggerVar->nFractionalDigits);
    rMetaData.SetThresholds(pLoggerVar->dThresholds);
    rMetaData.SetTags(pLoggerVar->szTags);
    
    if(pLoggerVar->pszImageUrl)
      rMetaData.SetObject(pLoggerVar->pszImageUrl,"image/jpeg");

    // todo: if your logger supports the other members of CVarRef::META, initialize them
    }
  }



















//--------------------------------------------------------------------------
// CMyLoggerInterface()    Constructor of CMyLoggerInterface
// --------------------
// Input: pLogger = [in] pointer to logger to connect to
//        addrHttpServer = [in] address identifying on which adapter the http server works on
//                         Only the sin_port and sin_addr is used. Ohter members are ignored
//                         sin_addr can be set to INADDR_ANY if only one network adapter is present or if the http server accepts connections on all adapters
//                         ATTENTION: Use htons() for setting the server port
// Return: 

CMyLoggerInterface::CMyLoggerInterface (CMyFakeLogger *pLogger, const struct sockaddr_in &addrHttpServer)
  {
  m_addrHttpServer=addrHttpServer;
  m_pLogger=pLogger;

  // for this demo, start one discovery server
  CMyDiscoveryServer *pSvrDiscovery=new CMyDiscoveryServer(this);
  m_poolDiscoveryServers.AddAndStartServer(pSvrDiscovery);
  }


//--------------------------------------------------------------------------
// ~CMyLoggerInterface()   Constructor of ~CMyLoggerInterface
// ---------------------
// Input: -
// Return: 

/*virtual*/ CMyLoggerInterface::~CMyLoggerInterface ()
  {
  m_pLogger->StopPaceThread(); // end the pace thread (also ends generating trend data wich would accss the dead interface variables)
  }


//--------------------------------------------------------------------------
// OnGetVariableList()     called to retrieve the available variable catalogue
// -----------------
// Input: pVarList = pointer to an empty variable list where variable reference objects
//                   can be added. The objects must not be deleted elsewhere. they are
//                   deleted by the logger interface
// Return: true on success, false on error

/*virtual*/ bool CMyLoggerInterface::OnGetVariableList (std::list<CVarRef *> *pList) const
  {
  int nVar;
  int nVarCount=m_pLogger->GetVarCount();
  for(nVar=0;nVar<nVarCount;nVar++)
    {
    FAKEVAR *pLoggerVar=m_pLogger->GetVar(nVar); // get the variable of the logger

    // compose a variable for the interface
    CMyVarRef *pVarNew=new CMyVarRef(pLoggerVar->szName);
    pLoggerVar->pVarInterface=pVarNew;
    pVarNew->SetLocation(m_pLogger,false,nVar); // tell the var reference where to find the real variable

    // put the variable reference to the list
    pList->push_back(pVarNew);
    }

  return true;
  }


//--------------------------------------------------------------------------
// OnGetMailboxList()     called to retrieve the available mailbox catalogue
// -----------------
// Input: pVarList = pointer to an empty variable list where mailbox reference objects
//                   can be added. The objects must not be deleted elsewhere. They are
//                   deleted by the logger interface
// Return: true on success, false on error

/*virtual*/ bool CMyLoggerInterface::OnGetMailboxList (std::list<CVarRef *> *pList) const
  {
  int nMailbox; // mailbox index
  for(nMailbox=0;nMailbox<CMyFakeLogger::MAILBOX_COUNT;nMailbox++)
    {
    const FAKEMAILBOX *pLoggerMailbox=m_pLogger->GetMailbox(nMailbox); // get the mailbox of the logger

    // compose a mailbox for the interface
    CMyVarRef *pVarNew=new CMyVarRef(pLoggerMailbox->strName);
    pVarNew->SetLocation(m_pLogger,true,nMailbox); // tell the var reference where to find the real mailbox

    // put the variable reference to the list
    pList->push_back(pVarNew);
    }  
  return true;
  }



//--------------------------------------------------------------------------
// OnClientEvent()         called when an event from a client has been received
// ---------------         do not call the default implementation in yur derived function
// Input: strSender = sender address identification
//        tmSent = time at the senders clock in local time
//        strEventType = type of event
//        strParam = general purpose string parameter
//        dParam1 = general purpose numeric param
//        dParam2 = general purpose numeric param
// Return: true if handled. no further dispatching is performed
//         false if not handled, event will be dispatched to all clients

/*virtual*/ bool CMyLoggerInterface::OnClientEvent (int nSender, time_t tmSent, const std::string &strEventType, const std::string &strParam, double dParam1, double dParam2)
  {
  // test for start of data transfer
  if(!strEventType.compare("Custom_StartDataTransfer"))
    {
    // parse the string param since the route and storage type are coded there as JSON
    std::string strRoute;
    std::string strStorage;
    CJsonParser parseEventInfo(strParam.c_str());
    for(++parseEventInfo;!parseEventInfo.IsDone();++parseEventInfo)
      {
      parseEventInfo.ExtractValue("Route",&strRoute);
      parseEventInfo.ExtractValue("Storage",&strStorage);
      parseEventInfo.SkipItem();
      }
    m_pLogger->StartDataTransfer(strRoute.c_str(),strStorage.c_str());
    }

  // test for alert confirmation
  if(!strEventType.compare(ABK_CLIENTEVENT_ALERT_CONFIRM))
    {
    std::string strAlertClass;
    bool bClassDecoded=false;
    int nSeverity;
    bool bSeverityDecoded=false;
    int nCount;
    bool bCountDecoded=false;
    bool bSuppressed; // true if automatically generated confirmation
    bool bSuppressedDecoded=false;
    bool bTimeout; // true if alert confirmation was timed-out (user did not respond and the alert window closed automatically)
    bool bTimeoutDecoded=false;
    bool bPermanentSupprByUser=false;
    bool bPermanentSupprByUserDecoded=false;
    CJsonParser parseEventInfo(strParam.c_str());
    for(++parseEventInfo;!parseEventInfo.IsDone();++parseEventInfo)
      {
      bClassDecoded                 |=parseEventInfo.ExtractValue(ABK_ALERTCONFIRM_CLASS,&strAlertClass);
      bSeverityDecoded              |=parseEventInfo.ExtractValue(ABK_ALERTCONFIRM_SEVERITY,&nSeverity);
      bCountDecoded                 |=parseEventInfo.ExtractValue(ABK_ALERTCONFIRM_COUNT,&nCount);
      bSuppressedDecoded            |=parseEventInfo.ExtractValue(ABK_ALERTCONFIRM_SUPPRESSED,&bSuppressed);
      bTimeoutDecoded               |=parseEventInfo.ExtractValue(ABK_ALERTCONFIRM_TIMEOUT,&bTimeout);
      bPermanentSupprByUserDecoded  |=parseEventInfo.ExtractValue(ABK_ALERTCONFIRM_PERMASUPPRBYUSER,&bPermanentSupprByUser);
      parseEventInfo.SkipItem(); // skip any unexpected object or array
      }
    std::cout<<"Alert confirmation recieved  ";
    if(bClassDecoded)
      std::cout<<"Class: "<<strAlertClass<<"  ";
    if(bSeverityDecoded)
      std::cout<<"Severity: "<<nSeverity<<"  ";
    if(bCountDecoded)
      std::cout<<"Count: "<<nCount<<"  ";
    if(bSuppressedDecoded)
      std::cout<<"Suppressed: "<<(bSuppressed?"true":"false")<<"  ";
    if(bTimeoutDecoded)
      std::cout<<"Timeout: "<<(bTimeout?"true":"false")<<"  ";
    if(bPermanentSupprByUserDecoded)
      std::cout<<"Permanently suppressed: "<<(bPermanentSupprByUser?"true":"false")<<"  ";
    std::cout<<"\n";
    }

  // test for messagebox confirmation
  if(!strEventType.compare(ABK_CLIENTEVENT_MSGBOX_CONFIRM))
    {
    std::cout<<"MessageBox confirmation recieved. ID="<<dParam1<<" Button="<<strParam;
    std::cout<<"\n";
    }

  // test for Button event
  else if(!strEventType.compare(ABK_CLIENTEVENT_BUTTON))
    {
    std::cout<<"Button event, StringParam = \""<<strParam<<"\" Param1 = "<<dParam1<<" Param2 = "<<dParam2;
    std::cout<<"\n";
    }

  // test for AudioRecordingReject client event
  else if(!strEventType.compare(ABK_CLIENTEVENT_AUDIOREC_REJECT)) // the user rejected the audio recording
    {
    std::cout<<"User rejected audio recording with ID "<<(int)dParam1;
    std::cout<<"\n";
    }

  // test for Xxx client event
  //else if(!strEventType.compare(ABK_CLIENTEVENT_Xxx))
  //  {
  //  }


  return false;    
  }



//--------------------------------------------------------------------------
// OnGetLoggerFwVersion()  called to retrieve the logger firmware revision
// ----------------------
// Input: strReturn = string to return the info
// Return: true if version info was set to strReturn
//         false if no version info applicable

/*virtual*/ bool CMyLoggerInterface::OnGetLoggerFwVersion (std::string &strReturn) const
  {
  strReturn=m_pLogger->GetVersion();
  return true;
  }



//--------------------------------------------------------------------------
// OnGetLoggerHwVersion()  called to retrieve the logger hardware revision
// ----------------------
// Input: strReturn = string to return the info
// Return: true if version info was set to strReturn
//         false if no version info applicable

/*virtual*/ bool CMyLoggerInterface::OnGetLoggerHwVersion (std::string &strReturn) const
  {
  return false; // this fake has no hardware version information
  }




//--------------------------------------------------------------------------
// OnGetLoggerName()       called to retrieve the name of the logger instance (e.g. PowerTrain-Logger)
// -----------------
// Input: strReturn = string to return the info
// Return: true if version info was set to strReturn
//         false if no version info applicable

/*virtual*/ bool CMyLoggerInterface::OnGetLoggerName (std::string &strReturn) const
  {
  strReturn="Logger on your PC";
  return true;
  }




//--------------------------------------------------------------------------
// OnGetLoggerType()       called to retrieve the type of the logger, e.g. Logger2000
// -----------------
// Input: strReturn = string to return the info
// Return: true if type info was set to strReturn
//         false if no type info applicable

/*virtual*/ bool CMyLoggerInterface::OnGetLoggerType (std::string &strReturn) const
  {
  strReturn="MyFakeLogger";
  return true;
  }



//--------------------------------------------------------------------------
// OnGetLoggerSerial()     called to retrieve the serial number of the logger, e.g. 0001
// -------------------
// Input: strReturn = 
// Return: true if serial info was set to strReturn
//         false if no serial info applicable

/*virtual*/ bool CMyLoggerInterface::OnGetLoggerSerial (std::string &strReturn) const
  {
  strReturn="0123";
  return true;
  }


//--------------------------------------------------------------------------
// OnGetLoggerDescriptionUrl() called to retrieve the description file URL type of the logger, e.g. /index.html
// ---------------------------
// Input: strReturn = return url of description file, shall be an absolute location description
// Return: true if info was set to strReturn
//         false if no info applicable

/*virtual*/ bool CMyLoggerInterface::OnGetLoggerDescriptionUrl (std::string &strReturn) const
  {
  strReturn="/index.html";
  return true;
  }




/** Returns list of available firmware fot a specific client
@note This virtual method gets called when the available client firmware information shall be gathered.
 In derived classed, there is no need to call the base class implementation.
@param pszClientClass class name of client requesting the firmware info
@param pszClientType type of client, typ. manufacturer and type merged string
@param lstGet list to return all available firmware images.
 There is no need to empty the container since When this method is called, the list is guaranteed to be empty
@return true if handled, false if not handled
*/
/*virtual*/ bool CMyLoggerInterface::OnGetClientFirmwareInfo (const char *pszClientClass, const char *pszClientType, std::list<CClientFirmware> &lstGet) const /*override*/
{
  std::string strLocalPath = LOCDIR_CLIENTFIRMWARE; // where we store the firmware files
  strLocalPath.append (pszClientClass);
  strLocalPath.append ("_");
  strLocalPath.append (pszClientType); // now we have the local path without file extension, e.g. "c:\abk\client_firmware\Display_MyTrionics_SuperDisplay3000"

  CString strFind = CA2T (strLocalPath.c_str (), CP_UTF8);
  strFind.Append (_T (".*")); // wildcard for the file extension
  WIN32_FIND_DATA ffd;
  HANDLE hFind = FindFirstFile (strFind, &ffd);
  if (hFind != INVALID_HANDLE_VALUE)
  {
    WIN32_FIND_DATA ffdAdvance;
    if (!FindNextFile (hFind, &ffdAdvance)) // is there another file? It would be ambiguous
    {
      CClientFirmware cfw;
      std::string strFilenameA = CT2A (ffd.cFileName, CP_UTF8); // file name in UTF-8
      std::string strLocalA = LOCDIR_CLIENTFIRMWARE;
      strLocalA.append (strFilenameA);
      cfw.m_strMd5 = Md5FromFile (strLocalA.c_str ());
      if (!cfw.m_strMd5.empty ()) // if file is present and a valid MD5 could be created..
      {
        cfw.m_strUrl = ABK_SERVICE_CLIENTFIRMWARE;
        cfw.m_strUrl.append ("/");
        cfw.m_strUrl.append (strFilenameA);
        CString strVersion = GetClientFwVersionString (CA2T (strLocalA.c_str (), CP_UTF8));
        cfw.m_strVersion = CT2A (strVersion);
        if (cfw.m_strVersion.empty ())
          cfw.m_strVersion = "[unknown]"; // with this fake info, we do not support version info
        lstGet.push_back (cfw); // append to list
      }
    }
    FindClose (hFind);
  }
  return true; // true: we provided firmware info (even if the list was left empty)
}




//--------------------------------------------------------------------------
// OnGetClientConfigInfo() returns client config file info
// -----------------------
// Input: strClientClass = class of client requesting the firmware info
//                         must be one of the ABK_CLASSNAME_xxx definitions, e.g. "Display"
//        strClientType = type of client, typ. manifacturer and type merged string
//                        e.g. "MyTrionics_SuperDisplay3000"
//        strClientSerial = serial number the client specified, e.g. "1234"
//        cgfGet = [out] ref to return client configuration file info
//                 if no config file available, set the m_strUrl member to an
//                 empty string
// Return: true if sucessfully handled (even if no config file available and
//              the m_strUrl member was emptied)
//         false otherwise (on error or if feature is not supported)

/*virtual*/ bool CMyLoggerInterface::OnGetClientConfigInfo (const char *pszClientClass, const char *pszClientType, const char *pszClientSerial, CClientConfig &cfgGet) const
  {
  // demo filtering the client class and type and then returning a fake info


  cfgGet.m_strUrl.clear(); // in case we find no file, clear the result path

  std::string strLocalPath=LOCDIR_CLIENTCONFIG;
  strLocalPath.append(pszClientClass);
  strLocalPath.append("_");
  strLocalPath.append(pszClientType); // now we have the local path without file extension, e.g. "c:\abk\client_config\Display_MyTrionics_SuperDisplay3000"

  CString strFind=CA2T(strLocalPath.c_str(),CP_UTF8);
  strFind.Append(_T(".*")); // wildcard for the file extension
  WIN32_FIND_DATA ffd;
  HANDLE hFind=FindFirstFile(strFind,&ffd);
  if(hFind!=INVALID_HANDLE_VALUE)
    {
    WIN32_FIND_DATA ffdAdvance;
    if(!FindNextFile(hFind,&ffdAdvance)) // is there another file? It would be ambiguous
      {
      std::string strFilenameA=CT2A(ffd.cFileName,CP_UTF8); // file name in UTF-8
      std::string strLocalA=LOCDIR_CLIENTCONFIG;
      strLocalA.append(strFilenameA);
      cfgGet.m_strMd5=Md5FromFile(strLocalA.c_str());
      if(!cfgGet.m_strMd5.empty()) // if file is present and a valid MD5 could be created..
        {
        std::string strUrlA=ABK_SERVICE_CLIENTCONFIG;
        strUrlA.append("/");
        strUrlA.append(strFilenameA);
        cfgGet.m_strUrl=strUrlA; // .. return the url
        }
      }
    FindClose(hFind);
    }
  return true; // true: we support the client-config feature
  }



//--------------------------------------------------------------------------
// OnGetStorageInfo()      called to retrieve the total and available storage space (storage for logging data)
// ------------------
// Input: rTotal = [out] ref to return total storage capacity in terms of bytes
//        rFree = [out] ref to return available storage capacity in terms of bytes
// Return: true if version info was set to strReturn
//         false if no version info applicable

/*virtual*/ bool CMyLoggerInterface::OnGetStorageInfo (unsigned long long &rTotal, unsigned long long &rFree) const
  {
  rTotal=m_pLogger->GetStorageTotal(); // get it in MByte
  rTotal<<=20; // convert from MB to byte
  rFree=m_pLogger->GetStorageAvailable();
  rFree<<=20;
  return true;
  }


//--------------------------------------------------------------------------
// Identify()              tells a client to identify itself
// ----------
// Input: nClient = index of client. 0 means first client, 1 second one etc.
// Return: -

void CMyLoggerInterface::Identify (int nClient)
  {
  std::list <CLoggerInterface::CClientUid> lstClients;
  EnumSessions(&lstClients);
  int nEnum=0;
  std::list <CLoggerInterface::CClientUid>::iterator iterClients;
  for(iterClients=lstClients.begin();iterClients!=lstClients.end();++iterClients)
    {
    if(nEnum==nClient)
      {
      IdentifyClient(*iterClients,"Identification");
      break;
      }
    nEnum++;
    }
  }


//--------------------------------------------------------------------------
// OpenForm()              opens a demo form
// ----------
// Input: strFormName = name of the form to be opened
// Return: -

void CMyLoggerInterface::OpenForm (const char *pszFormName)
  {
  //for(int i=0;i<100;i++)
  //  {
    RequestOpenForm(pszFormName);
    //Sleep(1);
    //}
  }


//--------------------------------------------------------------------------
// CloseForm()             closes the demo form
// -----------
// Input: strFormName = name of form to be closed
// Return: 

void CMyLoggerInterface::CloseForm (const char *pszFormName)
  {
  RequestCloseForm(pszFormName);  
  }


//--------------------------------------------------------------------------
// GetHttpConnectionInfo() returns how the http server can be connected
// -----------------------
// Input: addrHttpServer = [out] address identifying on which adapter the http server works on
//                         Only the sin_port and sin_addr is valid.
//                         sin_addr may identify to INADDR_ANY if the http server accepts connections on all adapters
// Return: -

void CMyLoggerInterface::GetHttpConnectionInfo (struct sockaddr_in &addrHttpServer) const
  {
  addrHttpServer=m_addrHttpServer;
  }




//--------------------------------------------------------------------------
// PrintAllConnectedClients() prints all connected clients to the console
// --------------------------
// Input: -
// Return: 

void CMyLoggerInterface::PrintAllConnectedClients (void)
  {
  std::list <CLoggerInterface::CClientUid> lstClients;
  EnumSessions(&lstClients); // get the list of clients (sessions)
  std::list <CLoggerInterface::CClientUid>::iterator iterClients;
  for(iterClients=lstClients.begin();iterClients!=lstClients.end();++iterClients)
    cout<<"  Client: addr="<<iterClients->m_strAddress<<", cls="<<iterClients->m_strClass<<", type="<<iterClients->m_strType<<", ser="<<iterClients->m_strSerial<<""<<endl;
  }






//--------------------------------------------------------------------------
// OnClientConnected()     called to notify the server about client connection
// -------------------
// Input: strClass = class of the client requesting a session
//        strType = type of the client
//        strSerial = serial number of the client, empty if not specified
//        strFwVersion = firmware version of the client, empty if not specified
//        strHwVersion = hardware version of the client, empty if not specified
//        nSessionId = id of the new sesson
// Return: -

/*virtual*/ void CMyLoggerInterface::OnClientConnected (const char *pszClass, const char *pszType, const char *pszSerial, const char *pszFwVersion, const char *pszHwVersion, int nSessionId)
  {
  cout<<"A client connected ("<<pszClass<<", "<<pszType<<", S/N: " << pszSerial << "). Session Id: "<<nSessionId<<"."<<endl;
  // note: you can store the information to a log file or a data base instead
  }




//--------------------------------------------------------------------------
// OnFormGet()             renders a form. return false if not rendered
// -----------
// Input: strFormName = name of the form to be retrieved
//        jfForm = JSON jormatter to render the form into
//        strErr = for returnung any error message
// Return: true if the form was rendered, false on error

/*virtual*/ bool CMyLoggerInterface::OnFormGet (const char *pszFormName, CJsonFormatter &jfForm, std::stringstream &strErr) const
  {
//#define MAKEFORMBYRAWJSON
#if (defined MAKEFORMBYRAWJSON)
  class CJsonFormatterX : public CJsonFormatter
    {
    public:
    void SetRawString (char *pszRawString)
      {
      m_strFormat=std::stringstream();
      m_strFormat<<pszRawString;
      }
    };

  CJsonFormatterX &jfFormX=(CJsonFormatterX &)jfForm;
  jfFormX.SetRawString("{\"Caption\":\"Driver\\/Shift\\/Track\",\"Persistence\":35000,\"Controls\":[{\"Type\":\"Input\",\"Name\":\"ctrlTrackInputTxt\",\"Caption\":\"Track\",\"InitialValue\":\"Please select a track.\",\"MaxLen\":23,\"ReadOnly\":true,\"Password\":false,\"Updateable\":false},{\"Type\":\"List\",\"Name\":\"ctrlTrackList\",\"Caption\":\"choices\",\"InitialValue\":-1,\"Updateable\":false,\"Options\":[\"Track1\",\"Track2\",\"Track3\"]},{\"Type\":\"Input\",\"Name\":\"ctrlDriverInputTxt\",\"Caption\":\"Driver\",\"InitialValue\":\"Please select a driver.\",\"MaxLen\":24,\"ReadOnly\":true,\"Password\":false,\"Updateable\":false},{\"Type\":\"List\",\"Name\":\"ctrlDriverList\",\"Caption\":\"choices\",\"InitialValue\":-1,\"Updateable\":false,\"Options\":[\"Driver1\",\"Driver2\",\"Driver3\"]},{\"Type\":\"Button\",\"Name\":\"ctrlDriverShiftTrackButton\",\"Caption\":\"OK\",\"Submit\":true,\"Cancel\":false,\"Updateable\":false}]}");
#else

  // compose a demo form
  char cRandomOption[100];
  int nRandomVal=rand();
  sprintf(cRandomOption,"Option 1 [%d]",nRandomVal);
  std::list<std::string> lstOptions;
  lstOptions.push_back(std::string(cRandomOption));
  lstOptions.push_back(std::string("Option2"));
  lstOptions.push_back(std::string("Option3"));
  lstOptions.push_back(std::string("Option4"));

  jfForm.WriteValue(ABK_RSP_FORMS_CAPTION,"Demo-Form"); // "Caption" : "Demo-Form",
  jfForm.WriteValue(ABK_RSP_FORMS_PERSISTENCE,5000); // "Persistence" : 5000,
  if(1)
    {
    char cRandomText[100];
    sprintf(cRandomText,"bitte eingeben [%d]",nRandomVal);
    CJsonStreamArray jaControls(&jfForm,ABK_RSP_FORMS_CONTROLS); // "Controls": [

    CLoggerInterface::FormWriteInput   (jaControls,"ctrlInput1", "Eingabe",cRandomText/*"bitte eingeben"*/,-1,false,false,true);
    CLoggerInterface::FormWriteInput   (jaControls,"ctrlInput2", "Passwort","",-1,false,true);
    CLoggerInterface::FormWriteInput   (jaControls,"ctrlInput3", "Nur Lesen","read-only",-1,true);
    CLoggerInterface::FormWriteCheckbox(jaControls,"ctrlCheck1", "Check",true,true); // updateable check box
    CLoggerInterface::FormWriteList    (jaControls,"ctrlList1",  "Auswahl",0,lstOptions,true);
    CLoggerInterface::FormWriteButton  (jaControls,"ctrlButton1","OK",true);
    } // ]
#endif

  return true;
  }




//--------------------------------------------------------------------------
// OnFormPut()             called when client returned a forms result. return false if error form
// -----------
// Input: strFormName = name of the form the data in jpForm are associated to
//        jpForm = json parser containing the form data
//        strErr = for returning any error message
// Return: true if data was accepted, false if not (e.g. wrong form name or invalid content in jpForm)

/*virtual*/ bool CMyLoggerInterface::OnFormPut (const char *pszFormName, CJsonParser &jpForm, std::stringstream &strErr)
  {
  cout<<"Returned data from "<<pszFormName<<":"<<endl;
  for(;!jpForm.IsDone();++jpForm)
    {
    std::string strData;
    double dData;
    bool bData;
    std::string strElement;
    jpForm.GetName(strElement);
    cout<<"  "<<strElement<<": ";
    if(jpForm.ExtractValue(&strData))
      cout<<strData.c_str();
    if(jpForm.ExtractValue(&dData))
      cout<<dData;
    if(jpForm.ExtractValue(&bData))
      cout<<bData;
    // todo: test for date and occassionally output it
    cout<<endl;
    }
  return true;
  }




//--------------------------------------------------------------------------
// OnSessionEventPollEstablished() called to notify about new client session established
// -------------------------------
// Input: pNewSession = for this session, the first event long-polling came from the client
// Return: <tbd>

/*virtual*/ bool CMyLoggerInterface::OnSessionEventPollEstablished (CSession* pNewSession)
  {
  return false;
  }






//--------------------------------------------------------------------------
// Md5FromFile()           generates MD5 from file
// -------------
// Input: strFilePath = full path of file the MD5 shall be created for
// Return: md5 hash string on success, empty string on error

/*static*/ std::string CMyLoggerInterface::Md5FromFile (const char *pszFilePath)
  {
#define MD5LEN 16 // number of byte an MD5 has
#define NIPPLESPERBYTE 2

  std::string strResult;
  FILE *pFile=fopen(pszFilePath,"rb");
  if(pFile)
    {
    HCRYPTPROV hProv=NULL; // crypto provider
    if(CryptAcquireContext(&hProv,NULL,NULL,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT))
      {
      HCRYPTHASH hHash=NULL;
      if(CryptCreateHash(hProv,CALG_MD5,0,0,&hHash))
        {
        BYTE cReadBuffer[1024];
        for(;;)
          {
          DWORD dwRead=fread(cReadBuffer,sizeof(BYTE),_countof(cReadBuffer),pFile); // read from file
          if(dwRead==0) // if at end of file
            {
            DWORD dwHashLen=MD5LEN;
            BYTE cHashBuffer[MD5LEN];
            if(CryptGetHashParam(hHash,HP_HASHVAL,cHashBuffer,&dwHashLen,0)) // get the generated hash
              {
              char cHashString[MD5LEN*NIPPLESPERBYTE+1];
              for(DWORD dwByte=0;dwByte<dwHashLen;dwByte++)
                sprintf(cHashString+NIPPLESPERBYTE*dwByte,"%02x",((int)cHashBuffer[dwByte])&0x0ff);
              strResult.assign(cHashString);
              }
            break;
            }
          if(!CryptHashData(hHash,cReadBuffer,dwRead,0)) // feed data to hash engine
            break;
          }
        CryptDestroyHash(hHash);
        }
      CryptReleaseContext(hProv, 0);
      }

    fclose(pFile);
    }
  return strResult;
  }




//--------------------------------------------------------------------------
// OnLogAdded()            notifies that the log queue has got new entities. may be called in any thread context
// ------------
// Input: -
// Return: 

/*virtual*/ void CMyLoggerInterface::OnLogAdded (void)
  {
  LOGSEVERITY nSeverity;
  std::string strError;
  for(;;)
    {
    BOOL bPopSuccess=PopLog(nSeverity,strError); // get one log message
    if(!bPopSuccess) // if the log was empty, ready with popping messages
      break;

    if(1)
      {
      CAbkSingleLock guard(&m_mutexDumpLog,true,1000); // output from different threads shall not interfer
      cout<<strError<<std::endl;
      }
    }
  }



//--------------------------------------------------------------------------
// OnAudioRecHeader()      gets called when client starts an audio recording
// ------------------
// Input: nId = general purpose ID the server wanted to be reflected by the client
//        nSamplerateHz = sample rate in Hz
//        nChannels = number of channels (1=mono, 2=stereo)
//        nBitsPerSample = number of bits per sample (8 or 16)
// Return: true if e.g. file could be created successfully. false otherwise

/*virtual*/ bool CMyLoggerInterface::OnAudioRecHeader (int nId, int nSamplerateHz, int nChannels, int nBitsPerSample) const /*override*/
  {
  return m_pLogger->OnAudioRecHeader(nId,nSamplerateHz,nChannels,nBitsPerSample);
  }



//--------------------------------------------------------------------------
// OnAudioRecData()        gets called when client has audio recording data
// ----------------
// Input: nId = general purpose ID the server wanted to be reflected by the client
// Input: pSampleData = data. Number of entities must be nSamplesPerChannel
//                      if multiple channels (n): first n data are the first samples of n channels and so on
//        nSamples = number of samples (of all channels)
// Return: 

/*virtual*/ bool CMyLoggerInterface::OnAudioRecData (int nId, const int *pSampleData, int nSamples) const /*override*/
  {
  return m_pLogger->OnAudioRecData(nId,pSampleData,nSamples);
  }



//--------------------------------------------------------------------------
// OnAudioRecFooter()      gets called when client terminates audio recording
// ------------------
// Input: nId = general purpose ID the server wanted to be reflected by the client
// Return: 

/*virtual*/ bool CMyLoggerInterface::OnAudioRecFooter (int nId) const /*override*/
  {
  return m_pLogger->OnAudioRecFooter(nId);
  }





/** retrieves version info of module
@param pszFileName file path or file name. Dll and Exe load paths are applied if no drectory is specified
@param pVersionInfo buffer recieving the version info
@param pProductName pointer to receive product name string, may be NULL
@param pFileDescription pointer to receive file description string, may be NULL
@param pLegalCopyRight pointer to receive copyright string, may be NULL
@param pCompanyName pointer to receive copany name, may be NULL
@return TRUE on success, FALSE on error
*/
/*static*/ BOOL CMyLoggerInterface::GetPeModuleVersionInfo (LPCTSTR pszFileName, __out VS_FIXEDFILEINFO* pVersionInfo, __out CString* pProductName/*=NULL*/, __out CString* pFileDescription/*=NULL*/, __out CString* pLegalCopyRight/*=NULL*/, __out CString* pCompanyName/*=NULL*/)
{
  ASSERT (pszFileName);
  BOOL bSuccess = FALSE;
  TCHAR tcFileName[_MAX_PATH];
  _tcscpy_s (tcFileName, _countof (tcFileName), pszFileName); // make copy of file name since GetFileVersionInfoSize() needs non-const input (in WINCE platforms)
  DWORD dwHandle = 0;
  DWORD dwInfoSize = GetFileVersionInfoSize (tcFileName, &dwHandle);
  if (dwInfoSize > 0)
  {
    void* pFileVersionInfo = malloc (dwInfoSize);
    if (pFileVersionInfo)
    {
      if (GetFileVersionInfo (tcFileName, NULL, dwInfoSize, pFileVersionInfo))
      {
        void* pSubInfo;
        UINT nSubLen;

        if (VerQueryValue (pFileVersionInfo, _T ("\\VarFileInfo\\Translation"), &pSubInfo, &nSubLen))
        {
          CString strEntryNamePrefix;
          if (nSubLen == sizeof (DWORD))
          {
            DWORD dwLang;
            memcpy (&dwLang, pSubInfo, nSubLen);
            strEntryNamePrefix.Format (_T ("\\StringFileInfo\\%02X%02X%02X%02X"), (dwLang & 0xff00) >> 8, dwLang & 0xff, (dwLang & 0xff000000) >> 24, (dwLang & 0xff0000) >> 16);
          }
          else // more languages defined
            strEntryNamePrefix.Format (_T ("\\StringFileInfo\\%04X04B0"), GetUserDefaultLangID ());
          CString strEntryName;

          // get copyright string
          if (pLegalCopyRight)
          {
            strEntryName.Format (_T ("%s\\LegalCopyright"), (LPCTSTR)strEntryNamePrefix);
            if (VerQueryValue (pFileVersionInfo, strEntryName.GetBuffer (), &pSubInfo, &nSubLen))
              *pLegalCopyRight = (TCHAR*)pSubInfo;
            else
              pLegalCopyRight->Empty ();
            strEntryName.ReleaseBuffer ();
          }
          // get product name string
          if (pProductName)
          {
            strEntryName.Format (_T ("%s\\ProductName"), (LPCTSTR)strEntryNamePrefix);
            if (VerQueryValue (pFileVersionInfo, strEntryName.GetBuffer (), &pSubInfo, &nSubLen))
              *pProductName = (TCHAR*)pSubInfo;
            else
              pProductName->Empty ();
            strEntryName.ReleaseBuffer ();
          }
          // get product description string
          if (pFileDescription)
          {
            strEntryName.Format (_T ("%s\\FileDescription"), (LPCTSTR)strEntryNamePrefix);
            if (VerQueryValue (pFileVersionInfo, strEntryName.GetBuffer (), &pSubInfo, &nSubLen))
              *pFileDescription = (TCHAR*)pSubInfo;
            else
              pFileDescription->Empty ();
            strEntryName.ReleaseBuffer ();
          }
          // get company name string
          if (pCompanyName)
          {
            strEntryName.Format (_T ("%s\\CompanyName"), (LPCTSTR)strEntryNamePrefix);
            if (VerQueryValue (pFileVersionInfo, strEntryName.GetBuffer (), &pSubInfo, &nSubLen))
              *pCompanyName = (TCHAR*)pSubInfo;
            else
              pCompanyName->Empty ();
            strEntryName.ReleaseBuffer ();
          }
        }

        if (VerQueryValue (pFileVersionInfo, _T ("\\"), &pSubInfo, &nSubLen))
        {
          VS_FIXEDFILEINFO* pFixedFileInfo = (VS_FIXEDFILEINFO*)pSubInfo;
          memcpy (pVersionInfo, pFixedFileInfo, sizeof (VS_FIXEDFILEINFO));
          bSuccess = TRUE;
        }
      }
      free (pFileVersionInfo);
    }
  }
  return bSuccess;
}




/** retrieves version info of module
@param hModule module handle to be queried. NULL for actual module
@param pVersionInfo buffer recieving the version info
@param pProductName pointer to receive product name string, may be NULL
@param pFileDescription pointer to receive file description string, may be NULL
@param pLegalCopyRight pointer to receive copyright string, may be NULL
@param pCompanyName pointer to receive copany name, may be NULL
@return TRUE on success, FALSE on error
*/
BOOL CMyLoggerInterface::GetPeModuleVersionInfo (_In_opt_ HMODULE hModule, __out VS_FIXEDFILEINFO* pVersionInfo, __out CString* pProductName/*=NULL*/, __out CString* pFileDescription/*=NULL*/, __out CString* pLegalCopyRight/*=NULL*/, __out CString* pCompanyName/*=NULL*/)
{
  TCHAR tcFileName[_MAX_PATH];
  if (GetModuleFileName (hModule, tcFileName, _countof (tcFileName)))
    return GetPeModuleVersionInfo (tcFileName, pVersionInfo, pProductName, pFileDescription, pLegalCopyRight, pCompanyName);
  return FALSE;
}




/** formats revision string
@param fiModule fileinfo containing the version information
@return string containing the revision information
*/
/*static*/ CString CMyLoggerInterface::GetPeModuleVersionString (VS_FIXEDFILEINFO& fiModule)
{
  CString strInfo; // result string
  strInfo.Format (_T ("%d.%d.%d.%d"), (int)(HIWORD (fiModule.dwFileVersionMS)), (int)(LOWORD (fiModule.dwFileVersionMS)), (int)(HIWORD (fiModule.dwFileVersionLS)), (int)(LOWORD (fiModule.dwFileVersionLS)));
  // to test when logger only supplies 3 number blocks: strInfo.Format (_T ("%d.%d.%d"), (int)(HIWORD (fiModule.dwFileVersionMS)), (int)(LOWORD (fiModule.dwFileVersionMS)), (int)(LOWORD (fiModule.dwFileVersionLS)));
  return strInfo;
}




/** returns string for typical about box information
@param hModule module handle to be queried. NULL for actual module
@param nLineCount number of lines to generate, -1 generate all lines
@return String containing file information in a human readable fashion
*/
/*static*/ CString CMyLoggerInterface::GetPeModuleInfoString (_In_opt_ HMODULE hModule/*=NULL*/, int nLineCount/*=-1*/)
{
  VS_FIXEDFILEINFO fiModule;
  CString strInfo; // result string
  CString strProductName;
  CString strFileDescription;
  CString strCopyright;
  CString strCompanyName;
  if (GetPeModuleVersionInfo (hModule, &fiModule, &strProductName, &strFileDescription, &strCopyright, &strCompanyName))
  {
    CString strVersion = GetPeModuleVersionString (fiModule);
    strInfo.Format (_T ("%s  Ver %s"), (LPCTSTR)strProductName, (LPCTSTR)strVersion);
    int nLines = 1;
    if (nLines < nLineCount || nLineCount < 0)
    {
      strInfo.Append (_T ("\n"));
      strInfo.Append (strCopyright);
      nLines++;
    }
    if (nLines < nLineCount || nLineCount < 0)
    {
      strInfo.Append (_T ("\n"));
      strInfo.Append (strCompanyName);
      nLines++;
    }
    if (nLines < nLineCount || nLineCount < 0)
    {
      strInfo.Append (_T ("\n"));
      strInfo.Append (strFileDescription);
      nLines++;
    }
  }
  return strInfo;
}




/** queries the version string of a client firmware
@param pszPath file path of client firmware package
@return string containing the revision information
*/
/*static*/ CString CMyLoggerInterface::GetClientFwVersionString (LPCTSTR pszPath)
{
  CString strVersion;

  if (1) // try the PE (Microsoft portable executable) type
  {
    VS_FIXEDFILEINFO fi = { 0 };
    if (GetPeModuleVersionInfo (pszPath, &fi, nullptr, nullptr, nullptr, nullptr))
      strVersion = GetPeModuleVersionString (fi);
  }

  if (strVersion.IsEmpty ()) // try to get the version out of the Microsoft module installer
  {
    MSIHANDLE hMsi=0;
    if (SUCCEEDED (MsiOpenPackage (pszPath, &hMsi)))
    {
      DWORD dwVersionLen = 100;
      UINT nQueryReult = MsiGetProductProperty (hMsi, _T ("ProductVersion"), strVersion.GetBufferSetLength (dwVersionLen + 1), &dwVersionLen);
      strVersion.ReleaseBuffer ();
      if (!SUCCEEDED (nQueryReult))
        strVersion.Empty ();
      MsiCloseHandle (hMsi);
    }
  }

  return strVersion;
}

















/*static*/ LPFN_WSARECVMSG CMyDiscoveryServer::WSARecvMsg=NULL; // pointer to WSARecvMsg() function



//--------------------------------------------------------------------------
// CMyDiscoveryServer()    Constructor of CMyDiscoveryServer
// --------------------
// Input: pLoggerIf = [in] logger interface the discovery service shall expose
// Return: 

CMyDiscoveryServer::CMyDiscoveryServer (const CMyLoggerInterface *pLoggerIf)
  {
  m_pLoggerIf=pLoggerIf;

  // obtain the WSARecvMsg() function pointer
  if(NULL==WSARecvMsg)
    {
    // https://stackoverflow.com/questions/23819812/ruby-get-incoming-address-from-udp-message
    // https://www.mkssoftware.com/docs/man3/recvmsg.3.asp
    // https://groups.google.com/forum/#!topic/comp.os.linux.development.system/7Eql8Xkef7o
    // https://stackoverflow.com/questions/17828343/wsarecvfrom-target-ip-address
    // http://simplesamples.info/Networking/WSARecvMsg.aspx
    GUID WSARecvMsg_GUID=WSAID_WSARECVMSG;
    DWORD dwNumberOfBytes;
    SOCKET sockDummy=INVALID_SOCKET;
    sockDummy=socket(AF_INET,SOCK_DGRAM,0);
    if(WSAIoctl(sockDummy,SIO_GET_EXTENSION_FUNCTION_POINTER,&WSARecvMsg_GUID,sizeof(WSARecvMsg_GUID),&WSARecvMsg,sizeof(WSARecvMsg),&dwNumberOfBytes,NULL,NULL)==SOCKET_ERROR)
      WSARecvMsg=NULL;
    closesocket(sockDummy);
    ASSERT(WSARecvMsg!=NULL);
    }

  // populate the adapter index to addresses. RESTRICTION: Since the map is created once at startup, the adapter configuration must not be changed (no adapters must be inserted nor removed)
  if(1)
    {
    DWORD dwIpAddrSize=0;
    MIB_IPADDRTABLE mibtblDummy;
    GetIpAddrTable(&mibtblDummy,&dwIpAddrSize,FALSE); // first call to get the required size
    m_pIpAddrTable=(MIB_IPADDRTABLE *)malloc(dwIpAddrSize);
    if(GetIpAddrTable(m_pIpAddrTable,&dwIpAddrSize,FALSE)!=NO_ERROR)
      {
      free(m_pIpAddrTable);
      m_pIpAddrTable=NULL;
      }
    }

  }


//--------------------------------------------------------------------------
// ~CMyDiscoveryServer()   Destructor of CMyDiscoveryServer
// ---------------------
// Input: -
// Return: -

CMyDiscoveryServer::~CMyDiscoveryServer ()
  {
  if(m_pIpAddrTable)
    free(m_pIpAddrTable);
  }

//--------------------------------------------------------------------------
// OnGetConnectionPreference() shall return preference of a connection
// ---------------------------
// Input: pszClass = class of the requester
//        pszType = type of the requester
//        pszSerial = serial number of string of the requester
// Return: true if the requester shall connect to me on ambiguity, false otherwise

/*virtual*/ bool CMyDiscoveryServer::OnGetConnectionPreference (const char *pszClass, const char *pszType, const char *pszSerial) const
  {
  return m_pLoggerIf->OnGetConnectionPreference(pszClass,pszType,pszSerial); // we let the logger interface decide 
  }




//--------------------------------------------------------------------------
// OnGetStaticServerInfo() shall provide the static server information. will be called once when the service starts
// -----------------------
// Input: rServerInfoGet = [out] get the non-dynamic changing server data
//                         m_strMyIpAddr may identify INADDR_ANY (0.0.0.0) if the http server works on all interfaces
// Return: -

/*virtual*/ void CMyDiscoveryServer::OnGetStaticServerInfo (SServerInfo &rServerInfoGet) const
  {
  sockaddr_in addrHttpAddress;
  m_pLoggerIf->GetHttpConnectionInfo(addrHttpAddress); // ip address where the http server listens for new connections. May also be INADDR_ANY ("0.0.0.0")
  rServerInfoGet.m_nPortHttp=ntohs(addrHttpAddress.sin_port);
  rServerInfoGet.m_strMyIpAddr=inet_ntoa(addrHttpAddress.sin_addr);
  rServerInfoGet.m_strServerClass=ABK_CLASSNAME_LOGGER;  // class name of server, typically "Logger" (ABK_CLASSNAME_LOGGER)
  m_pLoggerIf->OnGetLoggerType(rServerInfoGet.m_strServerType); // type of server, e.g. "MyTronicx_SuperLogger3000"
  m_pLoggerIf->OnGetLoggerName(rServerInfoGet.m_strServerName);    // name describing the role in the system e.g. "Powertrain-Logger"
  m_pLoggerIf->OnGetLoggerSerial(rServerInfoGet.m_strServerSerial);  // serial number/string of the logger
  m_pLoggerIf->OnGetLoggerDescriptionUrl(rServerInfoGet.m_strDescUrl);       // url with description of the server (optional) , e.g. "/index.html"
  }




//--------------------------------------------------------------------------
// OnSocketBind()          a socket can be bound
// --------------
// Input: nListenPort = port where to listen for the UDP broadcasts
//        pszHttpIpAddress = ip address of the http server. This may also be 0.0.0.0 (INADDR_ANY) if the http server listens on all interfaces
//                           Note: this info will not be placed into the answer
// Return: true on success, false on error

/*virtual*/ bool CMyDiscoveryServer::OnSocketBind (int nListenPort, const char *pszHttpIpAddress)
  {
  assert(nListenPort==ABK_ENUM_PORT); // why shall we listen on a server port different from openABK speicfication ??
  bool bSuccess=false;
  int nEnable=1;
  m_sockRTx=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
  setsockopt(m_sockRTx,IPPROTO_IP,IP_RECVIF,(char*)&nEnable,sizeof(nEnable)); // obtain additional information with WSARecvMsg() or recvmsg()
  setsockopt(m_sockRTx,SOL_SOCKET,SO_REUSEADDR,(char*)&nEnable,sizeof(nEnable)); // allow reusing the address
  sockaddr_in sadrRx;
  sadrRx.sin_family = AF_INET;
  sadrRx.sin_addr.s_addr =inet_addr(pszHttpIpAddress); // bind the socket to the same adapter as the http server
  sadrRx.sin_port = htons(nListenPort);
  if(bind(m_sockRTx,(sockaddr*) &sadrRx, sizeof(sadrRx)) >= 0)
    bSuccess=true;
  return bSuccess;
  }



//--------------------------------------------------------------------------
// OnSocketRxRequest()          recieve request
// --------------
// Input: pRxBuf = [out] buffer where to place the rx data
//        nBufLen = [in] length of buffer in bytes pointed to by pRxBuf
//        strLocalIpOfRequest [out] =  local address where the listening socket recieved a discovery request
// Return: true if recieved a request or if timed-out
//         false if error occured (typically due to a socket close)

/*virtual*/ bool CMyDiscoveryServer::OnSocketRxRequest (char *pRxBuf, size_t nBufLen, std::string &strLocalIpOfRequest) /*override*/
  {
  // on other platforms, you may use recvmsg() instead of WSARecvMsg()
  // If the target system has only one interface, you may use recvfrom() and populate strLocalIpOfRequest with the adapters address, see also AbkGetOwnIpAddress().
  bool bSuccess=false;
  if(WSARecvMsg && m_pIpAddrTable) // do only if the required prerequisites are available
    {
    ZeroMemory(pRxBuf,nBufLen);
    fd_set fds;
    struct timeval tvRx={0,250};
    FD_ZERO(&fds);
    FD_SET(m_sockRTx,&fds);
    select(m_sockRTx+1,&fds,NULL,NULL,&tvRx);
    if(FD_ISSET(m_sockRTx,&fds))
      {
      char cControlBuffer[1024];
      WSABUF wsaBuf;
      wsaBuf.buf=pRxBuf;
      wsaBuf.len=nBufLen;
      WSAMSG wsaMsg;
      wsaMsg.name=(sockaddr*)&m_sadrFrom;
      wsaMsg.namelen=sizeof(sockaddr_in);
      wsaMsg.lpBuffers=&wsaBuf;
      wsaMsg.dwBufferCount=1;
      wsaMsg.Control.len=sizeof(cControlBuffer);
      wsaMsg.Control.buf=cControlBuffer;
      wsaMsg.dwFlags=0;
      DWORD dwNumberOfBytes;
      if(WSARecvMsg(m_sockRTx,&wsaMsg,&dwNumberOfBytes,NULL,NULL)!=SOCKET_ERROR)
        {
        pRxBuf[dwNumberOfBytes]='\0'; // terminate the recieved string
        WSACMSGHDR *pCMsgHdr=NULL;
        while(pCMsgHdr=WSA_CMSG_NXTHDR(&wsaMsg,pCMsgHdr))
          {
          if(pCMsgHdr->cmsg_type==IP_RECVIF)
            {
            ULONG *pPktInfo;
            pPktInfo=(ULONG *)WSA_CMSG_DATA(pCMsgHdr);
            int nAdapterIndex=*pPktInfo; // this is the index of the adapter the UDP message came in
            //char cIfName[IF_NAMESIZE];
            //char *pIfName=if_indextoname(*pPktInfo,cIfName);
            // check for local request
            BOOL bLocalHost=FALSE;
            for(int nEntity=0;nEntity<(int)m_pIpAddrTable->dwNumEntries;++nEntity)
              {
              MIB_IPADDRROW *pEntity=&m_pIpAddrTable->table[nEntity];
              if(m_sadrFrom.sin_addr.S_un.S_addr==pEntity->dwAddr) // if the request came from one of the local adapters
                {
                bLocalHost=TRUE;
                in_addr addrInbound;
                addrInbound.S_un.S_addr=htonl(INADDR_LOOPBACK);
                strLocalIpOfRequest=inet_ntoa(addrInbound);
                bSuccess=TRUE;
                break;
                }
              }
            if(!bLocalHost)
              {
              for(int nEntity=0;nEntity<(int)m_pIpAddrTable->dwNumEntries;++nEntity)
                {
                MIB_IPADDRROW *pEntity=&m_pIpAddrTable->table[nEntity];
                if(pEntity->dwIndex==nAdapterIndex)
                  {
                  in_addr addrInbound;
                  addrInbound.S_un.S_addr=pEntity->dwAddr;
                  if(IsSamePrivateNet(m_sadrFrom.sin_addr,addrInbound))
                    strLocalIpOfRequest=inet_ntoa(addrInbound);
                  else
                    pRxBuf[0]='\0';
                  break;
                  }
                }
              }
            break;
            }
          } // while
#ifdef DISCOVERY_TRACE_RX
        if(bSuccess)
          {
          CA2T strFrom(inet_ntoa(m_sadrFrom.sin_addr));
          TRACE(_T("Discovery request from %s at %s: %s\n"),(LPCTSTR)strFrom,(LPCTSTR)CA2T(strLocalIpOfRequest.c_str(),CP_UTF8),(LPCTSTR)CA2T(pRxBuf,CP_UTF8));
          }
#endif
        }
      else
        {
	      int nErrorCode=WSAGetLastError();
	      }
      bSuccess=TRUE;
      }
    else
      bSuccess=true; // timed-out
    }
  return bSuccess;
  }



//--------------------------------------------------------------------------
// IsSamePrivateNet()             checks whether two addresses are of same private network
// -----------
// Input: addr1 = address to be compared to addr2
//        addr2 = address to be compared to addr1
// Return: TRUE if both addresses are of the same network.
//         FALSE if addresses are of different networks

BOOL CMyDiscoveryServer::IsSamePrivateNet (IN_ADDR addr1, IN_ADDR addr2)
  {
  if(addr1.S_un.S_un_b.s_b1==192 && addr1.S_un.S_un_b.s_b2==168 && addr2.S_un.S_un_b.s_b1==192 && addr2.S_un.S_un_b.s_b2==168)
    return addr1.S_un.S_un_b.s_b3==addr2.S_un.S_un_b.s_b3;
  if(addr1.S_un.S_un_b.s_b1==172 && (addr1.S_un.S_un_b.s_b2&0xe0)==0 && addr2.S_un.S_un_b.s_b1==172 && (addr2.S_un.S_un_b.s_b2&0xe0)==0)
    return addr1.S_un.S_un_b.s_b2==addr2.S_un.S_un_b.s_b2;
  if(addr1.S_un.S_un_b.s_b1==10 && addr2.S_un.S_un_b.s_b1==10)
    return TRUE;
  return FALSE;
  }


//--------------------------------------------------------------------------
// OnSocketTxResponse()          send response
// --------------
// Input: pTxBuf = [in] buffer to be sent
//        nContentLen = [in] length of content to be sent, w/o terminating \0
// Return: 

/*virtual*/ bool CMyDiscoveryServer::OnSocketTxResponse (const char *pTxBuf, size_t nContentLen)
  {
  int nAnswered=sendto(m_sockRTx,pTxBuf,nContentLen,0,(sockaddr*)&m_sadrFrom,sizeof(m_sadrFrom));
#ifdef DISCOVERY_TRACE_TX
  CA2T strTo(inet_ntoa(m_sadrFrom.sin_addr));
  TRACE(_T("Discovery answer to %s: %s\n"),(LPCTSTR)strTo,(LPCTSTR)CA2T(pTxBuf,CP_UTF8));
#endif
  return nAnswered==nContentLen;
  }

  
//--------------------------------------------------------------------------
// OnSocketShutdown()          shutdown the socket
// --------------
// Input: 
// Return: true on success

/*virtual*/ bool CMyDiscoveryServer::OnSocketShutdown (void)
  {
  return shutdown(m_sockRTx,SD_BOTH)==0;
  // return true; // successfully done nothing, we let the socket time-out
  }

  
//--------------------------------------------------------------------------
// OnSocketTidyUp()          a socket can be bound
// --------------
// Input: -
// Return: true on success

/*virtual*/ bool CMyDiscoveryServer::OnSocketTidyUp (void) // tidy-up the sockets resources
  {
  return closesocket(m_sockRTx)==0;
  }


