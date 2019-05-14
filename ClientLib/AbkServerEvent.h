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
// Filename:    AbkServerEvent.h
// Created:     2012-08-02 (07:43)
// Author:      D. Burger
// Description: Object to store an event coming from server
//------------------------------------------------------------------------------------------------

#pragma once

#include <CrossPlatform.h>
#include <comutil.h>


class CJsonParserAtl;

namespace Abk
  {

class CAbkServerEvent
  {
  public:
    class CData
      {
      public:
        CString     m_strType;      // type of event
        time_t      m_tmEvent;      // time of event source (generation of the event)
        int         m_nSender;      // sender of the event (session id the sending client has or 0 if event came from server)
        CString     m_strRole;      // role of client
        std::string m_strParam;     // string param of the event (use std::string instead of CString since it is likely that the string param is itself a JSON object)
        double      m_dParam1;      // numeric param 1
        double      m_dParam2;      // numeric param 2

      public:
        BOOL IsPrimary (void) const; // returns TRUE if client is the primary handling client
      public:
        BOOL IsButton (void) const; // returns TRUE if event is a button event
        CString GetButtonName (void) const; // returns the button name
        BOOL GetButtonState (void) const; // returns the button state. TRUE=pressed, FALSE=released
        BOOL GetButtonFilter (int nSession) const; // returns TRUE if button event shall be handled
      public:
        BOOL IsWheel (void) const; // returns TRUE if event is a wheel movement
        CString GetWheelName (void) const; // gets the name of the wheel
        int GetWheelIncrements (void) const; // gets the wheel increments
        BOOL GetWheelFilter (int nSession) const; // returns TRUE if wheel event shall be handled
      public:
        BOOL IsIdentification (void) const; // returns TRUE if event is an identification event
        CString GetIndentificationMessage (void) const; // returns the message to be displayed as identification
      public:
        BOOL IsLimitAlert (void) const; // returns TRUE if event is a limit violation alert
        BOOL GetLimitParams (CString &strVarName, _variant_t &varValue, CString &strClass, BOOL &bNoClientSuppress) const; // retrieves the parameters of limit alert
      public:
        BOOL IsFormOpen (void) const; // returns TRUE if event is a form-open request
        BOOL IsFormClose (void) const; // returns TRUE if event is a form-close request
        CString GetFormName (void) const; // returns the name of the requested form
      public:
        BOOL IsVarlistChanged (void) const; // returns TRUE if it is a VarlistChanged event
      public:
        BOOL IsAppChanged (void) const; // returns TRUE if event to restart all apps
      public:
        BOOL IsAudioRecReq (void) const; // returns TRUE if event for requesting audio recorder
        int IsAudioRecStop (void) const; // server requests to stop an audio recording
        int GetAudioRecId (void) const; // returns audio recording ID of the recording or stop request
        double GetAudioRecTimeLimit (void) const; // returns audio recording time limit the server put in its request
      };

  protected:
  // data members
    CAbkMutex m_mutex; // protecting the object for thread-safety
    CAbkEvent m_evIsEmpty; // set if the object contains no valid event and is capable to receive data
    CData m_data;

  // construction/destructin/setup
  public:
    CAbkServerEvent(void);
    virtual ~CAbkServerEvent(void);

  // attributes and methods
  public:
    BOOL GetEvent (CData &dataGet); // peeks data from event and then marks it as empty
    BOOL SetEvent (CJsonParserAtl &jpEvent); // sets event


  // implementation
  protected:
  
  };


  } // namespace