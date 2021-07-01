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
// Filename:    MyLoggerInterace.h
// Created:     2012-05-16 (15:19)
// Author:      D. Burger
// Description: demo interface adapting the http interface to an arbitrary data logger
//------------------------------------------------------------------------------------------------



#include "LoggerIf.h"
#include "HttpServer.h"
#include "DiscoveryServer.h"

#include <Mswsock.h>

// location this implementation stores files
#define LOCDIR_CLIENTSTATES     "c:\\abk\\client_states\\"
#define LOCDIR_CLIENTCONFIG     "c:\\abk\\client_config\\"
#define LOCDIR_CLIENTFIRMWARE   "c:\\abk\\client_firmware\\"
#define LOCDIR_AUDIOREC         "c:\\abk\\audio_rec\\" // audio recording files will be put here

using namespace Abk;

class CMyFakeLogger;
class CMyLoggerInterface;


class CMyDiscoveryServer : public CDiscoveryServer
  {
  // data members
  protected:
    const CMyLoggerInterface *m_pLoggerIf; // this discovery server gets associated with this server
    SOCKET m_sockRTx; // socket to make the UDP work
    sockaddr_in m_sadrFrom; // who has sent the last request
    static LPFN_WSARECVMSG WSARecvMsg; // pointer to WSARecvMsg() function
    MIB_IPADDRTABLE *m_pIpAddrTable; // table translating adapter index to its ipV4 address

  // construction/destruction
  public:
    CMyDiscoveryServer (const CMyLoggerInterface *pLoggerIf);
    ~CMyDiscoveryServer ();

  // attrs and methods
  public:
  
  // implementation
  protected:
    BOOL IsSamePrivateNet (IN_ADDR addr1, IN_ADDR addr2); // checks whether two addresses are of same network
  // overrides
  protected: // overrideables concerning the association with the logger interface
    virtual bool OnGetConnectionPreference (const char *pszClass, const char *pszType, const char *pszSerial) const override; // shall return preference of a connection
    virtual void OnGetStaticServerInfo (SServerInfo &rServerInfoGet) const override; // shall provide the static server information
  protected: // oerrideables concerning socket abstraction
    virtual bool OnSocketBind (int nListenPort, const char *pszHttpIpAddress) override; // a socket can be bound
    virtual bool OnSocketRxRequest (char *pRxBuf, size_t nBufLen, std::string &strLocalIpOfRequest) override; // recieve request
    virtual bool OnSocketTxResponse (const char *pTxBuf, size_t nContentLen) override; // send response
    virtual bool OnSocketShutdown (void) override; // shutdown the socket
    virtual bool OnSocketTidyUp (void) override; // tidy-up the sockets resources

  };



