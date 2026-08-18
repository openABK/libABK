#pragma once
enum variant_types
{
  VT_EMPTY = 0,
  VT_I2 = 2,
  VT_I4 = 3,
  VT_R4 = 4,
  VT_R8 = 5,
  VT_DATE = 7,
  VT_BSTR = 8,
  VT_ERROR = 10,
  VT_BOOL = 11,
  VT_UI4 = 19,
  VT_CLSID = 72, // Only for compatibility with part of VariantMath
  VT_TYPEMASK = 0xFFF,
};

#define V_I2(x) (x)->iVal
#define V_I4(x) (x)->lVal
#define V_R4(x) (x)->fltVal
#define V_R8(x) (x)->dblVal
#define V_DATE(x) (x)->dblVal
#define V_BOOL(x) (x)->boolVal
#define V_UI2(x) (x)->uiVal
#define V_UI4(x) (x)->ulVal
#define V_BSTR(x) (x)->bstrVal

// Actually "variant_types", but to build with existing code, we define VARTYPE as WORD
typedef WORD VARTYPE;