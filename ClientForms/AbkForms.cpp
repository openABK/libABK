//------------------------------------------------------------------------------------------------
// Author: D. Burger, Friedberg, Germany, <www.openABK.org>, <www.embu-sys.de>, <info@openABK.org>
//
// You are not allowed to remove this heading from the source code
// You are free to use this library under the terms of the
// Code Project Open Library, see <http://www.codeproject.com/info/cpol10.aspx>
//------------------------------------------------------------------------------------------------


// AbkForms.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "AbkForms.h"
#include "AbkFormsDlg.h"
#include "resource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


bool AbkBindSocketToAdapter (SOCKET pSockBind) {return false;} // [introduced Nov, 22th 2013]  if sending with a specific adapter is desired, this function must bind the socket to the adapters address. else this can be an empty function


// CAbkFormsApp

BEGIN_MESSAGE_MAP(CAbkFormsApp, CWinApp)
  ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CAbkFormsApp construction

CAbkFormsApp::CAbkFormsApp()
  {
  // support Restart Manager
  m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

  // TODO: add construction code here,
  // Place all significant initialization in InitInstance
  }


// The one and only CAbkFormsApp object

CAbkFormsApp theApp;


// CAbkFormsApp initialization

BOOL CAbkFormsApp::InitInstance()
  {
  // InitCommonControlsEx() is required on Windows XP if an application
  // manifest specifies use of ComCtl32.dll version 6 or later to enable
  // visual styles.  Otherwise, any window creation will fail.
  INITCOMMONCONTROLSEX InitCtrls;
  InitCtrls.dwSize = sizeof(InitCtrls);
  // Set this to include all the common control classes you want to use
  // in your application.
  InitCtrls.dwICC = ICC_WIN95_CLASSES;
  InitCommonControlsEx(&InitCtrls);

  CWinApp::InitInstance();

  if (!AfxSocketInit())
    {
    AfxMessageBox(IDP_SOCKETS_INIT_FAILED);
    return FALSE;
    }


  // Create the shell manager, in case the dialog contains
  // any shell tree view or shell list view controls.
  CShellManager *pShellManager = new CShellManager;

  // Standard initialization
  // If you are not using these features and wish to reduce the size
  // of your final executable, you should remove from the following
  // the specific initialization routines you do not need
  // Change the registry key under which our settings are stored
  // TODO: You should modify this string to be something appropriate
  // such as the name of your company or organization
  SetRegistryKey(_T("Local AppWizard-Generated Applications"));

  CAbkFormsDlg dlg;
  m_pMainWnd = &dlg;
  INT_PTR nResponse = dlg.DoModal();
  if (nResponse == IDOK)
    {
    // TODO: Place code here to handle when the dialog is
    //  dismissed with OK
    }
  else if (nResponse == IDCANCEL)
    {
    // TODO: Place code here to handle when the dialog is
    //  dismissed with Cancel
    }

  // Delete the shell manager created above.
  if (pShellManager != NULL)
    {
    delete pShellManager;
    }

  // Since the dialog has been closed, return FALSE so that we exit the
  //  application, rather than start the application's message pump.
  return FALSE;
  }

