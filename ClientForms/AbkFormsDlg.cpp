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
// Filename:    AbkFormsDlg.cpp
// Created:     2012-08-07 (11:45)
// Author:      D. Burger
// Description: Main dialog of demo app demonstrating ABK forms with MFC dialog
//------------------------------------------------------------------------------------------------


// AbkFormsDlg.cpp : implementation file
//

#include "stdafx.h"
#include "AbkForms.h"
#include "AbkFormsDlg.h"
#include "afxdialogex.h"

#include "AbkServerFinder.h"
#include "AbkClient.h"
#include "AbkServerEvent.h"
#include "DlgAbkForm.h"
#include "C:\Source\Repos\abk\ClientExample\MyClient.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif




/** returns MAC address of the first Ethernet adapter on enumeration  
@return MAX address in the notation of xx-xx-xx-xx-xx-xx
*/
CString GetMacAddress (void)
{
  static CString strResult;
  if (!strResult.IsEmpty ())
    return strResult;

#ifdef WINCE
  IP_ADAPTER_INFO info;
  memset (&info, 0, sizeof (info));
  IP_ADAPTER_INFO* pInfo = &info;
  ULONG ulSize = sizeof (IP_ADAPTER_INFO);
  DWORD dwResult = GetAdaptersInfo (&info, &ulSize);
  if (dwResult == ERROR_BUFFER_OVERFLOW) // if not sufficient space for adapter info
  {
    pInfo = reinterpret_cast<IP_ADAPTER_INFO*>(new BYTE[ulSize]);
    ASSERT (pInfo);
    memset (pInfo, 0, ulSize);
    dwResult = GetAdaptersInfo (pInfo, &ulSize);
  }
  if (dwResult == 0) // if successfully
  {
    unsigned int nSection;
    LPCTSTR pszFormat = _T ("%02x");
    for (nSection = 0; nSection < pInfo->AddressLength; nSection++)
    {
      strResult.AppendFormat (strFormat, pInfo->Address[nSection]);
      strFormat = _T ("-%02x");
    }
  }
  else
    strResult = _T ("");
  if (pInfo != &info)
    delete pInfo;
  return strResult;
#else
  TCHAR strBuffer[20];
  // unsigned char MACData[6];
  UUID uuid;
  UuidCreateSequential (&uuid);    // Ask OS to create UUID
  int nCol = 0;
  for (int i = 0; i < 6; i++)  // Bytes 2 through 7 inclusive are MAC address
  {
    int nWritten = _stprintf_s (strBuffer + nCol, sizeof (strBuffer) / sizeof (TCHAR) - nCol, _T ("%02X"), (int)uuid.Data4[i + 2]);
    if (nWritten < 0)
      return strResult;
    nCol += nWritten;
    if (i < 5)
    {
      strBuffer[nCol++] = '-';
      strBuffer[nCol] = '\0';
    }
  }
  strResult = strBuffer;
  return strResult;
#endif
}
















#define MESSAGE_FORM_OPEN   (WM_USER+1232)
#define MESSAGE_FORM_CLOSE  (WM_USER+1233)











using namespace Abk;










// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
  {
  public:
    CAboutDlg();

    // Dialog Data
    enum { IDD = IDD_ABOUTBOX };

  protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

    // Implementation
  protected:
    DECLARE_MESSAGE_MAP()
  };


BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


//--------------------------------------------------------------------------
// CAboutDlg()             Constructor of CAboutDlg
// -----------
// Input: -
// Return: 

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
  {
  }


//--------------------------------------------------------------------------
// DoDataExchange()        Called by framework to exchange and validate dialog data
// ----------------
// Input: pDX = pointer to a CDataExchange object
// Return: -

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
  {
  CDialogEx::DoDataExchange(pDX);
  }












// CAbkFormsDlg dialog

BEGIN_MESSAGE_MAP(CAbkFormsDlg, CDialogEx)
  ON_WM_SYSCOMMAND()
  ON_WM_PAINT()
  ON_WM_QUERYDRAGICON()
