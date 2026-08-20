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
// |  __| |   V   ||  -  /| |  | |  __  \___ \ | | | |/ __|
// | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
// |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
//                                              |___/      
// Filename:    EventQueue.cpp
// Created:     2012-08-17 (05:15)
// Author:      D. Burger
// Description: event queue holding already JSON-formatted events
//------------------------------------------------------------------------------------------------



#include "stdafx.h"
#include "EventQueue.h"

namespace Abk {


//--------------------------------------------------------------------------
// CEventQueue()        Constructor of CEventQueue
// ----------------
// Input: pszArryName = name for the array to be generated
// Return: 

CEventQueue::CEventQueue (const char *pszArryName)
  {
  m_pJaEvents=NULL;
  m_strArryName=pszArryName;
  Flush();
  }


//--------------------------------------------------------------------------
// ~CEventQueue()       Destructor of CEventQueue
// -----------------
// Input: -
// Return: 

/*virtual*/ CEventQueue::~CEventQueue ()
  {
  if(m_pJaEvents)
    delete m_pJaEvents;
  }


//--------------------------------------------------------------------------
// Flush()                 flushes the queue and prepares a new cycle
// -------
// Input: -
// Return: 

void CEventQueue::Flush (void)
  {
  if(m_pJaEvents)
    delete m_pJaEvents;
  m_jfBuffer.InitNew();
  m_pJaEvents=new CJsonStreamArray(&m_jfBuffer,m_strArryName);
  m_bEmpty=true;
  }


//--------------------------------------------------------------------------
// CloseAndGetFormatter()  returns pointer to formatter
// ----------------------
// Input: -
// Return: pointer to the closed json formatter

CJsonFormatter *CEventQueue::CloseAndGetFormatter (void)
  {
  m_jfBuffer.Close(); // write } if neccessary
  return &m_jfBuffer;
  }



//--------------------------------------------------------------------------
// GetArrayForFeeding()    returns array for feeding data
// --------------------
// Input: -
// Return: the event array object for feeding data

CJsonStreamArray *CEventQueue::GetArrayForFeeding (void)
  {
  m_bEmpty=false;
  return m_pJaEvents;
  }



//--------------------------------------------------------------------------
// IsEmpty()               returns true if event queue is empty
// ---------
// Input: -
// Return: true if event queue is empty

bool CEventQueue::IsEmpty (void) const
  {
  return m_bEmpty;
  }



  } // namespace

