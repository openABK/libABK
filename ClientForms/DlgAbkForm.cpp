// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------------------------
//  _____  __   __  _____  _    _        ____              
// |  ___||  \ /  ||  _  \| |  | |      / ___|  _   _  ___ 
// |  __| |   V   ||  -  /| |  | |  __  \___ \ | | | |/ __|
// | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
// |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
//                                              |___/      
// Filename:    DlgAbkForm.cpp
// Created:     2012-08-08 (12:50)
// Author:      D. Burger
// Description: Dialog handling ABK form functionalities
//------------------------------------------------------------------------------------------------



#include "stdafx.h"
#include "AbkForms.h"
#include "DlgAbkForm.h"
// #include "Utils.h"
#include <vector>

// CDlgAbkForm dialog

#define DLGCONTROLID_BASE 100 // ids of controls, base of

#define PERSISTENCE_TIMERID 452 // arbitrary number, timer id controlling the persistence

IMPLEMENT_DYNAMIC(CDlgAbkForm, CDialog)

BEGIN_MESSAGE_MAP(CDlgAbkForm, CDialog)
  ON_WM_TIMER()
END_MESSAGE_MAP()


//--------------------------------------------------------------------------
// CDlgAbkForm()           Constructor of CDlgAbkForm
// -------------
// Input: -
// Return: 

CDlgAbkForm::CDlgAbkForm ()
  : CDialog()
  {
  m_pTemplate=NULL;
  m_pOwner=NULL;
  m_bInitialUpdate=TRUE;
  m_nPersistenceMs=0; // default: no auto-close. form stays open until user closes it
  m_bUserTerminated=TRUE; // default: user is the one who had closed the form
  m_pfnUnregister=NULL;
  m_bIsRegistered=FALSE;
  }



//--------------------------------------------------------------------------
// ~CDlgAbkForm()          Constructor of ~CDlgAbkForm
// --------------
// Input: -
// Return: 

CDlgAbkForm::~CDlgAbkForm()
  {
  ASSERT(m_bIsRegistered); // destructor called without having it registered in e.g. Initmodal()
  if(m_pOwner && m_pfnUnregister)
    (*m_pOwner.*m_pfnUnregister)(this); // unregister form
  m_bIsRegistered=FALSE;
  if(m_pTemplate)
    delete m_pTemplate;
  }



//--------------------------------------------------------------------------
// DoDataExchange()        Called by framework to exchange and validate dialog data
// ----------------
// Input: pDX = pointer to a CDataExchange object
// Return: -