END_MESSAGE_MAP()



//--------------------------------------------------------------------------
// CAbkFormsDlg()          Constructor of CAbkFormsDlg
// --------------
// Input: pParent = 
// Return: 

CAbkFormsDlg::CAbkFormsDlg(CWnd* pParent /*=NULL*/)
  : CDialogEx(CAbkFormsDlg::IDD, pParent)
  , m_strStatus(_T(""))
  {
  m_client.m_pOwner=this;
  m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
  }


//--------------------------------------------------------------------------
// DoDataExchange()        Called by framework to exchange and validate dialog data
// ----------------
// Input: pDX = pointer to a CDataExchange object
// Return: -

void CAbkFormsDlg::DoDataExchange(CDataExchange* pDX)
  {
  CDialogEx::DoDataExchange(pDX);
  DDX_Text(pDX, IDC_STATUS, m_strStatus);
  }



//--------------------------------------------------------------------------
// OnInitDialog()          This method is called in response to the WM_INITDIALOG message
// --------------
// Input: -
// Return: non 0, if windows shall set the focus to the default location

BOOL CAbkFormsDlg::OnInitDialog()
  {
  CDialogEx::OnInitDialog();

  // Add "About..." menu item to system menu.

  // IDM_ABOUTBOX must be in the system command range.
  ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
  ASSERT(IDM_ABOUTBOX < 0xF000);

  CMenu* pSysMenu = GetSystemMenu(FALSE);
  if (pSysMenu != NULL)
    {
    BOOL bNameValid;
    CString strAboutMenu;
    bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
    ASSERT(bNameValid);
    if (!strAboutMenu.IsEmpty())
      {
      pSysMenu->AppendMenu(MF_SEPARATOR);
      pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
      }
    }

  // Set the icon for this dialog.  The framework does this automatically
  //  when the application's main window is not a dialog
  SetIcon(m_hIcon, TRUE);			// Set big icon
  SetIcon(m_hIcon, FALSE);		// Set small icon

  // search for appropriate server
  m_strStatus=_T("Searching for an ABK server...");
  UpdateData(FALSE);
  std::list<CAbkDiscoveredServer> lstServers;
  int nUnique; // index of unique server we can automatically connect to
  nUnique=AbkFindServers(ABK_CLASSNAME_DISPLAY,"AbkFormsDemo","1234",lstServers);
  if(nUnique>=0)
    {
    std::list<CAbkDiscoveredServer>::iterator iterServer=lstServers.begin();
    std::advance(iterServer,nUnique);
    CAbkDiscoveredServer serverUnique=*iterServer;
    CString strServerAddress=CA2T(serverUnique.m_strAddress.c_str());
    m_strStatus.Format(_T("Server %s:%d found. Please wait until server requests to open a form!"),(LPCTSTR)strServerAddress,serverUnique.m_nPortHttp);

    m_client.Create(strServerAddress,serverUnique.m_nPortHttp,&m_client.m_bufEvent,_T("Display"),_T("FormDemoWithMFC"), GetMacAddress ());

    }
  else
    {
    AfxMessageBox(_T("Es konnte kein eindeutiger Server gefunden werden"),MB_OK|MB_ICONEXCLAMATION);
    m_strStatus==_T("Failed to find a unique server to connect to.");
    }
  UpdateData(FALSE);


  return TRUE;  // return TRUE  unless you set the focus to a control
  }




//--------------------------------------------------------------------------
// OnSysCommand()          
// --------------
// Input: nID = 
//        lParam = 
// Return: 

void CAbkFormsDlg::OnSysCommand(UINT nID, LPARAM lParam)
  {
  if ((nID & 0xFFF0) == IDM_ABOUTBOX)
    {
    CAboutDlg dlgAbout;
    dlgAbout.DoModal();
    }
  else
    {
    CDialogEx::OnSysCommand(nID, lParam);
    }
  }



//--------------------------------------------------------------------------
// OnPaint()               called when Windows or an application makes a request to repaint a portion of window
// ---------
// Input: -
// Return: -

