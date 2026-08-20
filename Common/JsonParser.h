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
// Filename:    JsonParser.h
// Created:     2012-07-19 (15:48)
// Author:      D. Burger
// Description: JSON parser
//------------------------------------------------------------------------------------------------


#pragma once


#include <string>


#define JSONPARSER_MAXNESTING 10

class CJsonParser
  {
  public:
    enum TYPE {TYPE_INVALID, TYPE_OBJECT, TYPE_ARRAY, TYPE_STRING, TYPE_NUMERIC, TYPE_END}; // types of the item at the iterator


  // data members
  protected:
    const char *m_pPos; // actual scanning position
    const char *m_pNameStart; // start of name
    const char *m_pNameEnd; // end of name (points to null-termination)
    const char *m_pValueStart; // see name
    const char *m_pValueEnd;
    TYPE m_nType; // type of the item at the current position
    bool m_bObject[JSONPARSER_MAXNESTING]; // true if currently parsing an object, false for an array
    int m_nNestingLevel; // current nesting level


  // construction/destruction
  public:
    CJsonParser (const char *pStartOfExpression);
    ~CJsonParser ();


  // operators
  public:
    CJsonParser& operator++ (); // iterate (prefix ++)

  // properties of the actual item
  public:
    TYPE GetType (void) const {return m_nType;} // return the type of the item at the current position
    bool IsInvalidType (void) const {return m_nType==CJsonParser::TYPE_INVALID;} // returns true if type is invalid
    bool GetName (std::string &strGet) const; // returns name of the object/array/value
    bool GetValueString (std::string &strGet) const; // returns the string form of the value
    bool GetValueDouble (double &dValueGet) const; // returns double precision form of the value
    bool GetValueInt (int &nValueGet) const; // returns integer form of the value
    bool GetValueBool (bool &bValueGet) const; // returns boolean value
    const char *GetPosition (void) const; // returns the actual scanning position, e.g. for error reporting
    bool IsError (void) const {return m_nType==CJsonParser::TYPE_INVALID;} // returns true if an error occured
    bool TestArray (const char *pszName) const; // returns true if current is start of array with specified name
    bool TestArray (std::string *pNameGet) const; // test for array and returns name of it
    bool TestArray (void) const; // test for array within an array
    bool TestObject (const char *pszName) const; // returns true if current is start of array with specified name
    bool TestObject (std::string *pNameGet) const; // test for object and returns name of it
    bool TestObject (void) const; // test for object within an array

  // item extraction
  public:
    bool ExtractValue (const char *pszName, std::string *pGet) const;   // extracts string from object
    bool ExtractValue (const char *pszName, int *pGet) const;           // extracts integer from object
    bool ExtractValue (const char *pszName, double *pGet) const;        // extracts double from object
    bool ExtractValue (const char *pszName, bool *pGet) const;          // extracts boolean from object
    bool ExtractValue (const char *pszName, time_t *pGet) const;        // extracts time from object
    bool ExtractValue (std::string *pGet) const;                        // gets string from array
    bool ExtractValue (int *pGet) const;                                // gets integer from array
    bool ExtractValue (double *pGet) const;                             // gets double from array
    bool ExtractValue (bool *pGet) const;                               // gets boolean from array
    bool ExtractValue (time_t *pGet) const;                             // gets time from array

  // operations
  public:
    bool IsDone (void) const; // returns true if scanning done (either with or w/o success)
    bool IsObject (void); // returns true if currently parsing an object
    bool SkipItem (const char *pszItemName=NULL); // skips the item (array or object)
    void Restart (const char *pStartOfExpression); // restarts the parser
    int GetNestingLevel (void) const {return m_nNestingLevel;} // returns the current nesting level
  
  // implementation
  protected:
    bool TestForName (const char *pszName) const; // tests if current position matches the name
    void Iterate (void); // iterates one step
    static const char *SkipWhite (const char * pStart); // skips white characters
    static const char *SearchStringEnd (const char *pScan); // searches the end of a string
    static const char *SearchNumericEnd (const char *pScan); // searches the end of a value
    void IsolateName (void); // extracts a member name
    TYPE IsolateValue (void); // extracts a value of a member
    void PushNesting (bool bObject); // pushes an object to the nesting stack
    void PopNesting (void); // pops a nesting level
    bool ScanDate (const char *pszDate, size_t nLen, time_t *pReturn) const; // scans date into time_t
    static bool UnescapeString (const char *pStart, const char *pEnd, std::string *pGet); // unescapes a string
    static bool CompareUnescapedString (const char *pszUnescaped, const char *pStart, const char *pEnd); // compares unescaped string with escaped string
  };




