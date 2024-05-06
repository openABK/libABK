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
// Filename:    AbkClientDaq.cpp
// Created:     2012-07-12 (18:37)
// Author:      D. Burger
// Description: Data Acqusition List enabling interfacing to the ABK http interface
//------------------------------------------------------------------------------------------------




#include "stdafx.h"


#include <assert.h>

#include "AbkClientDaq.h"
#include "AbkClient.h"
#include "ValuesFromSpec.h"
#include "JsonFormatter.h"


namespace Abk
  {


//--------------------------------------------------------------------------
// CAbkClientDaq()         Constructor of CAbkClientDaq
// ---------------
// Input: strName = name to be assigned to the DAQ list
//        strUrl: = url where to request the mailbox
// Return: 

CAbkClientDaq::CAbkClientDaq (LPCTSTR pszName, LPCTSTR pszUrl)
  : m_nCycleMs(0)
  , m_strName(pszName)
  , m_bCycleInvalid(false)
  , m_bVarlistInvalid(false)
  , m_nVarCountAtServer(0)
  {
  m_pOwner=NULL;
  m_strUrl=pszUrl;
  }





//--------------------------------------------------------------------------
// ~CAbkClientDaq()        Destructor of CAbkClientDaq
// ----------------
// Input: -
// Return: 

/*virtual*/ CAbkClientDaq::~CAbkClientDaq ()
  {
  int nSessionID=GetSessionId();
  // delete the DAQ at the server
  if(nSessionID>0 && m_pOwner && m_nVarCountAtServer) // only non-empty daq lists are maintained at the server
    {
    CJsonFormatter jfDelete;
    jfDelete.WriteValue(ABK_DEL_DAQLIST_NAME,CT2A(m_strName,CP_UTF8)); // "Name": "DaqList1"
    CAbkClient::CClientPtrRef pClientAux(m_pOwner->m_pClientAux);
    pClientAux->NavigateDelete(m_strUrl,GetSessionId(),&jfDelete); // delete the DAQ at the server
    }

  DeleteAllVars();
  }




//--------------------------------------------------------------------------
// SetCycle()              sets the cycle w/o sending it to server
// ----------
// Input: nCycleMs = update cycle in ms
//               after setting all vars and params, Update() must be
//               called to update or creaate the daq list at the server
// Return: -

void CAbkClientDaq::SetCycle (int nCycleMs)
  {
  m_nCycleMs=nCycleMs;
  m_bCycleInvalid=true;
  }



//--------------------------------------------------------------------------
// FindVar()               searches for a variable by name
// ---------
// Input: strName = name to be searched for
// Return: pointer to variable, NULL if not found

CAbkClientVar *CAbkClientDaq::FindVar (LPCTSTR pszName)
  {
  if(const size_t nCount=m_vectVars.size())
    {
    CAbkClientVar **ppVars=&m_vectVars[0];
    for(size_t nVar=0;nVar<nCount;++nVar,++ppVars)
      {
      CAbkClientVar *pVar=*ppVars;
      if(!_tcscmp(pszName,pVar->GetName()))
        return pVar;
      }
    }
  return NULL;
  }


//--------------------------------------------------------------------------
// AddVar()                adds a variable
// --------
// Input: pVar = variable to be added, the variable will be deleted with the daq deletion
//               after setting all vars and params, Update() must be
//               called to update or creaate the daq list at the server
// Return: true if added successfully. It will be deleted when daq gets deleted
//         false if already inserted.

bool CAbkClientDaq::AddVar (CAbkClientVar *pVar)
  {
  if(const size_t nCount=m_vectVars.size())
    {
    CAbkClientVar **ppVars=&m_vectVars[0];
    for(size_t nVar=0;nVar<nCount;++nVar,++ppVars)
      {
      CAbkClientVar *pVarOfList=*ppVars;
      if(pVarOfList==pVar) // if variable already in list
        return false;
      }
    }
  if(m_vectVars.size()==m_vectVars.capacity())
    m_vectVars.reserve(128);
  m_vectVars.push_back(pVar);
  m_bVarlistInvalid=true;
  return true;
  }


//--------------------------------------------------------------------------
// DeleteAllVars()         deletes and removes all variables of this DAQ. If needed, Update() must be called to send the changes to the server
// ---------------
// Input: -
// Return: -

void CAbkClientDaq::DeleteAllVars (void)
  {
  if(const size_t nCount=m_vectVars.size())
    {
    CAbkClientVar **ppVars=&m_vectVars[0];
    for(size_t nVar=0;nVar<nCount;++nVar,++ppVars)
      {
      CAbkClientVar *pVar=*ppVars;
      delete pVar;
      }
    }
  m_vectVars.clear();
  m_bVarlistInvalid=true;
  }




/** updates the daq list at the server
@return true on success, false on error
*/
bool CAbkClientDaq::Update (void)
{
  bool bSuccess = false;
  if (m_pOwner)
  {
    CJsonFormatter jfDaq;
    jfDaq.WriteValue (ABK_RSP_DAQLIST_NAME, CT2A (m_strName, CP_UTF8)); // "Name": "DaqList1",
    if (m_bCycleInvalid)
      jfDaq.WriteValue (ABK_RSP_DAQLIST_CYCLE, m_nCycleMs); // "Cycle": 500,
    if (m_bVarlistInvalid) // if variable list must be updated
    {
      jfDaq.WriteValue (ABK_RSP_DAQLIST_TEXTTRANSLATION, m_pOwner->TextTranslationByServer ()); // "TextTranslation": true,
      CJsonStreamArray jaVars (&jfDaq, ABK_RSP_DAQLIST_DAQLIST); // "DaqList": [
      if (const size_t nCount = m_vectVars.size ())
      {
        CAbkClientVar** ppVars = &m_vectVars[0];
        for (size_t nVar = 0; nVar < nCount; ++nVar, ++ppVars)
        {
          CAbkClientVar* pVar = *ppVars;
          LPCTSTR pszVarNameT = pVar->GetName ();
          jaVars.WriteValue (CT2A (pszVarNameT, CP_UTF8)); // "Var1",
        }
      }
    } // array falls out of scope => "]"
    jfDaq.Close (); // "}"
    bSuccess = true;
    if (!m_vectVars.empty ()) // only non-empty daq lists are maintained at the server
    {
      CAbkClient::CClientPtrRef pClientAux (m_pOwner->m_pClientAux);
      bSuccess &= pClientAux->NavigatePut (m_strUrl, GetSessionId (), &jfDaq);
      if (bSuccess)
      {
        m_nVarCountAtServer = m_vectVars.size ();
        m_bVarlistInvalid = false;
        m_bCycleInvalid = false;
      }
    }
  }
  return bSuccess;
}




//--------------------------------------------------------------------------
// GetSessionId()          returns the session id
// --------------
// Input: -
// Return: session id or -1 if error

int CAbkClientDaq::GetSessionId (void)
  {
  if(!m_pOwner)
    return -1;
  return m_pOwner->GetSessionId();
  }


//--------------------------------------------------------------------------
// OnBeginDataFromServer() called before the variables OnValueFromServer()
// ----------------------- calls for this DAQ start.
// Input: -
// Return: usually return true. You can return false if you want to suppress
//         all calls to the variables virtual function OnValueFromServer()
//         for this reciept of data. OnEndDataFromServer() is called regardless
//         of the return value

/*virtual*/ bool CAbkClientDaq::OnBeginDataFromServer (void)
  {
  return true; // default: don not suppress further calls to OnValueFromServer()
  }




//--------------------------------------------------------------------------
// OnEndDataFromServer()   called when calls to variables OnValueFromServer()
// ---------------------   for this DAQ are done. function is called regardless
//                         of the return value of OnBeginDataFromServer()
// Input: -
// Return: -

/*virtual*/ void CAbkClientDaq::OnEndDataFromServer (void)
  {
  ; // default implementation does nothing
  }



} // namespace