void CDlgAbkForm::DoDataExchange (CDataExchange* pDX)
  {
  CDialog::DoDataExchange(pDX);

  ASSERT(m_pTemplate); // dialog was not initialized
  int nControlId=DLGCONTROLID_BASE;
  std::vector<Abk::CAbkClient::CFormElement>::iterator iterElement;
  if(pDX->m_bSaveAndValidate) // update data from control
    {
    for(iterElement=m_vectElements.begin();iterElement!=m_vectElements.end();++iterElement)
      {
      CWnd *pControl=GetDlgItem(nControlId);
      Abk::CAbkClient::CFormElement *pElement=&*iterElement;
      VariantClear(&pElement->m_varValue);
      if(pElement->m_nType==Abk::CAbkClient::CFormElement::TYPE_BUTTON) // cancel and submit buttons have special IDs
        {
        if(pElement->m_nFlags&Abk::CAbkClient::CFormElement::CANCEL)
          pControl=GetDlgItem(IDCANCEL);
        if(pElement->m_nFlags&Abk::CAbkClient::CFormElement::SUBMIT)
          pControl=GetDlgItem(IDOK);
        }
      if(pControl)
        {
        CString strValue;
        COleVariant varValue;
        varValue.vt=VT_EMPTY;
        switch(pElement->m_nType)
          {
        case Abk::CAbkClient::CFormElement::TYPE_EDIT:
          pControl->GetWindowText(strValue);
          varValue=strValue;
          break;
        case Abk::CAbkClient::CFormElement::TYPE_CHECKBOX:
          {
          CButton *pCheck=static_cast<CButton *>(pControl);
          varValue.vt=VT_BOOL;
          if(pCheck->GetCheck())
            varValue.boolVal=VARIANT_TRUE;
          else
            varValue.boolVal=VARIANT_FALSE;
          }
          break;
        case Abk::CAbkClient::CFormElement::TYPE_COMBO:
          {
          CComboBox *pCombo=static_cast<CComboBox *>(pControl);
          varValue.lVal=pCombo->GetCurSel();
          varValue.vt=VT_I4;
          }
          break;
        case Abk::CAbkClient::CFormElement::TYPE_BUTTON:
          varValue.vt=VT_BOOL;
          if(CWnd::GetFocus()==pControl)
            varValue.boolVal=VARIANT_TRUE;
          else
            varValue.boolVal=VARIANT_FALSE;
          break;
        default:
          ASSERT(FALSE); // an unimplemented type encountered
          }
        if(varValue.vt!=VT_EMPTY)
          {
          VariantCopy(&pElement->m_varValue,&varValue);
          }
        }
      nControlId++;
      }
    }
  else // update the control from data
    {
    for(iterElement=m_vectElements.begin();iterElement!=m_vectElements.end();++iterElement)
      {
      Abk::CAbkClient::CFormElement *pElement=&*iterElement;
      CWnd *pControl=GetDlgItem(nControlId);
      if(pControl)
        {
        if((m_bInitialUpdate)||(pElement->m_nFlags&Abk::CAbkClient::CFormElement::UPDATEABLE)) // update the controls on initial update or if it has the updateable flag
          {
          switch(pElement->m_nType)
            {
          case Abk::CAbkClient::CFormElement::TYPE_EDIT:
            {
            CString strFormatted;
            FormatVariant((LPCVARIANT)&pElement->m_varValue,strFormatted);
            pControl->SetWindowText(strFormatted);
            }
            break;
          case Abk::CAbkClient::CFormElement::TYPE_BUTTON:
            pControl->SetWindowText(pElement->m_strCaption);
            break;
          case Abk::CAbkClient::CFormElement::TYPE_CHECKBOX: // check box needs to be initialized here since it could not be done with the dialog template method
            {
            ASSERT(pControl);
            CButton *pCheckbox=static_cast<CButton *>(pControl);
            HRESULT hresConversion=::VariantChangeType(&pElement->m_varValue,&pElement->m_varValue,0,VT_BOOL);
            if(hresConversion!=S_OK)
              pDX->Fail();
            pCheckbox->SetCheck(pElement->m_varValue.boolVal!=0);
            }
            break;
          case Abk::CAbkClient::CFormElement::TYPE_COMBO: // combo needs to be filled and initialized here since it could not be done with the dialog template method
            {
            ASSERT(pControl);
            CComboBox *pCombo=static_cast<CComboBox *>(pControl);
            pCombo->ResetContent(); // before inserting items, remove the old combo options
            std::vector<CString>::iterator iterOptions;
            int nOption=0;
            for(iterOptions=pElement->m_vectOptions.begin();iterOptions!=pElement->m_vectOptions.end();++iterOptions)
              pCombo->InsertString(-1,*iterOptions);
            HRESULT hresConversion=::VariantChangeType(&pElement->m_varValue,&pElement->m_varValue,0,VT_I4);
            if(hresConversion!=S_OK)
              pDX->Fail();
            pCombo->SetCurSel(pElement->m_varValue.lVal);
            }
            break;
          default:
            ASSERT(FALSE);
            }
          }
        }
      nControlId++;
      }
    }
  }