class CMyLoggerInterface : public CLoggerInterface
  {
  // data members
  protected:
    CMyFakeLogger *m_pLogger; // pointer to the data logger to connect to
    CAbkMutex m_mutexDumpLog; // mutex preventing the log dump getting confused
    struct sockaddr_in m_addrHttpServer; // address and port the server works on
    CDiscoveryServerPool m_poolDiscoveryServers; // pool of discovery servers

  // construction/destruction
  public:
    CMyLoggerInterface (CMyFakeLogger *pLogger, const struct sockaddr_in &addrHttpServer);
    /*virtual*/ ~CMyLoggerInterface ();

  // misc methods and properties
    void PrintAllConnectedClients (void); // prints all connected clients to the console
    void Identify (int nClient); // tells a client to identify itself
    void OpenForm (const char *pszFormName); // opens a demo form
    void CloseForm (const char *pszFormName); // closes the demo form
    void GetHttpConnectionInfo (struct sockaddr_in &addrHttpServer) const; // returns how the http server can be connected

  // implementation
  protected:
    static std::string Md5FromFile (const char *pszFilePath); // generates MD5 from file
    static BOOL GetPeModuleVersionInfo (LPCTSTR pszFileName, __out VS_FIXEDFILEINFO* pVersionInfo, __out CString* pProductName/*=NULL*/, __out CString* pFileDescription/*=NULL*/, __out CString* pLegalCopyRight/*=NULL*/, __out CString* pCompanyName/*=NULL*/);
    static BOOL GetPeModuleVersionInfo (_In_opt_ HMODULE hModule, __out VS_FIXEDFILEINFO* pVersionInfo, __out CString* pProductName/*=NULL*/, __out CString* pFileDescription/*=NULL*/, __out CString* pLegalCopyRight/*=NULL*/, __out CString* pCompanyName/*=NULL*/);
    static CString GetPeModuleVersionString (VS_FIXEDFILEINFO& fiModule); // reads version information from a Microsoft compatible module
    static CString GetPeModuleInfoString (_In_opt_ HMODULE hModule/*=NULL*/, int nLineCount/*=-1*/);
    static CString GetClientFwVersionString (LPCTSTR pszFileName); // queries the version string of a client firmware
  // overrides
  protected:
    virtual bool OnGetVariableList (std::list<CVarRef *> *pList) const override; // called to retrieve the available variable catalogue
    virtual bool OnGetMailboxList (std::list<CVarRef *> *pList) const override; // called to retrieve the available mailbox catalogue
    virtual bool OnClientEvent (int nSender, time_t tmSent, const std::string &strEventType, const std::string &strParam, double dParam1, double dParam2) override; // called when an event from a client is recieved
    virtual bool OnGetStorageInfo (unsigned long long &rTotal, unsigned long long &rFree) const override; // called to retrieve the total and available storage space
    virtual bool OnGetClientFirmwareInfo (const char *pszClientClass, const char *pszClientType, std::list<CClientFirmware> &lstGet) const override; // returns list of available firmware fot a specific client
    virtual bool OnGetClientConfigInfo (const char *pszClientClass, const char *pszClientType, const char *pszClientSerial, CClientConfig &cgfGet) const override; // returns client config file info
    virtual void OnClientConnected (const char *pszClass, const char *pszType, const char *pszSerial, const char *pszFwVersion, const char *pszHwVersion, int nSessionId) override; // called to notify the server about client connection
    virtual bool OnFormGet (const char *pszFormName, CJsonFormatter &jfForm, std::stringstream &strErr) const override; // renders a form. return false if not rendered
    virtual bool OnFormPut (const char *pszFormName, CJsonParser &jpForm, std::stringstream &strErr) override; // called when client returned a forms result. return false if error form
    virtual bool OnSessionEventPollEstablished (CSession* pNewSession) override; // called to notify about new client session established
    virtual void OnLogAdded (void) override; // notifies that the log queue has got new entities. may be called in any thread context
    virtual void OnDAQCreate (int nSessionId, const char *pszDaqName, bool &rbUseExternalTimer) {;} //set rbUseExternalTimer to true if an external timer for a daq exist (instead of the internal daq timer). 
    virtual void OnDAQCycleUpdate (int nSessionId, const char *pszDaqName, int &rnDAQCycleMs) override {;} // do not lock session list, it is already locked!
    virtual void OnDAQDispose (int nSessionId, const char *pszDaqName) override {;}

  // overrides, logger information
  public:
    virtual bool OnGetLoggerFwVersion (std::string &strReturn) const override; // called to retrieve the logger firmware revision
    virtual bool OnGetLoggerHwVersion (std::string &strReturn) const override; // called to retrieve the logger hardware revision
    virtual bool OnGetLoggerName (std::string &strReturn) const override; // called to retrieve the name of the logger instance (e.g. PowerTrain-Logger)
    virtual bool OnGetLoggerType (std::string &strReturn) const override; // called to retrieve the type of the logger, e.g. Logger2000
    virtual bool OnGetLoggerSerial (std::string &strReturn) const override; // called to retrieve the serial number of the logger, e.g. 0001
    virtual bool OnGetLoggerDescriptionUrl (std::string &strReturn) const override; // called to retrieve the description file URL type of the logger, e.g. /index.html

  // audio recording misc overrideables
  public:
    virtual bool OnAudioRecHeader (int nId, int nSamplerateHz, int nChannels, int nBitsPerSample) const override; // gets called when client starts an audio recording
    virtual bool OnAudioRecData (int nId, const int *pSampleData, int nSamples) const override; // gets called when client has audio recording data
    virtual bool OnAudioRecFooter (int nId) const override; // gets called when client terminates audio recording

  };



class CMyVarRef : public CVarRef
  {
  // data members
  protected:
    CMyFakeLogger *m_pLogger; // logger the real variable is installed
    int m_nIndex; // index of the variable within the logger
    bool m_bMailbox; // true if mailbox rather than measurement variable

  // construction/destruction
  public:
    CMyVarRef (const char *pszName);

  // operations
  public:
    void SetLocation (CMyFakeLogger *pLogger, bool bMailbox, int nIndex); // sets the location where to find the real variable

  // misc
  public:
    bool IsMailbox (void) const {return m_bMailbox;} // returns true is variable is a mailbox

  // overrides
  protected:
    virtual void OnGetMeta (CMeta &rMetaData) const override; // called when variables meta data are needed by the interface
    virtual void OnFormatValue (CJsonStreamBase &rFormatterOut) const override; // called when a variable shall be formatted
    virtual void OnFormatMAM   (CJsonStreamArray &rFormatterOut) const override; // called when a variable min-max-average shall be formatted
    virtual void OnSetValue (const std::string &strSet) const override; // sets string value
    virtual void OnSetValue (double dSet) const override; // sets numeric value
    virtual void OnSetValue (bool bSet) const override; // sets boolean value
  };





