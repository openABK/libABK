#include "AbkClientSoup.h"

#include <libsoup/soup.h>

#include "JsonParser.h"

// Required to add to log
#include "AbkClient.h"

namespace Abk
{

CBaseAbstractionSoup::CBaseAbstractionSoup(class CAbkClient *pClient) : CBaseAbstraction(pClient) {}

CBaseAbstractionSoup::~CBaseAbstractionSoup() {}

bool CBaseAbstractionSoup::NavigatePut(LPCTSTR pszPath, int nSessionId, const std::string &strPutData)
{
  return false;
}

std::string CBaseAbstractionSoup::NavigateGet(LPCTSTR pszPath, int nSessionId)
{
  return std::string();
}

bool CBaseAbstractionSoup::NavigateGet(LPCTSTR pszPath, int nSessionId, std::ostream &out)
{
  return false;
}

std::string CBaseAbstractionSoup::NavigatePost(LPCTSTR pszPath, int nSessionId, const std::string &strPostData)
{
  return std::string();
}

bool CBaseAbstractionSoup::NavigateDelete(LPCTSTR pszPath, int nSessionId, const std::string &strDeleteData)
{
  return false;
}

unsigned int CBaseAbstractionSoup::GetStatus() const
{
  return 200;
}

bool CBaseAbstractionSoup::IsConnected() const
{
  return false;
}

void CBaseAbstractionSoup::Close() {}

bool CBaseAbstractionSoup::EnsureConnection()
{
  return false;
}

void CBaseAbstractionSoup::SetServerAddr(const std::string &pszServerAddress, int nPort) {}

} // namespace Abk