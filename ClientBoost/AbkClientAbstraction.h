#pragma once

#include "JsonFormatter.h"
#include "ValuesFromSpec.h"

namespace Abk
{
  class CBaseAbstraction
  {
  public:
    CBaseAbstraction(class CAbkClient *pClient);
    virtual ~CBaseAbstraction();

    // bool NavigatePut(LPCTSTR pszPath, int nSessionId, CJsonFormatter *pPutData);
    // std::string NavigateGet(LPCTSTR pszPath, int nSessionId);
    // bool NavigateGet(LPCTSTR pszPath, int nSessionId, std::ostream &out);
    // std::string NavigatePost(LPCTSTR pszPath, int nSessionId, CJsonFormatter *pPostData);
    // bool NavigateDelete(LPCTSTR pszPath, int nSessionId, CJsonFormatter *pPostData);

    bool NavigatePut(LPCTSTR pszPath, int nSessionId, const std::string &strPutData);
    std::string NavigateGet(LPCTSTR pszPath, int nSessionId);
    bool NavigateGet(LPCTSTR pszPath, int nSessionId, std::ostream &out);
    std::string NavigatePost(LPCTSTR pszPath, int nSessionId, const std::string &strPostData);
    bool NavigateDelete(LPCTSTR pszPath, int nSessionId, const std::string &strDeleteData);

    unsigned int GetStatus() const;

    // This one is new
    bool IsConnected() const;

    // TODO: I feel like these don't belong here, they should be in AbkClient.cpp
    bool DeleteSession(int nSessionId); // deletes the actual session
    int ObtainSessionId(LPCTSTR pszClientClass, LPCTSTR pszClientType, LPCTSTR pszClientSerial /*=NULL*/, LPCTSTR pszClientFwRev, LPCTSTR pszClientHwRev);
    void Close();
    bool EnsureConnection();
    void SetServerAddr(const std::string &pszServerAddress, int nPort); // re-assigns the server address and port

    // This is just here because eliminating it from here would be a huge PITA
    template <typename U>
    bool SetVarOrMailboxValue(LPCTSTR pszPath, const char *pszName, const U *pSet)
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