//--------------------------------------------------------------------------
// InitModal()             creates the dialog
// -----------
// Input: pOwner = pointer to the owning object recieving notifications for registering and unregistering
//        strName = name of the form
//        strCaption = caption for the form
//        nPersistenceMs = persitence time in ms. 0 means no auto-close
//                         (infinite, until user closes)
//        lstElements = list with form elements
//        pLogfont = font description for the dialog
//        pParentWnd = parent window, optional
// Return: 

BOOL CDlgAbkForm::InitModal (CObject *pOwner, PFN_REGISTER pfnRegister, PFN_UNREGISTER pfnUnregister, LPCTSTR pszName, LPCTSTR pszCaption, int nPersistenceMs, std::vector<Abk::CAbkClient::CFormElement> &vectElements, const LOGFONT *pLogfont/*=NULL*/, CWnd* pParentWnd/*=NULL*/)
  {
  ASSERT(m_pOwner==NULL); // you tried to double-init the form
  ASSERT(!m_bIsRegistered); // twice initialized?
  m_pOwner=pOwner;
  m_strName=pszName;
  m_pfnUnregister=pfnUnregister;
  if(m_pOwner&&pfnRegister)
    (*m_pOwner.*pfnRegister)(this); // register form at the owner
  m_bIsRegistered=TRUE;
  m_nPersistenceMs=nPersistenceMs;
  m_vectElements=vectElements; // make copy of element list
  if(CreateTemplate(pszCaption,vectElements,pLogfont))
    {
    return CDialog::InitModalIndirect(m_pTemplate,pParentWnd);
    }
  return FALSE;
  }



//--------------------------------------------------------------------------
// UpdateContent()         update the forms content (and caption)
// ---------------
// Input: strCaption = new caption string
//        nPersistenceMs = persitence time in ms. 0 means no auto-close
//                         (infinite, until user closes)
//        lstElements = list of elements containing the new values
// Return: TRUE on success, FALSE on error

BOOL CDlgAbkForm::UpdateContent (LPCTSTR pszCaption, int nPersistenceMs, std::list<Abk::CAbkClient::CFormElement> &lstElements)
  {
  ASSERT(pszCaption);
  ASSERT(IsWindow(m_hWnd));
  m_bInitialUpdate=FALSE; // all further calls to DoDataExchange() are not the initial one
  SetWindowText(pszCaption); // update the window caption
  std::list<Abk::CAbkClient::CFormElement>::iterator iterElement;
  BOOL bSuccess=TRUE;
  for(iterElement=lstElements.begin();iterElement!=lstElements.end();++iterElement) // go through all elements
    {
    Abk::CAbkClient::CFormElement *pElement=&*iterElement;

    CWnd *pControl=GetControl(pElement->m_strName); // get the control out of its name
    if(!pControl)  // if dialog contains no such control
      bSuccess=FALSE; // .. treat it as error but continue to update other controls (use case: list contains unsupported controls which were never created in InitModal())
    else
      {
      Abk::CAbkClient::CFormElement *pDialogDataElement=GetData(pElement->m_strName); // find the element in the dialog data with the appropriate name
      if(pDialogDataElement) // if found
        {
        if(pElement->m_nFlags&Abk::CAbkClient::CFormElement::UPDATEABLE) // if control is marked as updateable
          *pDialogDataElement=*pElement; // copy value, caption, options etc.
        }
      else
        bSuccess=FALSE; // no data element found in the daialog data: treat as error but continue to update the other controls
      }
    }
  UpdateData(FALSE); // invoke DoDataExchange()
  m_nPersistenceMs=nPersistenceMs;
  if(m_nPersistenceMs)
    SetTimer(PERSISTENCE_TIMERID,m_nPersistenceMs,NULL); // set the timer to the new persistence time
  else // infinite duration (close when user clicks
    KillTimer(PERSISTENCE_TIMERID);
  return bSuccess;
  }


//--------------------------------------------------------------------------
// GetData()               searches for the data structure for a given control name
// ---------
// Input: strName = name of element to be retrieved
// Return: pointer to a data structure, NULL if not found

