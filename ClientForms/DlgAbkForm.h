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
// Filename:    DlgAbkForm.h
// Created:     2012-08-08 (12:50)
// Author:      D. Burger
// Description: Dialog handling ABK form functionalities
//------------------------------------------------------------------------------------------------

#pragma once

#include "AbkClient.h"

class CBusAbk;

class CDlgAbkForm : public CDialog
  {
  DECLARE_DYNAMIC(CDlgAbkForm)

  typedef void (CObject::*PFN_REGISTER) (CDlgAbkForm *pDlgForm); // callback for registering
  typedef void (CObject::*PFN_UNREGISTER) (CDlgAbkForm *pDlgForm); // callback for unregistering

  // data members
  protected:
    std::list<Abk::CAbkClient::CFormElement> m_lstElements; // elements to be displayed
    std::map<CString,int> m_mapControlIds; // mapping control names to ids
    DLGTEMPLATE *m_pTemplate; // dialog template
    BOOL m_bInitialUpdate; // TRUE if controls are updated from data initially. FALSE on consecutive updates
    CObject *m_pOwner; // this form belongs to this object and notifications for registering and unregistering are called
    PFN_UNREGISTER m_pfnUnregister; // called when dialog is about to be destroyed
    BOOL m_bIsRegistered; // flag to track the registered state
    CString m_strName; // name of the form
    int m_nPersistenceMs; // persitence, time for auto-close. 0 means no auto-close
    BOOL m_bUserTerminated; // if user closed the dialog: TRUE. if terminated by timeout or close request: FALSE

  // construction/destruction/setup
  public:
    CDlgAbkForm();   // standard constructor
    virtual ~CDlgAbkForm();
    BOOL InitModal (CObject *pOwner, PFN_REGISTER pfnRegister, PFN_UNREGISTER pfnUnregister, LPCTSTR pszName, LPCTSTR pszCaption, int nPersistenceMs, std::list<Abk::CAbkClient::CFormElement> &lstElements, const LOGFONT *pLogfont=NULL, CWnd* pParentWnd=NULL); // creates the dialog

  // attribute and methods
  public:
    std::list<Abk::CAbkClient::CFormElement> &GetData (void); // returns reference to the data elements
    const CString &GetName (void); // returns the name of the form
    BOOL UpdateContent (LPCTSTR pszCaption, int nPersistenceMs, std::list<Abk::CAbkClient::CFormElement> &lstElements); // update the forms content (and caption)
    void CloseForm (void); // closes the form
    BOOL IsTerminatedByUser (void); // returns TRUE if the user terminated the dialog
    static BOOL FormatVariant (LPCVARIANT pVariant, CString &strOutput, LPCTSTR ppszFormatInt=NULL, LPCTSTR ppszFormatDouble=NULL);

  // implementation
  protected:
    virtual void DoDataExchange (CDataExchange* pDX);    // DDX/DDV support
    BOOL CreateTemplate (LPCTSTR pszCaption, std::list<Abk::CAbkClient::CFormElement> &lstElements, const LOGFONT *pLogfont=NULL); // creates template for given form elements
    CWnd *GetControl (const CString &strName); // returns control by given name
    Abk::CAbkClient::CFormElement *GetData (LPCTSTR pszName); // searches for the data structure for a given control name

  protected:
    DECLARE_MESSAGE_MAP()
  public:
    virtual BOOL OnInitDialog();
    virtual void OnCancel();
    virtual void OnOK();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
  };


