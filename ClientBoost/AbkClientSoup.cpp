#include "AbkClientSoup.h"

#include "JsonParser.h"

// Required to add to log
#include "AbkClient.h"

namespace Abk
{

CBaseAbstractionSoup::CBaseAbstractionSoup(class CAbkClient *pClient) : CBaseAbstraction(pClient)
{
  m_pSoupSession = soup_session_new();
  // Time-out needs to be lower than whatever the main thread uses to detect if the connection is lost
  // (See LONGPOLL_FAILURE_TOLERANCE_MS in AbkClient.cpp)
  // Otherwise, the long-polling thread might block indefinitely in case of a lost connection,
  // which will lead to the main thread asserting correctly it's still in use.
  // 2 seconds timeout
  soup_session_set_timeout(m_pSoupSession, 2);
}

CBaseAbstractionSoup::~CBaseAbstractionSoup()
{
  if (m_pSoupSession)
  {
    g_object_unref(m_pSoupSession);
    m_pSoupSession = nullptr;
  }
}

bool CBaseAbstractionSoup::NavigatePut(LPCTSTR pszPath, int nSessionId, const std::string &strPutData)
{
  bool bSuccess = true;
  SoupMessage *pSoupMessage = soup_message_new(SOUP_METHOD_PUT, ConstructUrl(pszPath, nSessionId).c_str());
  if (!pSoupMessage)
  {
    g_logMain.Error(_T("Failed in creating PUT message for URL \"%s\""), pszPath);
    Close();
    return false;
  }
  GBytes *pBytesPut = g_bytes_new_static(strPutData.c_str(), strPutData.size());
  soup_message_set_request_body_from_bytes(pSoupMessage, "application/json", pBytesPut);
  g_bytes_unref(pBytesPut);
  GError *pError = NULL;
  GBytes *pBytes = soup_session_send_and_read(m_pSoupSession, pSoupMessage, NULL, &pError);
  // We don't care about the response, but free the memory associated with it and check for errors
  if (pBytes)
  {
    g_bytes_unref(pBytes);
  }
  if (pError)
  {
    g_logMain.Error(_T("Failed in putting file to URL \"%s\", error=\"%s\""), pszPath, pError->message);
    g_error_free(pError);
    bSuccess = false;
  }
  m_lastStatus = soup_message_get_status(pSoupMessage);
  if (m_lastStatus != SOUP_STATUS_OK)
  {
    g_logMain.Error(_T("PUT to URL \"%s\" returned status %d"), pszPath, m_lastStatus);
    bSuccess = false;
  }
  g_object_unref(pSoupMessage);
  return bSuccess;
}

std::string CBaseAbstractionSoup::NavigateGet(LPCTSTR pszPath, int nSessionId)
{
  std::string strResult;
  SoupMessage *pSoupMessage = soup_message_new(SOUP_METHOD_GET, ConstructUrl(pszPath, nSessionId).c_str());
  if (!pSoupMessage)
  {
    g_logMain.Error(_T("Failed in creating GET message for URL \"%s\""), pszPath);
    Close();
    return std::string();
  }
  GError *pError = NULL;
  GBytes *pBytes = soup_session_send_and_read(m_pSoupSession, pSoupMessage, NULL, &pError);
  if (pError)
  {
    g_logMain.Error(_T("Failed in getting URL \"%s\", error=\"%s\""), pszPath, pError->message);
    g_error_free(pError);
  }
  else
  {
    size_t nDataLen = 0;
    const gchar *pcszData = static_cast<const gchar *>(g_bytes_get_data(pBytes, &nDataLen));
    strResult.assign(pcszData, nDataLen);
    g_bytes_unref(pBytes);
  }
  m_lastStatus = soup_message_get_status(pSoupMessage);
  if (m_lastStatus != SOUP_STATUS_OK)
  {
    g_logMain.Error(_T("GET from URL \"%s\" returned status %d"), pszPath, m_lastStatus);
    strResult.clear();
  }
  g_object_unref(pSoupMessage);
  return strResult;
}

bool CBaseAbstractionSoup::NavigateGet(LPCTSTR pszPath, int nSessionId, std::ostream &out)
{
  // Needs seperate handling, because it might contain binary data
  bool bSuccess = false;
  SoupMessage *pSoupMessage = soup_message_new(SOUP_METHOD_GET, ConstructUrl(pszPath, nSessionId).c_str());
  if (!pSoupMessage)
  {
    g_logMain.Error(_T("Failed in creating GET message for URL \"%s\""), pszPath);
    Close();
    return false;
  }
  GError *pError = NULL;
  GBytes *pBytes = soup_session_send_and_read(m_pSoupSession, pSoupMessage, NULL, &pError);
  if (pError)
  {
    g_logMain.Error(_T("Failed in getting file from URL \"%s\", error=\"%s\""), pszPath, pError->message);
    g_error_free(pError);
  }
  else
  {
    size_t nDataLen = 0;
    const gchar *pcszData = static_cast<const gchar *>(g_bytes_get_data(pBytes, &nDataLen));
    out.write(pcszData, nDataLen);
    g_bytes_unref(pBytes);
    bSuccess = true;
  }
  m_lastStatus = soup_message_get_status(pSoupMessage);
  if (m_lastStatus != SOUP_STATUS_OK)
  {
    g_logMain.Error(_T("GET from URL \"%s\" returned status %d"), pszPath, m_lastStatus);
    bSuccess = false;
  }
  g_object_unref(pSoupMessage);
  return bSuccess;
}

std::string CBaseAbstractionSoup::NavigatePost(LPCTSTR pszPath, int nSessionId, const std::string &strPostData)
{
  SoupMessage *pSoupMessage = soup_message_new(SOUP_METHOD_POST, ConstructUrl(pszPath, nSessionId).c_str());
  if (!pSoupMessage)
  {
    g_logMain.Error(_T("Failed in creating POST message for URL \"%s\""), pszPath);
    Close();
    return std::string();
  }
  GBytes *pBytesPost = g_bytes_new(strPostData.c_str(), strPostData.size());
  soup_message_set_request_body_from_bytes(pSoupMessage, "application/json", pBytesPost);
  g_bytes_unref(pBytesPost);
  GError *pError = NULL;
  GBytes *pBytes = soup_session_send_and_read(m_pSoupSession, pSoupMessage, NULL, &pError);
  std::string strResult;
  if (pError)
  {
    g_logMain.Error(_T("Failed in posting to URL \"%s\", error=\"%s\""), pszPath, pError->message);
    g_error_free(pError);
  }
  else
  {
    size_t nDataLen = 0;
    const gchar *pcszData = static_cast<const gchar *>(g_bytes_get_data(pBytes, &nDataLen));
    strResult.assign(pcszData, nDataLen);
    g_bytes_unref(pBytes);
  }
  m_lastStatus = soup_message_get_status(pSoupMessage);
  if (m_lastStatus != SOUP_STATUS_OK)
  {
    g_logMain.Error(_T("POST to URL \"%s\" returned status %d"), pszPath, m_lastStatus);
    strResult.clear();
  }
  g_object_unref(pSoupMessage);
  return strResult;
}

bool CBaseAbstractionSoup::NavigateDelete(LPCTSTR pszPath, int nSessionId, const std::string &strDeleteData)
{
  bool bSuccess = true;
  SoupMessage *pSoupMessage = soup_message_new(SOUP_METHOD_DELETE, ConstructUrl(pszPath, nSessionId).c_str());
  if (!pSoupMessage)
  {
    g_logMain.Error(_T("Failed in creating DELETE message for URL \"%s\""), pszPath);
    Close();
    return false;
  }
  GError *pError = NULL;
  GBytes *pBytes = soup_session_send_and_read(m_pSoupSession, pSoupMessage, NULL, &pError);
  if (pError)
  {
    g_logMain.Error(_T("Failed in deleting file at URL \"%s\", error=\"%s\""), pszPath, pError->message);
    g_error_free(pError);
    bSuccess = false;
  }
  m_lastStatus = soup_message_get_status(pSoupMessage);
  g_object_unref(pSoupMessage);
  if (m_lastStatus != SOUP_STATUS_OK)
  {
    g_logMain.Error(_T("DELETE to URL \"%s\" returned status %d"), pszPath, m_lastStatus);
    bSuccess = false;
  }
  return bSuccess;
}

unsigned int CBaseAbstractionSoup::GetStatus() const
{
  return static_cast<unsigned int>(m_lastStatus);
}

bool CBaseAbstractionSoup::IsConnected() const
{
  return (m_pSoupSession != nullptr);
}

std::string CBaseAbstractionSoup::ConstructUrl(LPCTSTR pszPath, int nSessionId) const
{
  CString strPathAndQuery;
  strPathAndQuery.Format(_T("%s?") _T(ABK_QRY_SESSIONID) _T("=%d"), std::string(m_strServerUrl + pszPath).c_str(), nSessionId);
  return std::string(strPathAndQuery.GetString());
}

void CBaseAbstractionSoup::Close()
{
  if (m_pSoupSession)
  {
    g_object_unref(m_pSoupSession);
    m_pSoupSession = nullptr;
  }
}

bool CBaseAbstractionSoup::EnsureConnection()
{
  if (!m_pSoupSession)
  {
    m_pSoupSession = soup_session_new();
  }
  return (m_pSoupSession != nullptr);
}

void CBaseAbstractionSoup::SetServerAddr(const std::string &pszServerAddress, int nPort)
{
  m_strServerAddress = pszServerAddress;
  m_nPort = nPort;
  m_strServerUrl = "http://" + m_strServerAddress + ":" + std::to_string(m_nPort);
}

} // namespace Abk