#include "AbkClientAbstraction.h"

#include "JsonParser.h"

// Required to add to log
#include "AbkClient.h"
#include "AbkClientVar.h"

namespace Abk
{

CBaseAbstraction::CBaseAbstraction(class CAbkClient *pClient)
{
  m_pOwner = pClient;
}

CBaseAbstraction::~CBaseAbstraction() {}

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

bool CBaseAbstraction::GetVarOrMailboxList(LPCTSTR pszPath, std::vector<CString> *pGet)
{
	bool bSuccess = false;
	std::string response = NavigateGet(pszPath, -1);
	if (!response.empty())
	{
		CJsonParser jpVars(response.c_str());
		for (; !jpVars.IsDone(); ++jpVars)
		{
			if (jpVars.TestArray(ABK_RSP_VARLIST)) // is there "VarList": [
			{
				for (++jpVars; !jpVars.IsDone(); ++jpVars)
				{
					std::string strVarName;
					jpVars.ExtractValue(&strVarName);
					pGet->push_back(CString(CA2T(strVarName.c_str())));
				}
				bSuccess = true;
			}
			jpVars.SkipItem();
		}
	}
	return bSuccess;
}

bool CBaseAbstraction::GetVarOrMailboxMeta(const std::vector<LPCTSTR> &vectVarNames, std::vector<class CAbkClientMeta> *pGet, bool bMailboxFlag)
{
	bool bSuccess = false;
	LPCTSTR pszPath;
	if (bMailboxFlag)
		pszPath = _T(ABK_REQUESTURL_MAILBOXMETA);
	else
		pszPath = _T(ABK_REQUESTURL_VARMETA);
	CJsonFormatter jfRequest;
	{
		CJsonStreamArray jaVarList(&jfRequest, ABK_REQ_VARMETA_VARLIST); // "VarList": [
		std::vector<LPCTSTR>::const_iterator iterVarNames;
		for (iterVarNames = vectVarNames.begin(); iterVarNames != vectVarNames.end(); ++iterVarNames)
		{
			LPCTSTR pszVarName = *iterVarNames;
			jaVarList.WriteValue(CT2A(pszVarName, CP_UTF8)); // "Var1",
		}
	} // jaVarList falls out of scope => "]"
	jfRequest.Close(); // "}"

	std::string strResponse = NavigatePost(pszPath, -1, jfRequest.GetStream()->str());
	if (!strResponse.empty())
	{
		size_t nReserve = pGet->size() + vectVarNames.size();
		if (nReserve > pGet->capacity())
			pGet->reserve(nReserve);
		CJsonParser jpMeta(strResponse.c_str());
		for (; !jpMeta.IsDone(); ++jpMeta)
		{
			if (jpMeta.TestArray(ABK_RSP_VARMETA_METADATA)) // is there an array named "MetaData":
			{
				for (++jpMeta; !jpMeta.IsDone(); ++jpMeta)
				{
					CAbkClientMeta metaVar;
					bSuccess = metaVar.ExtractFromJson(jpMeta);
					metaVar.m_bIsMailbox = bMailboxFlag;
					pGet->push_back(metaVar); // append meta data to result list
				}
			}
			jpMeta.SkipItem(); // skip any other members
		}
		bSuccess = true;
	}
	else
	{
		m_pOwner->AddLogHttp(CAbkClient::LOGSEVERITY_WARNING, GetStatus(), pszPath, _T("POST"), strResponse.c_str());
	}
	return bSuccess;
}

} // namespace Abk