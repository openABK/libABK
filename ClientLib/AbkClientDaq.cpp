// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------------------------
//  _____  __   __  _____  _    _        ____              
// |  ___||  \ /  ||  _  \| |  | |      / ___|  _   _  ___ 
// |  __| |   V   ||  -  /| |  | |  __  \___ \ | | | |/ __|
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
#include "AbkClientAbstraction.h"


namespace Abk
  {


//--------------------------------------------------------------------------
// CAbkClientDaq()         Constructor of CAbkClientDaq
// ---------------
// Input: strName = name to be assigned to the DAQ list
//        strUrl: = url where to request the mailbox
// Return: 

  CAbkClientDaq::CAbkClientDaq(LPCTSTR pszName, LPCTSTR pszUrl)
    : m_strName(pszName)
    , m_nVarCountAtServer(0)
  {
    m_pOwner = NULL;
    m_strUrl = pszUrl;
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
    pClientAux->NavigateDelete(m_strUrl,GetSessionId(),jfDelete.GetStream()->str()); // delete the DAQ at the server
    }
  }




/** updates the DAQ list at the server
@param vectVarNames Reference to vector containing the variable names to be set at the server
@param nCycleMs Cycle to be set to the server
@return true on success, false on error
*/
bool CAbkClientDaq::Update(const std::vector<LPCTSTR>& vectVarNames, int nCycleMs)
{
  bool bSuccess = false;
  if (!vectVarNames.empty()) // only non-empty daq lists are maintained at the server
  {
    if (m_pOwner)
    {
      CJsonFormatter jfDaq;
      jfDaq.WriteValue(ABK_RSP_DAQLIST_NAME, CT2A(m_strName, CP_UTF8)); // "Name": "DaqList1",
      jfDaq.WriteValue(ABK_RSP_DAQLIST_CYCLE, nCycleMs); // "Cycle": 500,
      if (1) // scope for the array
      {
        jfDaq.WriteValue(ABK_RSP_DAQLIST_TEXTTRANSLATION, m_pOwner->TextTranslationByServer()); // "TextTranslation": true,
        CJsonStreamArray jaVars(&jfDaq, ABK_RSP_DAQLIST_DAQLIST); // "DaqList": [
        if (const size_t nCount = vectVarNames.size())
        {
          const LPCTSTR* ppVarNames = &vectVarNames[0];
          for (size_t nVar = 0; nVar < nCount; ++nVar, ++ppVarNames)
          {
            LPCTSTR pszVarNameT = *ppVarNames;
            jaVars.WriteValue(CT2A(pszVarNameT, CP_UTF8)); // "Var1",
          }
        }
      } // array falls out of scope => "]"
      jfDaq.Close(); // "}"
      bSuccess = true;
      CAbkClient::CClientPtrRef pClientAux(m_pOwner->m_pClientAux);
      bSuccess &= pClientAux->NavigatePut(m_strUrl, GetSessionId(), jfDaq.GetStream()->str());
      if (bSuccess)
        m_nVarCountAtServer = vectVarNames.size();
    }
  }
  return bSuccess;
}



/** skips the complete DAQ in the JSON parser
@param rSkip Reference to the JSON parser where the position shall be set to the end of the DAQ
*/
void CAbkClientDaq::DiscardJson(CJsonParserAtl& jpSkip)
{
  for (++jpSkip; !jpSkip.IsDone(); ++jpSkip); // skip each value
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




} // namespace


