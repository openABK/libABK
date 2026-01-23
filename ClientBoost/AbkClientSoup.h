#pragma once

#include "AbkClientAbstraction.h"

#include <libsoup/soup.h>

namespace Abk
{

class CBaseAbstractionSoup : public CBaseAbstraction
{
public:
  CBaseAbstractionSoup(class CAbkClient *pClient);
  virtual ~CBaseAbstractionSoup();

  virtual bool NavigatePut(LPCTSTR pszPath, int nSessionId, const std::string &strPutData) override;
  virtual std::string NavigateGet(LPCTSTR pszPath, int nSessionId) override;
  virtual bool NavigateGet(LPCTSTR pszPath, int nSessionId, std::ostream &out) override;
  virtual std::string NavigatePost(LPCTSTR pszPath, int nSessionId, const std::string &strPostData) override;
  virtual bool NavigateDelete(LPCTSTR pszPath, int nSessionId, const std::string &strDeleteData) override;
  virtual unsigned int GetStatus() const override;

  virtual void Close() override;
  virtual bool EnsureConnection() override;
  virtual void SetServerAddr(const std::string &pszServerAddress, int nPort) override; // re-assigns the server address and port

  // This one is new
  virtual bool IsConnected() const override;

private:
  std::string ConstructUrl(LPCTSTR pszPath, int nSessionId) const;

  std::string m_strServerAddress;
  int m_nPort = 0;
  std::string m_strServerUrl;

  SoupSession *m_pSoupSession = nullptr;
  SoupStatus m_lastStatus = SOUP_STATUS_NONE;
};

}; // namespace Abk