Abk::CAbkClient::CFormElement *CDlgAbkForm::GetData (LPCTSTR pszName)
  {
  std::vector<Abk::CAbkClient::CFormElement>::iterator iterElement;
  BOOL bSuccess=TRUE;
  for(iterElement=m_vectElements.begin();iterElement!=m_vectElements.end();++iterElement) // go through all elements
    {
    if(!iterElement->m_strName.Compare(pszName)) // if element found
      return &*iterElement;
    }
  return NULL;
  }



//--------------------------------------------------------------------------
// GetName()               returns the name of the form
// ---------
// Input: -
// Return: 

const CString &CDlgAbkForm::GetName (void)
  {
  return m_strName;
  }



class CTemplateWriter
  {
  public:
    LPCDLGTEMPLATE Template (void) {return (LPCDLGTEMPLATE)&v[0];}
    int GetSize (void) {return v.size();}
    void AlignToDword (void) {if(v.size()%4) Write(NULL,4-(v.size()%4));}
    void Write (LPCVOID pvWrite, DWORD cbWrite)
      {
      v.insert(v.end(),cbWrite,0);
      if(pvWrite)
        CopyMemory(&v[v.size()-cbWrite],pvWrite,cbWrite);
      }
    template <typename T> void Write(T t) { Write(&t, sizeof(T)); }
    void WriteString (LPCTSTR psz) {Write(psz,(lstrlenW(psz) + 1)*sizeof(TCHAR));}
  private:
    std::vector<BYTE> v;
  };

//--------------------------------------------------------------------------
// CreateTemplate()        creates template for given form elements
// ----------------
// Input: strCaption = caption string
//        lstElements = list of elements to be generated in the template
//        pLogFont = pointe to font description for the dialog
// Return: TRUE on success, false on error

