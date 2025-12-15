#include "AbkClientAbstraction.h"

#include <libsoup/soup.h>

#include "JsonParser.h"

// Required to add to log
#include "AbkClient.h"

namespace Abk
{

CBaseAbstraction::CBaseAbstraction(class CAbkClient *pClient) {}

CBaseAbstraction::~CBaseAbstraction() {}

bool CBaseAbstraction::NavigatePut(LPCTSTR pszPath, int nSessionId, const std::string &strPutData)
{
  return false;
}

std::string CBaseAbstraction::NavigateGet(LPCTSTR pszPath, int nSessionId)
{
  return std::string();
}

bool CBaseAbstraction::NavigateGet(LPCTSTR pszPath, int nSessionId, std::ostream &out)
{
  return false;
}

std::string CBaseAbstraction::NavigatePost(LPCTSTR pszPath, int nSessionId, const std::string &strPostData)
{
  return std::string();
}

bool CBaseAbstraction::NavigateDelete(LPCTSTR pszPath, int nSessionId, const std::string &strDeleteData)
{
  return false;
}

unsigned int CBaseAbstraction::GetStatus() const
{
  return 200;
}


bool CBaseAbstraction::IsConnected() const
{
  return false;
}

bool CBaseAbstraction::DeleteSession(int nSessionId)
{
  CJsonFormatter jfSend;
  return NavigateDelete(_T(ABK_REQUESTURL_SESSIONID), nSessionId, jfSend.GetStream()->str());
}

  /**
@param pszClientClass class name of the client
@param pszClientType type name of the client
@param pszClientSerial serial number or id of the client. If an empty string, the field will not be encoded
@param pszClientFwRev Firmware revision of the client. If an empty string, the field will not be encoded
@param pszClientHwRev Hardware revision of the client. If an empty string, the field will not be encoded
@return session id, -1 on error
*/
int CBaseAbstraction::ObtainSessionId(LPCTSTR pszClientClass, LPCTSTR pszClientType, LPCTSTR pszClientSerial /*=NULL*/,
                                      LPCTSTR pszClientFwRev, LPCTSTR pszClientHwRev)
{
  int nSessionId = -1; // result
  assert(pszClientClass);
  assert(pszClientType);
  CJsonFormatter jfPost;
  jfPost.WriteValue(ABK_REQ_SESSIONID_CLASS, CT2A(pszClientClass));
  jfPost.WriteValue(ABK_REQ_SESSIONID_TYPE, CT2A(pszClientType));

  if (pszClientSerial && pszClientSerial[0])
    jfPost.WriteValue(ABK_REQ_SESSIONID_SERIAL, CT2A(pszClientSerial));
  if (pszClientFwRev && pszClientFwRev[0])
    jfPost.WriteValue(ABK_REQ_SESSIONID_FWVERSION, CT2A(pszClientFwRev));
  if (pszClientHwRev && pszClientHwRev[0])
    jfPost.WriteValue(ABK_REQ_SESSIONID_HWVERSION, CT2A(pszClientHwRev));

  std::string pReturn = NavigatePost(_T(ABK_REQUESTURL_SESSIONID), -1, jfPost.GetStream()->str());
  if (pReturn.empty())
    return -1;

  // extract the session id
  CJsonParser parsResponse(pReturn.c_str());
  for (; !parsResponse.IsDone(); ++parsResponse)
    parsResponse.ExtractValue(ABK_RSP_SESSIONID_ID, &nSessionId);
  if (nSessionId < 0)
  {
    m_pOwner->AddLog(CAbkClient::LOGSEVERITY_ERROR, _T("Got no session id from server."));
    return -1;
  }
  return nSessionId;
}

void CBaseAbstraction::Close()
{
}

bool CBaseAbstraction::EnsureConnection()
{
  return false;
}

void CBaseAbstraction::SetServerAddr(const std::string &pszServerAddress, int nPort)
{
}

bool CBaseAbstraction::GetVarOrMailboxList(LPCTSTR pszPath, std::vector<CString> *pGet)
{
  return false;
}

bool CBaseAbstraction::GetVarOrMailboxMeta(const std::vector<LPCTSTR> &vectVarNames, std::vector<class CAbkClientMeta> *pGet, bool bMailboxFlag)
{
  return false;
}

} // namespace Abk