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
// Filename:    LogQueue.cpp
// Created:     2015-01-19 (07:26)
// Author:      D. Burger
// Description: logging queue used to queue logging info (trace, debug, info, warning, errors)
//------------------------------------------------------------------------------------------------


#include "stdafx.h"

#include <LogQueue.h>
#include <assert.h>

namespace Abk
  {

//--------------------------------------------------------------------------
// constructor

template <typename T>
CLogQueue<T>::CEntity::CEntity (LOGSEVERITY nSeverity, const T *pszMessage, va_list args)
  {
  SetV(nSeverity,pszMessage,args);
  }



//--------------------------------------------------------------------------
// Set()                   sets severity and formats string
// -----
// Input: nSeverity = severity, one of LOGSEVERITY_xxx
//        pszMessage = message format string, like printf
//        args = optional arguments
// Return: 

template <typename T>
void CLogQueue<T>::CEntity::Set (LOGSEVERITY nSeverity, const T *pszMessage, ...)
  {
  va_list args;
  va_start(args,pszMessage);
  SetV(nSeverity,pszMessage,args);
  va_end(args);
  }

template <>
void CLogQueue<char>::CEntity::SetV (LOGSEVERITY nSeverity, const char *pszMessage, va_list args)
  {
  assert(this); // called with a NULL instance pointer??
  assert(pszMessage); // you have to specify a message
  m_nSeverity=nSeverity;
  char cBuf[MAX_ENTITY_STRLEN+1]; // buffer for sprintf
  vsnprintf(cBuf,MAX_ENTITY_STRLEN,_TRUNCATE,pszMessage,args);
  m_strMessage=cBuf;
  }

template <>
void CLogQueue<WCHAR>::CEntity::SetV (LOGSEVERITY nSeverity, const WCHAR *pszMessage, va_list args)
  {
  assert(this); // called with a NULL instance pointer??
  assert(pszMessage); // you have to specify a message
  m_nSeverity=nSeverity;
  WCHAR wcBuf[MAX_ENTITY_STRLEN+1]; // buffer for sprintf
  _vsnwprintf_s(wcBuf,MAX_ENTITY_STRLEN,_TRUNCATE,pszMessage,args);
  m_strMessage=wcBuf;
  }


//--------------------------------------------------------------------------
// Add()                   sets severity and formats string
// -----
// Input: nSeverity = severity, one of LOGSEVERITY_xxx
//        pszMessage = message format string, like printf
//        args = optional arguments
// Return: 

template <typename T>
void CLogQueue<T>::Add (LOGSEVERITY nSeverity, const T *pszMessage, ...)
  {
  va_list args;
  va_start(args,pszMessage);
  AddV(nSeverity,pszMessage,args);
  va_end(args);
  }

template <typename T>
void CLogQueue<T>::AddV (LOGSEVERITY nSeverity, const T *pszMessage, va_list args)
  {
  CAbkSingleLock lock(&m_mutex,TRUE);
  CEntity ent(nSeverity,pszMessage,args);
  m_lst.push_back(ent);
  }



//--------------------------------------------------------------------------
// Pop()                   pops one entity from the error log
// -----
// Input: nSeverityGet = returns the severity
//        strMessageGet = copies the message string to this CString
// Return: TRUE on success, FALSE if no entities were in the list

template <typename T>
bool CLogQueue<T>::Pop (LOGSEVERITY &nSeverityGet, std::basic_string<T> &strMessageGet)
  {
  CAbkSingleLock lock(&m_mutex,TRUE);
  if(m_lst.size()>=1)
    {
    std::list<CEntity>::const_iterator itEntity=m_lst.begin();
    strMessageGet=itEntity->m_strMessage;
    nSeverityGet=itEntity->m_nSeverity;
    m_lst.erase(itEntity); // m_lst.pop_front();
    return TRUE;
    }
  return FALSE;
  }




// explicit template instantiations

  template CLogQueue<char>;
  template CLogQueue<WCHAR>;


  } // namespace Abk