BOOL CDlgAbkForm::CreateTemplate (LPCTSTR pszCaption, std::vector<Abk::CAbkClient::CFormElement> &vectElements, const LOGFONT *pLogfont/*=NULL*/)
  {
  // see http://msdn.microsoft.com/en-us/magazine/cc163755.aspx
  // see http://blogs.msdn.com/b/oldnewthing/archive/2005/04/29/412577.aspx
  // for WindowsCE, see CreateDialogIndirect() and the structures DLGTEMPLATE/DLGITEMTEMPLATE (http://msdn.microsoft.com/en-us/library/ms908170.aspx)

  enum 
    {
    CONTROLHEIGHT=10,
    CONTROLWIDTH=100,
    GAPY=3,
    TOPGAP=GAPY,
    BOTTOMGAP=GAPY,
    GAPX=7,
    LEFTGAP=GAPX,
    RIGHTGAP=GAPX,
    BUTTON_MINWIDTH=50,
    STATIC_YOFFSET=2, // statics are placed this offset below other controls
    BUTTON_HEIGHT=16, // height of buttons
    };
  
  ASSERT(m_pTemplate==NULL); // initialized twice?
  m_mapControlIds.clear();
  HDC hdc=::GetDC(NULL);
  CDC *pDC=CDC::FromHandle(hdc);

  if(hdc)
    {
    LOGFONT lfFont;
    // prepare and get metrics
    if(!pLogfont)
      {
      #ifdef WINCE
        HGDIOBJ hDefaultFont=GetStockObject(SYSTEM_FONT);
        ASSERT(hDefaultFont);
        CFont *pDefaultFont=CFont::FromHandle((HFONT)hDefaultFont);
        ASSERT(pDefaultFont);
        pDefaultFont->GetLogFont(&lfFont);
      #else
        NONCLIENTMETRICSW ncm={sizeof(ncm)};
        SystemParametersInfo(SPI_GETNONCLIENTMETRICS,0,&ncm,0);
        memcpy(&lfFont,&ncm.lfMessageFont,sizeof(LOGFONT));
      #endif
      }
    else
      {
      memcpy(&lfFont,pLogfont,sizeof(LOGFONT));
      }
    if(lfFont.lfHeight<0)
      lfFont.lfHeight=-::MulDiv(lfFont.lfHeight,72,GetDeviceCaps(hdc,LOGPIXELSY));
    CFont fontTemp;
    fontTemp.CreateFontIndirect(&lfFont);
    pDC->SelectObject(&fontTemp);
    CSize szDluBase256;
    szDluBase256=pDC->GetTextExtent(CString(_T("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz")));
    szDluBase256.cx=szDluBase256.cx*256/52/4;
    szDluBase256.cy=szDluBase256.cy*256/8;;

    CSize szDialog;
    szDialog.cy=TOPGAP+BOTTOMGAP-GAPY;
    int nCaptionWidthMax=0; // the widest caption
    int nControlCount=0; // number of controls to be set
    std::vector<Abk::CAbkClient::CFormElement>::iterator iterElement;
    for(iterElement=vectElements.begin();iterElement!=vectElements.end();++iterElement)
      {
      Abk::CAbkClient::CFormElement *pElement=&*iterElement;
      CString strStaticCaption=pElement->m_strCaption;
      strStaticCaption.Append(_T(":"));
      int nCaptionWidth=pDC->GetTextExtent(strStaticCaption).cx;
      nCaptionWidthMax=max(nCaptionWidthMax,nCaptionWidth);
      nControlCount++;
      if(pElement->m_nType!=Abk::CAbkClient::CFormElement::TYPE_BUTTON)
        nControlCount++; // all except buttons have an additional caption
      szDialog.cy+=GAPY;
      if(pElement->m_nType==Abk::CAbkClient::CFormElement::TYPE_BUTTON)
        szDialog.cy+=BUTTON_HEIGHT;
      else
        szDialog.cy+=CONTROLHEIGHT;
      }
    nCaptionWidthMax=nCaptionWidthMax*256/szDluBase256.cx;
    nCaptionWidthMax+=nCaptionWidthMax/8; // add extra width
    szDialog.cx=LEFTGAP+nCaptionWidthMax+GAPX+CONTROLWIDTH+RIGHTGAP;
    // szDialog.cy=TOPGAP+lstElements.size()*YPITCH-GAPY+BOTTOMGAP;


    // generate the template
    CTemplateWriter twDialog;
    DWORD dwDialogStyle=DS_SETFONT|DS_CENTER|DS_MODALFRAME|WS_CAPTION|WS_VISIBLE|WS_POPUP|WS_SYSMENU;

    #ifdef WINCE
      twDialog.Write<DWORD>(dwDialogStyle); // style
      twDialog.Write<DWORD>(0); // extended style
      twDialog.Write<WORD>(nControlCount); // number of controls
      twDialog.Write<WORD>(0); // X
      twDialog.Write<WORD>(0); // Y
      twDialog.Write<WORD>((WORD)szDialog.cx); // width
      twDialog.Write<WORD>((WORD)szDialog.cy); // height
    #else
      twDialog.Write<WORD>(1); // dialog version
      twDialog.Write<WORD>(0xFFFF); // extended dialog template
      twDialog.Write<DWORD>(0); // help ID
      twDialog.Write<DWORD>(0); // extended style
      twDialog.Write<DWORD>(dwDialogStyle);
      twDialog.Write<WORD>(nControlCount); // number of controls
      twDialog.Write<WORD>(0); // X
      twDialog.Write<WORD>(0); // Y
      twDialog.Write<WORD>((WORD)szDialog.cx); // width
      twDialog.Write<WORD>((WORD)szDialog.cy); // height
    #endif
    twDialog.WriteString(L""); // no menu
    twDialog.WriteString(L""); // default dialog class
    twDialog.WriteString(pszCaption); // title

    // Next comes the font description.
    twDialog.Write<WORD>((WORD)lfFont.lfHeight); // point
    #ifndef WINCE
      twDialog.Write<WORD>((WORD)lfFont.lfWeight); // weight
      twDialog.Write<BYTE>(lfFont.lfItalic); // Italic
      twDialog.Write<BYTE>(lfFont.lfCharSet); // CharSet
    #endif
    twDialog.WriteString(lfFont.lfFaceName);

    int nIdControl=DLGCONTROLID_BASE;
    int nYPos=TOPGAP; // current y-position
    for(iterElement=vectElements.begin();iterElement!=vectElements.end();++iterElement)
      {
      Abk::CAbkClient::CFormElement *pElement=&*iterElement;

      // write the static with the caption
      if(pElement->m_nType!=Abk::CAbkClient::CFormElement::TYPE_BUTTON)
        {
        twDialog.AlignToDword();
        #ifdef WINCE
          twDialog.Write<DWORD>(WS_CHILD|WS_VISIBLE); // style
          twDialog.Write<DWORD>(WS_EX_RIGHT); // window extended style
        #else
          twDialog.Write<DWORD>(0); // help id
          twDialog.Write<DWORD>(WS_EX_RIGHT); // window extended style
          twDialog.Write<DWORD>(WS_CHILD|WS_VISIBLE); // style
        #endif
        twDialog.Write<WORD>(LEFTGAP); // x
        twDialog.Write<WORD>(nYPos+STATIC_YOFFSET); // y
        twDialog.Write<WORD>(nCaptionWidthMax); // width
        twDialog.Write<WORD>(CONTROLHEIGHT); // height
#ifdef WINCE
        twDialog.Write<WORD>(IDC_STATIC); // control ID
#else
        twDialog.Write<DWORD>(IDC_STATIC); // control ID
#endif
        twDialog.Write<WORD>(0xFFFF); // control type id follows
        twDialog.Write<WORD>(0x0082); // static
        CString strStatic(pElement->m_strCaption);
        if(pElement->m_nType!=Abk::CAbkClient::CFormElement::TYPE_BUTTON)
          strStatic.Append(_T(":"));
        twDialog.WriteString(strStatic); // text
        twDialog.Write<WORD>(0); // no extra data
        }

      // write the element itself
      int nTypeAtom=0;
      int nX=LEFTGAP+nCaptionWidthMax+GAPX; // x position of control
      int nWidth=CONTROLWIDTH;
      int nIdControlSpecial=nIdControl; // may be overwritten for default buttons
      int nControlHeight=CONTROLHEIGHT;
      BOOL bCaptionInControl=FALSE; // TRUE if caption shall be set for the control
      DWORD dwStyle=WS_CHILD|WS_VISIBLE|WS_TABSTOP;
      if(pElement->m_nFlags&Abk::CAbkClient::CFormElement::READONLY)
        dwStyle|=WS_DISABLED;
      DWORD dwExStyle=0;
      switch(pElement->m_nType)
        {
      case Abk::CAbkClient::CFormElement::TYPE_EDIT:
        nTypeAtom=0x0081;
        dwStyle|=WS_BORDER;
        if(pElement->m_nFlags&Abk::CAbkClient::CFormElement::PASSWORD)
          dwStyle|=ES_PASSWORD;
        break;
      case Abk::CAbkClient::CFormElement::TYPE_CHECKBOX:
        nTypeAtom=0x0080;
        dwStyle|=BS_CHECKBOX|BS_AUTOCHECKBOX;
        break;
      case Abk::CAbkClient::CFormElement::TYPE_COMBO:
        nTypeAtom=0x0085;
        dwStyle|=CBS_DROPDOWNLIST|WS_BORDER; // CBS_DROPDOWN;
        nControlHeight=100;
        break;
      case Abk::CAbkClient::CFormElement::TYPE_BUTTON:
        nX=LEFTGAP; // buttons at left side
        nWidth=pDC->GetTextExtent(pElement->m_strCaption).cx*256/szDluBase256.cx+2*GAPX;
        nWidth=max(BUTTON_MINWIDTH,nWidth);
        if(pElement->m_nFlags&Abk::CAbkClient::CFormElement::SUBMIT)
          {
          nIdControlSpecial=IDOK;
          dwStyle|=BS_DEFPUSHBUTTON;
          }
        else if(pElement->m_nFlags&Abk::CAbkClient::CFormElement::CANCEL)
          {
          nIdControlSpecial=IDCANCEL;
          dwStyle|=BS_DEFPUSHBUTTON;
          }
        bCaptionInControl=TRUE;
        nControlHeight=BUTTON_HEIGHT;
        nTypeAtom=0x0080;
        break;
      default:
        nTypeAtom=0x0082;
        }
      twDialog.AlignToDword();
      #ifdef WINCE
        twDialog.Write<DWORD>(dwStyle); // style
        twDialog.Write<DWORD>(dwExStyle); // window extended style
      #else
        twDialog.Write<DWORD>(0); // help id
        twDialog.Write<DWORD>(dwExStyle); // window extended style
        twDialog.Write<DWORD>(dwStyle); // style
      #endif
      twDialog.Write<WORD>(nX); // x
      twDialog.Write<WORD>(nYPos); // y
      twDialog.Write<WORD>(nWidth); // width
      twDialog.Write<WORD>(nControlHeight); // height
      #ifdef WINCE
        twDialog.Write<WORD>((WORD)nIdControlSpecial); // control ID
      #else
        twDialog.Write<DWORD>(nIdControlSpecial); // control ID
      #endif
      twDialog.Write<WORD>(0xffff); // type atom follows
      twDialog.Write<WORD>(nTypeAtom);
      m_mapControlIds.insert(std::pair<CString,int>(pElement->m_strName,nIdControlSpecial)); // keep control id for its name in mind
      
      // write control data
      if(bCaptionInControl)
        {
        twDialog.WriteString(pElement->m_strCaption);
        }
      else
        {
        switch(pElement->m_varValue.vt)
          {
        case VT_BSTR:
          twDialog.WriteString(pElement->m_varValue.bstrVal); // text
          break;
        default:
          twDialog.WriteString(_T(""));
          break;
          }
        }
      twDialog.Write<WORD>(0); // no extra data

      nIdControl++;
      if(pElement->m_nType==Abk::CAbkClient::CFormElement::TYPE_BUTTON)
        nYPos+=BUTTON_HEIGHT+GAPY;
      else
        nYPos+=CONTROLHEIGHT+GAPY;
      }

    ::ReleaseDC(NULL,hdc);

    m_pTemplate=(DLGTEMPLATE *)(new BYTE[twDialog.GetSize()]);
    memcpy(m_pTemplate,twDialog.Template(),twDialog.GetSize()); // make copy
    return TRUE;
    }
  return FALSE;
  }



