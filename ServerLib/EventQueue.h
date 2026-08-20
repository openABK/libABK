// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------------------------
//  _____  __   __  _____  _    _        ____              
// |  ___||  \ /  ||  _  \| |  | |      / ___|  _   _  ___ 
// |  __| |   V   ||  -  /| |  | |  __  \___ \ | | | |/ __|
// | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
// |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
//                                              |___/      
// Filename:    EventQueue.h
// Created:     2012-08-17 (05:14)
// Author:      D. Burger
// Description: event queue holding already JSON-formatted events
//------------------------------------------------------------------------------------------------





#pragma once

#include "JsonFormatter.h"

namespace Abk {


// event queue holding events that must be sent to a client
class CEventQueue
  {
  // data members
  protected:
    CJsonFormatter m_jfBuffer;        // already formatted events waiting to be reported to the client
    CJsonStreamArray *m_pJaEvents;    // event array formatter for m_jfBuffer
    const char *m_strArryName;        // name of the array
    bool m_bEmpty;                    // false if at least one event is queued

  // construction/destruction
  public:
    CEventQueue (const char *pszArryName);
    virtual ~CEventQueue ();

  // methods
  public:
    CJsonFormatter *CloseAndGetFormatter (void);  // returns pointer to formatter
    CJsonStreamArray *GetArrayForFeeding (void); // returns array for feeding data
    void Flush (void); // flushes the queue and prepares a new cycle
    bool IsEmpty (void) const; // returns true if event queue is empty
  };


  
} // namespace
