// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------------------------
//  _____  __   __  _____  _    _        ____              
// |  ___||  \ /  ||  _  \| |  | |      / ___|  _   _  ___ 
// |  __| |   V   ||  -  /| |  | |  __  \___ \ | | | |/ __|
// | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
// |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
//                                              |___/      
// Filename:    JsonFormatter.h
// Created:     2012-07-25 (08:19)
// Author:      D. Burger
// Description: JSON formatter for formatting data into a std::stringstream
//------------------------------------------------------------------------------------------------


#pragma once



#include <iostream>
#include <string>
#include <sstream>


class CJsonStreamBase;

CJsonStreamBase &operator << (CJsonStreamBase &rDump, const char *pszValue);
CJsonStreamBase &operator << (CJsonStreamBase &rDump, int nValue);
CJsonStreamBase &operator << (CJsonStreamBase &rDump, double nValue);
CJsonStreamBase &operator << (CJsonStreamBase &rDump, const std::string &strValue);
CJsonStreamBase &operator << (CJsonStreamBase &rDump, bool bValue);


class CJsonStreamBase
  {
  friend CJsonStreamBase &operator << (CJsonStreamBase &rDump, const char *pszValue);
  friend CJsonStreamBase &operator << (CJsonStreamBase &rDump, int nValue);
  friend CJsonStreamBase &operator << (CJsonStreamBase &rDump, double nValue);
  friend CJsonStreamBase &operator << (CJsonStreamBase &rDump, const std::string &strValue);
  friend CJsonStreamBase &operator << (CJsonStreamBase &rDump, bool bValue);

  // data members
  protected:
    CJsonStreamBase *m_pParent; // parent object or array
    int m_nMemberCount; // counter of members
    std::stringstream *m_pDump; // string stream to dump the formatted json to
    CJsonStreamBase *m_pChild; // pointer to a formatter formatting the actual member
    const char *m_strCloseTag; // close tag string
    bool m_bClosed; // true if closing object tag is already written

  // constructor/destructor
  public:
    CJsonStreamBase (CJsonStreamBase *pParent);
    virtual ~CJsonStreamBase (void);
  
  // methods
  public:
    void Close (void); // closes the object
    void BeginMember (void); // writes an array/member separator, shall be called before the array member is formatted

  // implementation
  protected:
    void FormatDate (time_t tmDate); // formats date
  };


class CJsonStreamObject;
class CJsonFormatter;


class CJsonStreamArray : public CJsonStreamBase
  {
  // constructor/destructor
  public:
    CJsonStreamArray (CJsonStreamObject *pParent, const char *pszName);
    CJsonStreamArray (CJsonStreamObject *pParent, const std::string &strName);
    CJsonStreamArray (CJsonStreamArray *pParent);
    virtual ~CJsonStreamArray (void);

  // write values as array member
  public:
    void WriteValue (int nValue); // writes integer value
    void WriteValue (bool bValue); // writes bool value
    void WriteValue (double dValue); // writes double precision value
    void WriteValue (const char *pszValue); // writes string value
    void WriteValue (const std::string &strValue); // writes string value
    void WriteValue (time_t tmDateLocal); // writes date and time
    void WriteValue (const CJsonFormatter &objPut); // writes object

  // implementation
  protected:
  };


class CJsonStreamObject : public CJsonStreamBase
  {
  // constructor/destructor
  public:
    CJsonStreamObject (CJsonStreamObject *pParent, const char *pszName);
    CJsonStreamObject (CJsonStreamObject *pParent, const std::string &strName);
    CJsonStreamObject (CJsonStreamArray *pParent);
    virtual ~CJsonStreamObject (void);

  // write values as object member
  public:
    void WriteValue (const char *pszName, int nValue); // writes integer value
    void WriteValue (const char *pszName, bool bValue); // writes integer value
    void WriteValue (const char *pszName, double dValue); // writes double precision value
    void WriteValue (const char *pszName, const char *pszValue); // writes string value
    void WriteValue (const char *pszName, const std::string &strValue); // writes string value
    void WriteValue (const char *pszName, time_t tmDateLocal); // writes date and time
    void WriteValue (const char *pszName, const CJsonFormatter &objPut); // writes object
    void WriteValue (const CJsonFormatter &objPut); // writes members of an object into the object

  // methods
  public:
    void BeginMember (void); // begins a member
    void BeginMember (const char *pszName);

  // implementation
  protected:
  };


// provides a string stream with JSON formatting functions
class CJsonFormatter : public CJsonStreamObject
  {
  friend class CJsonStreamArray;
  // data members
  protected:
    std::stringstream m_strFormat; // buffer for formattig the json string

  // constructor/destructor/init
  public:
    CJsonFormatter (void);
    virtual ~CJsonFormatter (void);
    void InitNew (void); // inits to virgin state

  // stream access
  public:
    std::stringstream *GetStream (void); // closes the object and returns the string stream

  };