//--------------------------------------------------------------------------
// OnInitDialog()          This method is called in response to the WM_INITDIALOG message
// --------------
// Input: -
// Return: non 0, if windows shall set the focus to the default location

BOOL CDlgAbkForm::OnInitDialog()
  {
  CDialog::OnInitDialog();
  if(m_nPersistenceMs) // if a limited persistence is desired..
    SetTimer(PERSISTENCE_TIMERID,m_nPersistenceMs,NULL); // ..set the timer to the persistence time
  return TRUE;  // return TRUE unless you set the focus to a control
  }



//--------------------------------------------------------------------------
// GetData()               returns reference to the data elements
// ---------
// Input: -
// Return: 

std::vector<Abk::CAbkClient::CFormElement> &CDlgAbkForm::GetData (void)
  {
  return m_vectElements;
  }


//--------------------------------------------------------------------------
// GetControl()            returns control by given name
// ------------
// Input: strName = name of control
// Return: pointer to control, NULL if not found

CWnd *CDlgAbkForm::GetControl (const CString &strName)
  {
  CWnd *pControl;
  std::map<CString,int>::iterator iterControl=m_mapControlIds.find(strName); // get the control out of its name
  if(iterControl==m_mapControlIds.end())
    return NULL;
  int nControlId=iterControl->second;
  pControl=GetDlgItem(nControlId);
  return pControl;
  }



