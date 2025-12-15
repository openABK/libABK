#include "AbkClientAbstraction.h"

#include <libsoup/soup.h>

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
  return false;
}

int CBaseAbstraction::ObtainSessionId(LPCTSTR pszClientClass, LPCTSTR pszClientType, LPCTSTR pszClientSerial /*=NULL*/, LPCTSTR pszClientFwRev, LPCTSTR pszClientHwRev)
{
  return -1;
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