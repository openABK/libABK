//------------------------------------------------------------------------------------------------
// Author: D. Burger, Friedberg, Germany, <www.openABK.org>, <www.embu-sys.de>, <info@openABK.org>
//
// You are not allowed to remove this heading from the source code
// You are free to use this library under the terms of the
// Code Project Open Library, see <http://www.codeproject.com/info/cpol10.aspx>
//------------------------------------------------------------------------------------------------



#pragma once

#include "AbkClient.h"
#include "AbkServerEvent.h"
#include "resource.h"

namespace Abk
  {
  class CAbkServerEvent;
  }

class CAbkFormsDlg;

class CMyAbkClient : public Abk::CAbkClient
  {
  public:
    Abk::CAbkServerEvent m_bufEvent; // a one-stage event buffer queue
    CAbkFormsDlg *m_pOwner; // owning dialog of the ABL client
    Abk::CAbkMutex m_mutexAbk; // mutex for interlocking to ABK items
    CString m_strFormName; // name of form the server requested

  protected:
    virtual Abk::CAbkServerEvent *OnServerEvent (Abk::CAbkServerEvent *pEventData); // called when server sent an event
    virtual void OnLogAdded (void); // notifies that the log queue has got new entities. may be called in any thread context!

  };





// CAbkFormsDlg dialog
class CAbkFormsDlg : public CDialogEx
  {
  // data members
  protected:
    CMyAbkClient m_client; // communication to the server

  // Construction
  public:
	  CAbkFormsDlg(CWnd* pParent = NULL);	// standard constructor

  // Dialog Data
	  enum { IDD = IDD_ABKFORMS_DIALOG };

	protected:
	  virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


  // Implementation
  protected:
	  HICON m_hIcon;

	  // Generated message map functions
	  virtual BOOL OnInitDialog();
	  afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	  afx_msg void OnPaint();
	  afx_msg HCURSOR OnQueryDragIcon();
	  DECLARE_MESSAGE_MAP()
  public:
    CString m_strStatus;
//    virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
    virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
  };