//--------------------------------------------------------------------------
// OnCancel()              called when cancel button was clicked
// ----------
// Input: -
// Return: 

void CDlgAbkForm::OnCancel()
  {
  UpdateData();
  CDialog::OnCancel();
  }



//--------------------------------------------------------------------------
// OnOK()                  called when OK button was clicked
// ------
// Input: -
// Return: 

void CDlgAbkForm::OnOK()
  {
  UpdateData();
  CDialog::OnOK();
  }



//--------------------------------------------------------------------------
// OnTimer()               called when timer elapses
// ---------
// Input: nIDEvent = id of the timer which is expired
// Return: -

void CDlgAbkForm::OnTimer(UINT_PTR nIDEvent)
  {
  if(nIDEvent==PERSISTENCE_TIMERID)
    CloseForm();
  CDialog::OnTimer(nIDEvent);
  }



//--------------------------------------------------------------------------
// CloseForm()             closes the form
// -----------
// Input: -
// Return: 

void CDlgAbkForm::CloseForm (void)
  {
  m_bUserTerminated=FALSE; // if the form gets closed other then the user, suppress the response
  PostMessage(WM_CLOSE,0,0);
  }



//--------------------------------------------------------------------------
// IsTerminatedByUser()    returns TRUE if the user terminated the dialog
// --------------------
// Input: -
// Return: TRUE if user has closed, FALSE if closed by timer or close request

