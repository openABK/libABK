#pragma once

#ifdef NO_WINDOWS
struct VARIANT
{
  WORD vt;

  union
  {
    BOOL boolVal;

    int8_t cVal;
    int16_t iVal;
    int32_t lVal;

    uint8_t bVal;
    uint16_t uiVal;
    uint32_t ulVal;

    float fltVal;
    double dblVal;
    double date;
    // Pointer to a string
    bstr_t bstrVal;
    int intVal;
  };
};

typedef VARIANT *LPVARIANT;
typedef VARIANT VARIANTARG;

typedef DWORD LCID;
#endif

#ifdef NO_WINDOWS
// Forward declaration
class _variant_t;
static HRESULT VariantClear(_variant_t *pVariant);
// Define in "VariantMath.h"
extern "C"
{
  HRESULT VarCat(VARIANTARG *pLeft, VARIANTARG *pRight, VARIANTARG *pResult);
  HRESULT VariantChangeType(LPVARIANT pvargDest, LPVARIANT pvargSrc, USHORT wFlags, VARTYPE vt);
}
#endif

#ifndef SIMPLE_COMPARE
// Forward declaration also not elegant
typedef _variant_t variant_t;
HRESULT EmbuVarCmp (variant_t *pVarLeft, variant_t *pVarRight, int lcid, int nFlags);
#endif

enum var_compare_ops
{
  VARCMP_NULL,
  VARCMP_EQ,
  VARCMP_LT,
  VARCMP_GT
};

#define VARIANT_FALSE S_FAIL
#define VARIANT_TRUE S_OK
#define LOCALE_INVARIANT 0

#ifdef NO_ATL
class _variant_t : public VARIANT
{
public:
  _variant_t(long value, variant_types type = VT_I4)
  {
    lVal = value;
    vt = type;
  }

  _variant_t(int32_t value, variant_types type = VT_I4)
  {
    lVal = value;
    vt = type;
  }

  _variant_t(int16_t value, variant_types type = VT_I2)
  {
    iVal = value;
    vt = type;
  }

  _variant_t(double value, variant_types type = VT_R8)
  {
    dblVal = value;
    vt = type;
  }

  _variant_t(bool value)
  {
    boolVal = value;
    vt = VT_BOOL;
  }
  /*
    _variant_t(const void *value) {
      iVal = (intptr_t)value;
    }
    */

  _variant_t(const _variant_t *ptr)
  {
    // Directly use copy to avoid checking any uninitialized data member
    Copy(*ptr);
  }

  _variant_t &operator=(const _variant_t &right)
  {
    ClearIfStr();
    Copy(right);

    return *this;
  }

  _variant_t &operator=(const OLECHAR *pszStr)
  {
    ClearIfStr();
    SetString(pszStr);

    return *this;
  }

#if 0
#if (defined(OLE2ANSI) && defined(UNICODE)) || (!defined(OLE2ANSI) && !defined(UNICODE))
  // This overload is required, if BSTR does not match LPCTSTR
  _variant_t &operator=(BSTR bstr)
  {
    ClearIfStr();
#ifdef OLE2ANSI
    SetString(CA2T(bstr));
#else // OLECHAR is WCHAR
    SetString(CW2T(bstr));
#endif

    return *this;
  }
#endif
#endif

  // Comparison operators
  bool operator==(const _variant_t &right) const
  {
#ifdef SIMPLE_COMPARE
    // TODO: How to compare strings?
    bool result = false;
    if (vt == VT_BSTR || right.vt == VT_BSTR)
      STUBBED();

    if (vt == right.vt && lVal == right.lVal)
      result = true;

    if (vt != right.vt)
      STUBBED();

    return result;
#else
    return EmbuVarCmp((_variant_t *)this, (variant_t *)&right, LOCALE_INVARIANT, 0) == VARCMP_EQ;
#endif
  }

  bool operator!=(const _variant_t &right) const
  {
    return !(*this == right);
  }

  _variant_t(const OLECHAR *pszStr)
  {
    vt = VT_EMPTY;
    SetString(pszStr);
  }

  // I want to make sure that this class is copy-constructable
  _variant_t(const _variant_t &right)
  {
    Copy(right);
  }

  _variant_t()
  {
    Reset();
  }

// The optimization is disabled on GCC here, because GCC apparently thinks the members won't be accessed after delete
// and optimizes away the code, leading to double frees. Because variant_t are used with placement new and delete,
// the stack will need to be "deinitialized" properly (setting bstrVal set to NULL, at the very least).
// Meaning, this can't be skipped.
#ifdef __GNUC__
#pragma GCC push_options
#pragma GCC optimize ("O0")
#endif
  ~_variant_t()
  {
    ClearIfStr();
  }
#ifdef __GNUC__
#pragma GCC pop_options
#endif

#ifdef ALL_VT
#error "CAST_NUMBER_VT is not defined for all VT"
#endif 

#define CAST_NUMBER_VT(vt) \
  switch (vt) \
  { \
  case VT_I2: \
    return iVal; \
  case VT_I4: \
    return lVal; \
  case VT_UI4: \
    return ulVal; \
  case VT_R4: \
    return fltVal; \
  case VT_R8: \
    return dblVal; \
  case VT_BOOL: \
    return boolVal; \
  default: \
    ASSERT(false); \
  }