void CAbkFormsDlg::OnPaint()
  {
  if (IsIconic())
    {
    CPaintDC dc(this); // device context for painting

    SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

    // Center icon in client rectangle
    int cxIcon = GetSystemMetrics(SM_CXICON);
    int cyIcon = GetSystemMetrics(SM_CYICON);
    CRect rect;
    GetClientRect(&rect);
    int x = (rect.Width() - cxIcon + 1) / 2;
    int y = (rect.Height() - cyIcon + 1) / 2;

    // Draw the icon
    dc.DrawIcon(x, y, m_hIcon);
    }
  else
    {
    CDialogEx::OnPaint();
    }
  }



//--------------------------------------------------------------------------
// OnQueryDragIcon()       Calld for minimized window with no icon defined for its class
// -----------------
// Input: -
// Return: Handle to icon or cursor, NULL for default icon
//  the minimized window.

HCURSOR CAbkFormsDlg::OnQueryDragIcon()
  {
  return static_cast<HCURSOR>(m_hIcon);
  }



//--------------------------------------------------------------------------
// WindowProc()            window procedure receives all messages
// ------------
// Input: message = received message
//        wParam = word param, dependent on message
//        lParam = long param, dependent on message
// Return: LRESULT depends on message

LRESULT CAbkFormsDlg::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
  {
  if(message==MESSAGE_FORM_OPEN) // server requested to open a form
    {
    std::vector<Abk::CAbkClient::CFormElement> vectElements; // list for recieving the form elements
    CString strFormName;
      {
      CAbkSingleLock lockForm(&m_client.m_mutexAbk,true,1000);
      strFormName=m_client.m_strFormName;   // make thread-save copy of form name
      }
    CString strCaption; // caption of the form
    int nPersistenceMs=0;
    if(m_client.GetForm(strFormName, vectElements,strCaption,nPersistenceMs)) // request and decode the form
      {
      CDlgAbkForm dlgForm;
      dlgForm.InitModal(NULL,NULL,NULL,strFormName,strCaption,nPersistenceMs,vectElements,NULL,this);
      INT_PTR nResponse=dlgForm.DoModal();
      m_client.SendForm(strFormName,dlgForm.GetData()); // send back the filled form
      }
    }

  else if(message==MESSAGE_FORM_CLOSE) // server requested to close a form
    {
    ASSERT(FALSE); // todo implement closing a form
    }

  return CDialogEx::WindowProc(message, wParam, lParam);
  }









//--------------------------------------------------------------------------
// OnServerEvent()         called when event from server arrived
// ---------------
// Input: pEventData = data of event
// Return: pointer to place where to store the next event to

/*virtual*/ Abk::CAbkServerEvent *CMyAbkClient::OnServerEvent (Abk::CAbkServerEvent *pEventData)
  {
  CAbkServerEvent::CData dataEvent;
  if(pEventData->GetEvent(dataEvent)) // get and decode the event data
    {
    if(dataEvent.IsFormOpen()&&dataEvent.IsPrimary())
      {
      CAbkSingleLock lockForm(&m_mutexAbk,true,1000);
      m_strFormName=dataEvent.GetFormName(); // name of the form the server requested
      ::PostMessage(m_pOwner->m_hWnd,MESSAGE_FORM_OPEN,0,0); // defer event to main thread
      }
    else if(dataEvent.IsFormClose()&&dataEvent.IsPrimary())
      {
      CAbkSingleLock lockForm(&m_mutexAbk,true,1000);
      m_strFormName=dataEvent.GetFormName(); // name of the form the server requested
      ::PostMessage(m_pOwner->m_hWnd,MESSAGE_FORM_CLOSE,0,0); // defer event to main thread
      }
    }
  return pEventData;
  }




//--------------------------------------------------------------------------
// OnLogAdded()           notifies that the log queue has got new entities. may be called in any thread context!
// ---------
// Input: -
// Return: 

/*virtual*/ void CMyAbkClient::OnLogAdded(void)
  {
  ;
  }