BOOL CDlgAbkForm::IsTerminatedByUser (void)
  {
  return m_bUserTerminated;
  }


//--------------------------------------------------------------------------
// FormatVariant()         formats a variant to a string
// ---------------
// Input: pVariant = pointer to variant to be formatted
//        strOutput = reference to string receiving the formatted output
//        strFormatInt = integer format string, optional
//        strFormatDouble = float and double format string, optional
// Return: TRUE on success, FALSE on error (i.e. unsupported variant type)

/*static*/ BOOL CDlgAbkForm::FormatVariant (LPCVARIANT pVariant, CString &strOutput, LPCTSTR pszFormatInt/*=NULL*/, LPCTSTR pszFormatDouble/*=NULL*/)
  {
  int nValue;
  float fValue;
  double dValue;
  switch(pVariant->vt)
    {
    case VT_I2:
      nValue=pVariant->iVal;
      if(!pszFormatInt)
        pszFormatInt=_T("%d");
      strOutput.Format(pszFormatInt,nValue);
      return TRUE;
    case VT_I4:
      nValue=pVariant->lVal;
      if(!pszFormatInt)
        pszFormatInt=_T("%d");
      strOutput.Format(pszFormatInt,nValue);
      return TRUE;
    case VT_INT:
      nValue=pVariant->intVal;
      if(!pszFormatInt)
        pszFormatInt=_T("%d");
      strOutput.Format(pszFormatInt,nValue);
      return TRUE;
    case VT_R4:
      fValue=pVariant->fltVal;
      dValue=fValue;
      if(!pszFormatDouble)
        pszFormatDouble=_T("%g");
      strOutput.Format(pszFormatDouble,dValue);
      return TRUE;
    case VT_R8:
      dValue=pVariant->dblVal;
      if(!pszFormatDouble)
        pszFormatDouble=_T("%g");
      strOutput.Format(pszFormatDouble,dValue);
      return TRUE;
    case VT_BSTR:
      strOutput.SetString(pVariant->bstrVal);
      return TRUE;
    case VT_BOOL:
      nValue=pVariant->boolVal;
      if(nValue)
        strOutput.SetString(_T("true"));
      else
        strOutput.SetString(_T("false"));
      return TRUE;
    case VT_EMPTY:
      return TRUE;
    }
  ASSERT(0); // not a supported variant type
  return FALSE;
  }