  // conversion operators
  operator bool() const
  {
    CAST_NUMBER_VT(vt);
    return boolVal;
  }

  operator double() const
  {
    CAST_NUMBER_VT(vt);
    return dblVal;
  }

#ifdef ALL_VT
  operator int8_t() const
  {
    CAST_NUMBER_VT(vt);
    return cVal;
  }
#endif

  operator int16_t() const
  {
    CAST_NUMBER_VT(vt);
    return iVal;
  }

  operator int32_t() const
  {
    CAST_NUMBER_VT(vt);
    return lVal;
  }

#ifdef ALL_VT
  operator uint8_t() const
  {
    CAST_NUMBER_VT(vt);
    return bVal;
  }

  operator uint16_t() const
  {
    CAST_NUMBER_VT(vt);
    return uiVal;
  }
#endif

  operator uint32_t() const
  {
    CAST_NUMBER_VT(vt);
    return ulVal;
  }

  operator float() const
  {
    CAST_NUMBER_VT(vt);
    return fltVal;
  }

  // No bstr_t conversion operator, because it creates a new string and thus needs to be owned
  operator CString() const
  {
    _variant_t orig = *this;
    _variant_t temp;
    VariantChangeType(&temp, &orig, 0, VT_BSTR);
    ASSERT(temp.vt == VT_BSTR);
    return temp.bstrVal;
  }

  operator VARIANT *()
  {
    return this;
  }

private:
  inline void Copy(const _variant_t &right)
  {
    vt = right.vt;
    if (vt == VT_BSTR)
    {
#ifdef OLE2ANSI
      size_t length = strlen(right.bstrVal) + 1;
#else
      size_t length = wcslen(right.bstrVal) + 1;
#endif
      bstrVal = new OLECHAR[length]{0};
      memcpy(bstrVal, right.bstrVal, length * sizeof(OLECHAR));
    }
    else
    {
      // Always has to be the biggest of them
      dblVal = right.dblVal;
    }
  }

  inline void ClearIfStr()
  {
    if ((vt == VT_BSTR) && bstrVal)
    {
      delete[] bstrVal;
      bstrVal = NULL;
    }
  };

  inline void Reset()
  {
    // ClearVariant probably does not reset this
    // TixOperate relies on this (in VarNeg when pResult == pOp1)
    // iVal = 0;
    vt = VT_EMPTY;
  }

  inline void SetString(const OLECHAR *pszStr)
  {
    // Length including null-terminator
#ifdef OLE2ANSI
    size_t length = strlen(pszStr) + 1;
#else
    size_t length = wcslen(pszStr) + 1;
#endif
    bstrVal = new OLECHAR[length];
    memcpy(bstrVal, pszStr, length * sizeof(OLECHAR));
    vt = VT_BSTR;
  }

public:
  void Clear()
  {
    ClearIfStr();
    Reset();
  }

  void SetBool(bool value)
  {
    ClearIfStr();
    boolVal = value;
    vt = VT_BOOL;
  }

#ifdef ALL_VT
  void SetI1(int8_t value)
  {
    ClearIfStr();
    cVal = value;
    vt = VT_I1;
  }
#endif

  void SetI2(int16_t value)
  {
    ClearIfStr();
    iVal = value;
    vt = VT_I2;
  }

  void SetI4(int32_t value)
  {
    ClearIfStr();
    lVal = value;
    vt = VT_I4;
  }

  // TODO: unsigned variants

  void SetR4(float value)
  {
    ClearIfStr();
    fltVal = value;
    vt = VT_R4;
  }

  void SetR8(double value)
  {
    ClearIfStr();
    dblVal = value;
    vt = VT_R8;
  }
};

static_assert(sizeof(_variant_t) == sizeof(VARIANT), "_variant_t likely has additional members. Should be just a wrapper around VARIANT");
#endif // NO_ATL

#if defined(NO_WINDOWS) || defined(WINELIB)
// Neither Winelib nor Linux have this
typedef _variant_t variant_t;
#endif

static HRESULT VariantClear(_variant_t *pVariant)
{
  pVariant->Clear();
  return S_OK;
}


typedef BOOL VARIANT_BOOL;

#ifndef AFXAPI
#define AFXAPI
#endif
