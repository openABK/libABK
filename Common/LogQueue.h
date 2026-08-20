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
// Filename:    LogQueue.h
// Created:     2015-01-19 (07:26)
// Author:      D. Burger
// Description: logging queue used to queue logging info (trace, debug, info, warning, errors)
//------------------------------------------------------------------------------------------------



#pragma once

#include "CrossPlatform.h"
#include <string>
#include <list>

namespace Abk
  {

  template <typename T>
  class CLogQueue
    {
    public:
      enum LOGSEVERITY { LOGSEVERITY_INFO, LOGSEVERITY_DEBUG, LOGSEVERITY_TRACE, LOGSEVERITY_WARNING, LOGSEVERITY_ERROR };
    private:
      struct CEntity
        {
        enum {MAX_ENTITY_STRLEN=255};
        std::basic_string<T> m_strMessage;
        LOGSEVERITY m_nSeverity;
        CEntity () {m_strMessage.clear();m_nSeverity=LOGSEVERITY_INFO;} // ctor
        CEntity (LOGSEVERITY nSeverity, const T* pszMessage, va_list args); // ctor
        void Set (LOGSEVERITY nSeverity, const T* pszMessage, ...); // sets severity and formats string
        void SetV (LOGSEVERITY nSeverity, const T* pszMessage, va_list args);
        };

      Abk::CAbkMutex m_mutex;
      std::list<CEntity> m_lst;

    public:
      CLogQueue (void) {;}
      ~CLogQueue () {;}
      void Add (LOGSEVERITY nSeverity, const T* pszMessage, ...); // sets severity and formats string
      void AddV (LOGSEVERITY nSeverity, const T* pszMessage, va_list args);
      bool Pop (LOGSEVERITY &nSeverityGet, std::basic_string<T> &strMessageGet); // pops one entity from the error log
    };


  } // namespace