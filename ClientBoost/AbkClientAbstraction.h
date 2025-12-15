#pragma once

#include "JsonFormatter.h"
#include "ValuesFromSpec.h"

namespace Abk
{
class CBaseAbstraction
{
protected:
  class CAbkClient *m_pOwner;

public:
  CBaseAbstraction(class CAbkClient *pClient);
  virtual ~CBaseAbstraction();

  virtual bool NavigatePut(LPCTSTR pszPath, int nSessionId, const std::string &strPutData);
  virtual std::string NavigateGet(LPCTSTR pszPath, int nSessionId);
  virtual bool NavigateGet(LPCTSTR pszPath, int nSessionId, std::ostream &out);
  virtual std::string NavigatePost(LPCTSTR pszPath, int nSessionId, const std::string &strPostData);
  virtual bool NavigateDelete(LPCTSTR pszPath, int nSessionId, const std::string &strDeleteData);
  virtual unsigned int GetStatus() const;

  virtual void Close();
  virtual bool EnsureConnection();
  virtual void SetServerAddr(const std::string &pszServerAddress, int nPort); // re-assigns the server address and port

  // This one is new
  virtual bool IsConnected() const;

  // TODO: I feel like these don't belong here, they should be in AbkClient.cpp using helper functions.
  //       For now the implementations have been moved here to the base class, though to avoid having to touch too many uses.
  bool DeleteSession(int nSessionId);
  int ObtainSessionId(LPCTSTR pszClientClass, LPCTSTR pszClientType, LPCTSTR pszClientSerial /*=NULL*/, LPCTSTR pszClientFwRev,
                      LPCTSTR pszClientHwRev);

  // This is just here because eliminating it from here would be a huge PITA
  template <typename U> bool SetVarOrMailboxValue(LPCTSTR pszPath, const char *pszName, const U *pSet)
  {
    CJsonFormatter jfRequest;
    {
      CJsonStreamArray jaGetList(&jfRequest, ABK_REQ_VARVALUE_PUTLIST);
      {
        CJsonStreamObject joVarPut(&jaGetList);
        joVarPut.WriteValue(ABK_REQ_VARVALUE_NAME, pszName);
        joVarPut.WriteValue(ABK_REQ_VARVALUE_VALUE, *pSet);
      }
    } // let array object fall out of scope
    jfRequest.Close();
    return NavigatePut(pszPath, -1, jfRequest.GetStream()->str());
  }

  bool GetVarOrMailboxList(LPCTSTR pszPath, std::vector<CString> *pGet);
  bool GetVarOrMailboxMeta(const std::vector<LPCTSTR> &vectVarNames, std::vector<class CAbkClientMeta> *pGet, bool bMailboxFlag);
};
} // namespace Abk