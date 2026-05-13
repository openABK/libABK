#include "stdafx.h"

#include "AbkClient.h"
#include "AbkClientDaq.h"
#include "JsonParserAtl.h"
#include "StopWatch.h"

#include "AbkClientSoup.h"

#define DAQ_TIMEOUT 10000 // mutex timeout in ms
#define MIME_TYPE_TEXT "text/plain"

#define LONGPOLL_FAILURE_TOLERANCE_MS 5000 // maximum time span in ms where errors are tolerated
#define LONGPOLL_FAILURE_RECOVERY_MS 100 // 500 // time the longpoll thread is stalled after an http error occured. Used to reduce error log entry rate

//#define LOG_BOOST_ABK
// LOG_BOOST_ABK is defined in case you want logging
#ifndef LOG_BOOST_ABK

// Audio-recording returns 403 even though it's working
#define LOGGER_QUIRK

// Allow other translation units to configure for 8-bit only audio
bool g_bForce8BitAudioAbk = false;

struct CustomLog
{
	template<typename T>
	CustomLog& operator << (T &stream)
	{
		return *this;
	}
};

// define std::endl for CustomLog
namespace std
{
	inline CustomLog& endl(CustomLog& stream)
	{
		return stream;
	}
}

CustomLog nullStream;

static inline CustomLog& Log()
{
	return nullStream;
}

static inline CustomLog& LogErr()
{
	return nullStream;
}

#else

static inline std::ostream& Log()
{
	return std::cout;
}

static inline std::ostream& LogErr()
{
	return std::cerr;
}

#endif


namespace Abk
{


/** Thread which requests in order to generate a little traffic
@param vpThis Pointer to the temporary connection object
@return always 0
*/
/*static*/ DWORD WINAPI CAbkClient::CTempConnection::RequestThreadS(void *vpThis)
{
	CTempConnection *pThis = static_cast<CTempConnection *>(vpThis);
	for (size_t nRequest = 0; nRequest < REQUEST_COUNT; ++nRequest)
	{
		pThis->m_pClient->NavigateGet(_T(ABK_REQUESTURL_CURRENTTIME), -1);
	}
	pThis->m_thread.Stop();
	return 0;
}

/** Constructor
@param pClient http client used to send the requests
*/
CAbkClient::CTempConnection::CTempConnection(CBaseAbstraction *pClient)
		: m_thread(RequestThreadS, this) ,m_pClient(pClient)
{

}

/** dtor
 */
CAbkClient::CTempConnection::~CTempConnection()
{
	while (m_thread.IsRunning())
		Sleep(1);
}

bool CAbkClient::IsConnected() const
{
	CClientPtrRefConst a(m_pClientAux);
	CClientPtrRefConst e(m_pClientEvent);

	return ((m_nPort > 0) && a.IsValid() && e.IsValid() && e->IsConnected() && a->IsConnected());
}

void CAbkClient::AddLog(CAbkClient::LOGSEVERITY nSeverity, LPCTSTR pszMessage, ...)
{
	va_list args;
	va_start(args, pszMessage);
	//AddLogV(nSeverity,pszMessage,args);
	vprintf(CT2A(pszMessage), args);
	putc('\n', stdout);
	va_end(args);
}

CAbkClient::CAbkClient(bool bSuppressLog /*= false*/, bool bTextTranslationByServer /*=true*/)
{
	m_bTerminateLongPoll = false;
	m_pNextEventData = NULL;
	m_bSuppressLog = bSuppressLog;
	m_bTextTranslationByServer = bTextTranslationByServer;
}


/*virtual*/ CAbkClient::~CAbkClient()
{
	TidyUp(false);
}

bool CAbkClient::Create(LPCTSTR pszServerAddress, int nPort, CAbkServerEvent *pEventRxBuffer, LPCTSTR pszClientClass, LPCTSTR pszClientType, LPCTSTR pszClientSerial, LPCTSTR pszClientFwRev /*=NULL*/, LPCTSTR pszClientHwRev /*=NULL*/)
{
	ASSERT (pszClientSerial && pszClientSerial[0]); // the serial number is mandatory, since this device is identified when saving or loading client states!!
	bool bSuccess = false;
	std::stringstream sstream;

	m_strClientClass = CT2A(pszClientClass);
	m_strClientType = CT2A(pszClientType); // regular client type, except when querying firmware info
	if (pszClientSerial)
		m_strClientSerial = CT2A(pszClientSerial);

	if (pszClientFwRev)
		m_strClientFwRev = CT2A(pszClientFwRev);

	if (pszClientHwRev)
		m_strClientHwRev = CT2A(pszClientHwRev);

	sstream << nPort;
	m_strPort = std::string(sstream.str());


	m_nPort = nPort;
	m_strServerAddress = std::string(CT2A(pszServerAddress));
	m_pNextEventData = pEventRxBuffer;

#ifdef LOG_BOOST_ABK
	Log() << "Printing port: " << m_strPort << std::endl;
#endif

	if (nPort > 0)
	{
		// create clients
		m_pClientAux = new CBaseAbstractionSoup(this);
		m_pClientEvent = new CBaseAbstractionSoup(this);
		CClientPtrRef pClientAux(m_pClientAux);
		CClientPtrRef pClientEvent(m_pClientEvent);
		pClientAux->SetServerAddr(m_strServerAddress, nPort);
		pClientEvent->SetServerAddr(m_strServerAddress, nPort);

		pClientAux->EnsureConnection();
		pClientEvent->EnsureConnection();

		// tentative
		if (1)
		{
			for (size_t nTemp = 0; nTemp < 2; ++nTemp)
			{
				CBaseAbstraction *pClient = m_pClientAux.GetPtr();
				CTempConnection connTemp[3] = {CTempConnection(pClient), CTempConnection(pClient), CTempConnection(pClient)};
			}
		}

		// Connect and obtain session id
		m_nSessionId = pClientAux->ObtainSessionId(pszClientClass, pszClientType, pszClientSerial, pszClientFwRev, pszClientHwRev);
		if (m_nSessionId >= 0)
		{
			// Start long-polling thread
			m_longPollThread = boost::thread(LongPollThreadS, this);

			bSuccess = true;
		}
	}

	return true;
}

std::string CAbkClient::GetServerInfo()
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid()); // no connection with the aux http client established
	if (!pClientAux.IsValid())
		return NULL;
	if (!IsConnected())
		return NULL;
	return pClientAux->NavigateGet(_T(ABK_REQUESTURL_SERVERINFO), -1);
}

bool CAbkClient::GetServerInfo(CString & strProtocolVersion, CString & strInterfaceVersion, CString & strFwVersion, CString & strHwVersion, CString & strServerName, CString & strServerType, CString & strDescUrl)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid()); // no connection with the aux http client established
	if (!pClientAux.IsValid())
		return false;
	if (!IsConnected())
		return false;
	return !(pClientAux->NavigateGet(_T(ABK_REQUESTURL_SERVERINFO), -1)).empty();
}

std::string CAbkClient::GetInterfaceStatistics(void)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid()); // no connection with the aux http client established
	if (!pClientAux.IsValid())
		return NULL;
	if (!IsConnected())
		return NULL;
	return pClientAux->NavigateGet(_T(ABK_REQUESTURL_INTERFACESTATS), -1);
}

/** Sends a client event to the server

	@param pszEventType
		Event type string
	@param pszStringParam
		String parameter
	@param jfString
		formatted objected to be sent as string paramter
	@param dParam1
		numeric parameter 1
	@param dParam2
		numeric parameter 2
	@param bPrivate
		If true, client intends to process the reflected event
		by itself and the server shall not reflect the event
		to other clients
	@return
		true on success, false on error
*/
bool CAbkClient::SendEvent(const char *pszEventType, LPCTSTR pszStringParam, double dParam1, double dParam2, bool bPrivate)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	CJsonFormatter jfEvent; // whole event formatted in json
	jfEvent.WriteValue(ABK_RSP_CLIENTEVENT_SENDER, m_nSessionId);
#ifdef WINCE
	time_t tmNow = time(NULL); // get actual time
#else
	time_t tmNow;
	time(&tmNow); // get actual time
#endif
	jfEvent.WriteValue(ABK_RSP_CLIENTEVENT_TIME, tmNow);
	jfEvent.WriteValue(ABK_RSP_CLIENTEVENT_TYPE, pszEventType);
	jfEvent.WriteValue(ABK_RSP_SERVEREVENT_STRPARAM, CT2A(pszStringParam, CP_UTF8));
	jfEvent.WriteValue(ABK_RSP_SERVEREVENT_PARAM1, dParam1);
	jfEvent.WriteValue(ABK_RSP_SERVEREVENT_PARAM2, dParam2);
	jfEvent.WriteValue(ABK_RSP_CLIENTEVENT_PRIVATE, bPrivate);
	jfEvent.Close();
  STOPWATCH_GUARD_NAME(SendEvent);
	bool bSuccess = pClientAux->NavigatePut(_T(ABK_REQUESTURL_CLIENTEVENT), -1, jfEvent.GetStream()->str());
	return bSuccess;
}

/** @copydoc CAbkClient::SendEvent
*/
bool CAbkClient::SendEvent(const char *pszEventType, CJsonFormatter &jfString, double dParam1, double dParam2, bool bPrivate)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	CJsonFormatter jfEvent; // whole event formatted in json
	jfEvent.WriteValue(ABK_RSP_CLIENTEVENT_SENDER, m_nSessionId);
#ifdef WINCE
	time_t tmNow = time(NULL); // get actual time
#else
	time_t tmNow;
	time(&tmNow); // get actual time
#endif
	jfEvent.WriteValue(ABK_RSP_CLIENTEVENT_TIME, tmNow);
	jfEvent.WriteValue(ABK_RSP_CLIENTEVENT_TYPE, pszEventType);
	jfEvent.WriteValue(ABK_RSP_SERVEREVENT_STRPARAM, jfString.GetStream()->str().c_str());
	jfEvent.WriteValue(ABK_RSP_SERVEREVENT_PARAM1, dParam1);
	jfEvent.WriteValue(ABK_RSP_SERVEREVENT_PARAM2, dParam2);
	jfEvent.WriteValue(ABK_RSP_CLIENTEVENT_PRIVATE, bPrivate);
	jfEvent.Close();
	return pClientAux->NavigatePut(_T(ABK_REQUESTURL_CLIENTEVENT), -1, jfEvent.GetStream()->str());
}

bool CAbkClient::SendAlertConfirmEvent(LPCTSTR pszAlertClassName, int nSeverity, int nMerged, bool bPermanent, bool bSuppressed, bool bTimeout)
{
	assert(this);
	assert((!bSuppressed) || (bSuppressed && !bTimeout)); // if suppressed, timeout must not be set! Please check how you call the function
	CJsonFormatter jfSend;
	std::string strClassA(CT2A(pszAlertClassName, CP_UTF8));
	jfSend.WriteValue(ABK_ALERTCONFIRM_CLASS, strClassA.c_str()); // "Class": "KickDown"
	jfSend.WriteValue(ABK_ALERTCONFIRM_SEVERITY, nSeverity); // "Severity": 3
	jfSend.WriteValue(ABK_ALERTCONFIRM_COUNT, nMerged); // "Merged": 5
	jfSend.WriteValue(ABK_ALERTCONFIRM_SUPPRESSED, bSuppressed); // "Suppressed": false
	jfSend.WriteValue(ABK_ALERTCONFIRM_TIMEOUT, bTimeout); // "Timeout": false
	jfSend.WriteValue(ABK_ALERTCONFIRM_PERMASUPPRBYUSER, bPermanent); // "PermanentSuppressedByUser": false
	jfSend.Close();
	// TODO:
	return SendEvent(ABK_CLIENTEVENT_ALERT_CONFIRM, jfSend, 0, 0, false);
	//return true;
}

/** Called when server sent an event

	@details
		You can override this function to receive events.
		This function is called in the long polling
		threads context

	@param pEventData
		Pointer to event with the params of thevent
	@return
		Pointer to event object receiving the next event.
		You can either return the same event object when you have only
		one buffer for event reception.
		Alternatively you can return a pointer to another event object if you have a queue.
		In this case pEventData must be deleted manually
*/
CAbkServerEvent *CAbkClient::OnServerEvent(CAbkServerEvent *pEventData)
{
	// default implementation: use the same event object for the next event
	return pEventData;
}

/** Download a file to a stream

	@param pszUrl
		URL on the server where do load from
	@param out
		Output stream where the file should be written to
	@param pfnReadCallback
		Callback called for status updates during download (optional)
	@param dwCookie
		Callback data
	@return
		true on success, false if error occured
*/
bool CAbkClient::DownloadFile(LPCTSTR pszUrl, std::ostream &out, PFNSTATUSCALLBACK pfnReadCallback, DWORD_PTR dwCookie)
{
	CClientPtrRef pClientAux(m_pClientAux);
	if (!pClientAux.IsValid())
		return false;
	bool bSuccess = false;
	bSuccess = pClientAux->NavigateGet(pszUrl, -1, out);
	return bSuccess;
}

/** Download a file to a certain location

	@param pszUrl
		URL on the server where do load from
	@param pszStorePath
		Absolute local file path where to store to
	@param pfnReadCallback
		Callback called for status updates during download (optional)
	@param dwCookie
		Callback data
	@return
		true on success, false if error occured
*/
bool CAbkClient::DownloadFile(LPCTSTR pszUrl, LPCTSTR pszStorePath, PFNSTATUSCALLBACK pfnReadCallback, DWORD_PTR dwCookie)
{
	std::ofstream file(CT2A(pszStorePath), std::ofstream::out);
  bool bSuccess = DownloadFile(pszUrl, file);
  if (!bSuccess)
  {
    file.close();
    // File download is not successful or incomplete, delete the file if it was created
    DeleteFile(pszStorePath);
  }
	return bSuccess; 
}

/** Manually receive an event */
bool CAbkClient::ReceiveEvent()
{
	CClientPtrRef pClientEvent(m_pClientEvent);
	assert(pClientEvent.IsValid());
	if (!pClientEvent.IsValid())
		return false;
	std::string strResponse = pClientEvent->NavigateGet(_T(ABK_REQUESTURL_SERVEREVENT), m_nSessionId); // request event and wait for answer (blocks here)
	return !strResponse.empty();
}

/** Sets a variable value

	@param pszVarName
		Name of variable to be set
	@param pSet
		Pointer to new value
*/
bool CAbkClient::SetVarValue(LPCTSTR pszVarName, const CString &strSet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	const std::string strValue((CT2A(strSet)));
	return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_VARVALUE), CT2A(pszVarName, CP_UTF8), &strValue);
}

bool CAbkClient::SetVarValue(LPCTSTR pszVarName, const std::string &strSet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_VARVALUE), CT2A(pszVarName, CP_UTF8), &strSet);
}

bool CAbkClient::SetVarValue(LPCTSTR pszVarName, const double dSet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_VARVALUE), CT2A(pszVarName, CP_UTF8), &dSet);
}

bool CAbkClient::SetVarValue(LPCTSTR pszVarName, const int nSet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_VARVALUE), CT2A(pszVarName, CP_UTF8), &nSet);
}

bool CAbkClient::SetVarValue(LPCTSTR pszVarName, const bool bSet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_VARVALUE), CT2A(pszVarName, CP_UTF8), &bSet);
}

/** Sets a mailbox value

	@param pszMailboxName
		Name of the mailbox to be set
	@param pSet
		Pointer to new value
*/
bool CAbkClient::SetMailboxValue(LPCTSTR pszMailboxName, const CString &strSet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	const std::string strValue((CT2A(strSet)));
	return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_MAILBOXVALUE), CT2A(pszMailboxName, CP_UTF8), &strValue);
}

bool CAbkClient::SetMailboxValue(LPCTSTR pszMailboxName, const std::string &strSet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_MAILBOXVALUE), CT2A(pszMailboxName, CP_UTF8), &strSet);
}

bool CAbkClient::SetMailboxValue(LPCTSTR pszMailboxName, const double dSet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_MAILBOXVALUE), CT2A(pszMailboxName, CP_UTF8), &dSet);
}

bool CAbkClient::SetMailboxValue(LPCTSTR pszMailboxName, const int nSet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_MAILBOXVALUE), CT2A(pszMailboxName, CP_UTF8), &nSet);
}

bool CAbkClient::SetMailboxValue(LPCTSTR pszMailboxName, const bool bSet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	return pClientAux->SetVarOrMailboxValue(_T(ABK_REQUESTURL_MAILBOXVALUE), CT2A(pszMailboxName, CP_UTF8), &bSet);
}

/** Retrieves the current time of the server as local time
	@param pGet
		Pointer to return the server time, returned in client local time
	@return
		true on success, false on error
*/
bool CAbkClient::GetCurrentServerTime(time_t *pGet)
{
	bool bSuccess = FALSE;
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (pClientAux.IsValid())
	{
		std::string strReturn = pClientAux->NavigateGet(_T(ABK_REQUESTURL_CURRENTTIME), -1);
		if (!strReturn.empty())
		{
			CJsonParser parsResponse(strReturn.c_str());
			for (; !parsResponse.IsDone(); ++parsResponse)
			{
				bSuccess = parsResponse.ExtractValue(ABK_RSP_CURRENTTIME_TIME, pGet);
				if (bSuccess)
					break;
			}
			if (!bSuccess)
			{
				AddLog(LOGSEVERITY_ERROR, _T("Error: No date included in the answer of %s"), _T(ABK_REQUESTURL_CURRENTTIME));
			}
		}
	}
	return bSuccess;
}


/** Requests list of mailboxes

	@param pGet
		List receiving the available mailbox names
*/
bool CAbkClient::GetMailboxList(std::vector<CString> *pGet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	return pClientAux->GetVarOrMailboxList(_T(ABK_REQUESTURL_MAILBOXLIST), pGet);
}

/** Requests metadata of a variable

	@param vectVarNames
		List with mailbox names to retrieve metadata for
	@param pGet
		Pointer to list to store metadata in
	@return
		true on success, false on error
*/
bool CAbkClient::GetMailboxMeta(const std::vector<LPCTSTR>& vectMailboxNames, std::vector<CAbkClientMeta>* pGet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	return pClientAux->GetVarOrMailboxMeta(vectMailboxNames, pGet, true);
}

/** Requests metadata of a mailbox

	@param vectVarNames
		List with mailbox names to retrieve metadata for
	@param pGet
		Pointer to element to store metadata in
	@return
		true on success, false on error
*/
bool CAbkClient::GetMailboxMeta(LPCTSTR pszMailboxName, CAbkClientMeta *pGet)
{
	std::vector<LPCTSTR> vectVarNames;
	std::vector<CAbkClientMeta> vectMeta;
	vectVarNames.push_back(pszMailboxName); // compose a list with one entity
	bool bSuccess = GetMailboxMeta(vectVarNames, &vectMeta); // request the meta data
	assert(vectMeta.size() == 1);
	if (bSuccess)
		*pGet = *vectMeta.begin();
	return bSuccess;
}


/** Requests metadata of a variable

	@param vectVarNames
		List with variable names
	@param pGet
		Pointer to return the meta
	@return
		true on success, false on error
*/
bool CAbkClient::GetVarMeta(const std::vector<LPCTSTR> &vectVarNames, std::vector<CAbkClientMeta> *pGet)
{
	bool bSuccess = false;
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (pClientAux.IsValid())
		bSuccess = pClientAux->GetVarOrMailboxMeta(vectVarNames, pGet, false);
	return bSuccess;
}


/** Requests metadata of a variable

	@param pszVarName
		Name of variable to be queried
	@param pGet
		Pointer to return the metadata
	@return
		true on success, false on error
*/
bool CAbkClient::GetVarMeta(LPCTSTR pszVarName, CAbkClientMeta *pGet)
{
	std::vector<LPCTSTR> vectVarNames;
	std::vector<CAbkClientMeta> vectMeta;
	vectVarNames.push_back(pszVarName); // compose a list with one entity
	bool bSuccess = GetVarMeta(vectVarNames, &vectMeta); // request the meta data
	assert(vectMeta.size() == 1);
	if (bSuccess)
		*pGet = *vectMeta.begin();
	return bSuccess;
}


/** Sends a form

	@param pszFormName
		Name of form to be sent
	@param vectSend
		List of elements to be sent.
		Only the m_varValue with the corresponding names are set
*/
bool CAbkClient::SendForm(LPCTSTR pszFormName, const std::vector<CFormElement>& vectSend)
{
	ASSERT(pszFormName);
	// assert(m_pClientAux);

	CClientPtrRef pClientAux(m_pClientAux);
	if (!pClientAux.IsValid())
	{
		AddLog(LOGSEVERITY_ERROR, _T("Tried to send the filled form \"%s\" but connection to server was lost in the meanwhile."), pszFormName);
		return false;
	}
	bool bSuccess = true;
	CJsonFormatter jfForm;  // {
	std::vector<CFormElement>::const_iterator iterElement;
	for (iterElement = vectSend.begin(); iterElement != vectSend.end(); ++iterElement)
	{
		const CFormElement *pElement = &*iterElement;
		switch (pElement->m_varValue.vt)
		{
		case VT_I2:
			jfForm.WriteValue(CT2A(pElement->m_strName, CP_UTF8), (int)pElement->m_varValue.iVal); // "Elementname":123
			break;
		case VT_I4:
			jfForm.WriteValue(CT2A(pElement->m_strName, CP_UTF8), (int)pElement->m_varValue.lVal); // "Elementname":123
			break;
#ifdef ALL_VT
		case VT_INT:
			jfForm.WriteValue(CT2A(pElement->m_strName, CP_UTF8), (int)pElement->m_varValue.intVal); // "Elementname":123
			break;
#endif
		case VT_R8:
			jfForm.WriteValue(CT2A(pElement->m_strName, CP_UTF8), pElement->m_varValue.dblVal); // "Elementname":1.23
			break;
		case VT_BSTR:
    #ifdef OLE2ANSI
      jfForm.WriteValue(CT2A(pElement->m_strName, CP_UTF8), (char *)(CA2A(pElement->m_varValue.bstrVal, CP_UTF8))); // "Elementname":"string"
    #else
			jfForm.WriteValue(CT2A(pElement->m_strName, CP_UTF8), (char *)(CW2A(pElement->m_varValue.bstrVal, CP_UTF8))); // "Elementname":"string"
    #endif
			break;
		case VT_BOOL:
			jfForm.WriteValue(CT2A(pElement->m_strName, CP_UTF8), (bool)(pElement->m_varValue.boolVal != 0)); // "Elementname":true
			break;
		case VT_EMPTY:
			jfForm.WriteValue(CT2A(pElement->m_strName, CP_UTF8), std::numeric_limits<double>::quiet_NaN()); // null
			break;
		default:
			assert(false); // encountered an unimplemented variant type
			bSuccess = false;
		}
	}
	jfForm.Close(); // }
	if (!bSuccess)
		return false;

	// send form
	CString strUrl;
	strUrl.Format(_T("%s/%s"), _T(ABK_SERVICE_FORMS), pszFormName);
	if (!pClientAux->NavigatePut(strUrl, -1, jfForm.GetStream()->str()))
		return false;
	return true;
}


/** Sends an audio header

	@param nId
		General purpose id the server wants to be reflected when the server initiated the recording operation
	@param nSampleRateHz
		Sample rate in Hz
	@param nBitsPerSample
		Bits per sample, 8 and 16 allowed
	@param nChannels
		Number of Channels. Allowed is 1 (mono) and 2 (stereo)
	@return
		true on success, false on error
*/
bool CAbkClient::SendAudioRecHeader(int nId, int nSampleRateHz, int nBitsPerSample, int nChannels)
{
	bool bSuccess = false;
	assert(nBitsPerSample == 8 || nBitsPerSample == 16); // invalid bits per sample??
	assert(nChannels == 1 || nChannels == 2); // invalid number of channels??
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (pClientAux.IsValid() && IsConnected())
	{
		CJsonFormatter jfHeader; // header data formatted in json
		jfHeader.WriteValue(ABK_AUDIOREC_ID, nId);
		jfHeader.WriteValue(ABK_AUDIOREC_SAMPLERATE_HZ, nSampleRateHz);
		jfHeader.WriteValue(ABK_AUDIOREC_CHANNELS, nChannels);
    if (g_bForce8BitAudioAbk)
      jfHeader.WriteValue(ABK_AUDIOREC_BITSPERSAMPLE, 8);
    else
		  jfHeader.WriteValue(ABK_AUDIOREC_BITSPERSAMPLE, nBitsPerSample);
		jfHeader.Close();
		bSuccess = pClientAux->NavigatePut(_T(ABK_REQUESTURL_AUDIOREC_HEADER), -1, jfHeader.GetStream()->str());
#ifdef LOGGER_QUIRK
    if (pClientAux->GetStatus() == 403)
      bSuccess=true;
#endif
	}
	return bSuccess;
}


/** Sends audio data

	@param nId
		General purpose id the server wants to be reflected when the server initiated the recording operation
	@param pData
		Pointer to buffer containing audio samples

		if bits per sample == 8: BYTES @n
		if bits per sample == 16: WORDs in little endian format @n
		value sequence for stereo: left, right, left, right
	@param nBitsPerSample
		Bits per sample, 8 and 16 allowed
	@param nChannels
		Number of Channels. Allowed is 1 (mono) and 2 (stereo)
	@param nSamplesPerChannel
		Number of samples of each channel in pData
	@return
		true on success, false on error
*/
bool CAbkClient::SendAudioRecData(int nId, const void *pData, int nBitsPerSample, int nChannels, int nSamplesPerChannel)
{
	bool bSuccess = false;
	assert(nBitsPerSample == 8 || nBitsPerSample == 16); // invalid bits per sample??
	assert(nChannels == 1 || nChannels == 2); // invalid number of channels??
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (pClientAux.IsValid() && IsConnected())
	{
		CJsonFormatter jfData; // header data formatted in json
		jfData.WriteValue(ABK_AUDIOREC_ID, nId);

		CJsonStreamArray jaData(&jfData, ABK_AUDIOREC_DATA);
		if (nBitsPerSample == 8)
		{
			const BYTE *pData8 = (const BYTE *)pData;
			for (int nSample = 0; nSample < nSamplesPerChannel; ++nSample)
			{
				for (int nChannel = 0; nChannel < nChannels; ++nChannel)
				{
					jaData.WriteValue((int)(*pData8));
					++pData8;
				}
			}
		}
		else if (nBitsPerSample == 16)
		{
			const int16_t *pData16 = (const int16_t *)pData;
			for (int nSample = 0; nSample < nSamplesPerChannel; ++nSample)
			{
				for (int nChannel = 0; nChannel < nChannels; ++nChannel)
				{
          if (g_bForce8BitAudioAbk)
          {
            // Convert S16 to U8
            int nVal = *pData16 / 256 + 128;
            if (nVal < 0) nVal = 0;
            if (nVal > 255) nVal = 255;
            jaData.WriteValue(nVal);
          }
          else
          {
					  jaData.WriteValue((int)(*pData16));
          }
					++pData16;
				}
			}
		}
		else
		{
			assert(false);
		}
		jaData.Close();

		jfData.Close();
		bSuccess = pClientAux->NavigatePut(_T(ABK_REQUESTURL_AUDIOREC_DATA), -1, jfData.GetStream()->str());
#ifdef LOGGER_QUIRK
    if (pClientAux->GetStatus() == 403)
      bSuccess=true;
#endif
	}
	return bSuccess;
}


/** Sends audio footer

	@param nId
		General purpose id the server wants to be reflected when the server initiated the recording operation
	@return
		true on success, false on error
*/
bool CAbkClient::SendAudioRecFooter(int nId)
{
	bool bSuccess = false;
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (pClientAux.IsValid() && IsConnected())
	{
		CJsonFormatter jfFooter; // header data formatted in json
		jfFooter.WriteValue(ABK_AUDIOREC_ID, nId);
		jfFooter.Close();
		bSuccess = pClientAux->NavigatePut(_T(ABK_REQUESTURL_AUDIOREC_FOOTER), -1, jfFooter.GetStream()->str());
#ifdef LOGGER_QUIRK
    if (pClientAux->GetStatus() == 403)
      bSuccess=true;
#endif
	}
	return bSuccess;
}


/** Sends event that use rejected audio recording

	@param nId
		general purpose ID the sender wants to be reflected
*/
bool CAbkClient::SendAudioRecRejectEvent(int nId)
{
	return SendEvent(ABK_CLIENTEVENT_AUDIOREC_REJECT, _T(""), (double)nId, 0, false);
}

/** returns whether the server will be instructed to translate values into text representation
@return true, if the server is instructed to translate values into text representation
 false, if the server is instructed to send non-translated values and the client shall translate them by value-text tables, if applicable
*/
bool CAbkClient::TextTranslationByServer (void) const
{
  return m_bTextTranslationByServer;
}


/** Pauses long polling thread*/
bool CAbkClient::SuspendLongPolling(void)
{
	//if (!m_hLongPollThread)
	//	return false;
	m_evLongPollEnable.Reset(); // stall the long polling thread
	return true;
}


/** Resumes long polling thread */
bool CAbkClient::ResumeLongPolling(void)
{
	//if (!m_hLongPollThread)
	//	return false;
	m_evLongPollEnable.Set(); // no longer stall the long polling thread
	return true;
}


/** Long polling thread entrypoint

	@param vpThis
		pointer to CAbkClient
*/
/*static*/ DWORD WINAPI CAbkClient::LongPollThreadS(void *vpThis)
{
	assert(vpThis);
	return (reinterpret_cast<CAbkClient *>(vpThis))->LongPollThread();
}


/** Long polling thread */
int CAbkClient::LongPollThread(void)
{
	AddLog(LOGSEVERITY_TRACE, _T("LongPollThread() started"));
	// int nErrorCount=0; // incrementing on errors, decrementing on http success
  // ticks when the last successfull response was received
  // initialize to current tick count to avoid false failure at the beginning
	DWORD dwTickLastSuccessfulResponse = GetTickCount();
	while (!m_bTerminateLongPoll)
	{
		const char *pszResponse = NULL; // answer from server with events and data

		//WaitForSingleObject(m_evLongPollEnable.m_hObject, INFINITE); // if stalled, block here until the event gets set
		m_evLongPollEnable.Wait(INFINITE);

		CClientPtrRef pClientEvent(m_pClientEvent);
		assert(pClientEvent.IsValid()); // no connection with the aux http client established
		if (!pClientEvent.IsValid())
			break;
		//    DWORD dwTickBefore=GetTickCount(); // tick count before the request
		std::string strResponse = pClientEvent->NavigateGet(_T(ABK_REQUESTURL_SERVEREVENT), m_nSessionId); // request event and wait for answer (blocks here);
		
		int nHttpStatus = pClientEvent->GetStatus();

		if (nHttpStatus == 200)
			pszResponse = strResponse.c_str();

		DWORD dwTickAfter = GetTickCount();

		//char buf[1024];
		//pszResponse=buf;
		//memcpy(buf,"{\"DataLists\":{\"DaqListVar\":[74,72174,72174,72174,72174,72174,72174,72174,72174,72174]}}",1024);
		//Sleep(50);

		if (nHttpStatus == 200 && pszResponse)
		{
			dwTickLastSuccessfulResponse = dwTickAfter;
			CJsonParserAtl jpEvent(pszResponse);
			for (; !jpEvent.IsDone(); ++jpEvent)  // each event
			{
				if (jpEvent.TestObject(ABK_RSP_SERVEREVENT_DATALISTS)) // is there DataLists:{
				{
					//OnServerDataBegin();
					for (++jpEvent; !jpEvent.IsDone(); ++jpEvent)  // each data list
					{
						std::string strDaqName;
						if (jpEvent.TestArray(&strDaqName)) // if array
						{
							CAbkSingleLock lockDaq(&m_mutexDaq, true, DAQ_TIMEOUT); // lock the daq map
							assert(m_mutexDaq.IsLocked());
							CAbkClientDaq *pDaq = FindDaq(strDaqName); // get the client-side daq list
							if (pDaq)
							{
								if (!pDaq->OnDataFromServer(jpEvent)) // here, data gets dispatched to the destination
									CAbkClientDaq::DiscardJson(jpEvent);
							}
						} // end of data array
						jpEvent.SkipItem(); // skip unexpected items
					} // for each data list

					//OnServerDataEnd();
				}
				else if (jpEvent.TestArray(ABK_RSP_SERVEREVENT_EVENTS)) // is there Events:[
				{
					assert(m_pNextEventData); // there must be a location to store the event params
					for (++jpEvent; !jpEvent.IsDone(); ++jpEvent)  // each event
					{
						BOOL bSuccessDecode = m_pNextEventData->SetEvent(jpEvent); // decode event into m_pNextEventData
						jpEvent.SkipItem(); // skip any unknown items
						if (bSuccessDecode)
						{
							m_pNextEventData = OnServerEvent(m_pNextEventData); // call the event handler and get the location for the next event
						}
						else // error in syntax or completelyness of the event data
						{
							AddLog(LOGSEVERITY_ERROR, _T("The event data was incomplete or had incorrect syntax")/*,m_pNextEventData->m_data.m_strType*/);
							break;
						}
						jpEvent.SkipItem(); // skip unexpected items            
					} // each event
				}
				jpEvent.SkipItem(); // skip unexpected items
			} // for each event
		}
		else if (pszResponse == NULL)
		{
			if (dwTickAfter >= dwTickLastSuccessfulResponse + LONGPOLL_FAILURE_TOLERANCE_MS) // tolerated error time span exceeded
			{
				DWORD dwErrorDuration = 0;
				if (dwTickLastSuccessfulResponse)
					dwErrorDuration = dwTickAfter - dwTickLastSuccessfulResponse;
				m_bTerminateLongPoll = true; // terminate and signal that thread terminated itself (an error occured)
				AddLog(LOGSEVERITY_ERROR, _T("Successive http errors for %d ms."), (int)dwErrorDuration/*LONGPOLL_FAILURE_TOLERANCE_MS*/);
			}
			else
			{
				Sleep(LONGPOLL_FAILURE_RECOVERY_MS);
			}
		}
		else // server responded but with error
		{
			DWORD dwWaitBeforeResume = OnLongPollErrorResponse(nHttpStatus, m_nSessionId); // call custom handler
			while (!m_bTerminateLongPoll && dwWaitBeforeResume != 0)
			{
				Sleep(10);
				if (dwWaitBeforeResume > 10)
					dwWaitBeforeResume -= 10;
				else
					dwWaitBeforeResume = 0;
			}
		}
	}
	AddLog(LOGSEVERITY_TRACE, _T("LongPollThread() terminating"));
	//m_longPollThread.detach();
	m_evLongPollDone.Set();
	return 0;
}


bool CAbkClient::AddDaq(CAbkClientDaq *pAdd)
{
	bool bSuccess = false;

	if (m_nSessionId >= 0)
	{
		CAbkSingleLock lockDaq(&m_mutexDaq, true, DAQ_TIMEOUT);
		assert(m_mutexDaq.IsLocked());
		std::string strDaqNameA(CT2A(pAdd->m_strName, CP_UTF8));
		if (!FindDaq(strDaqNameA))
		{
			std::pair<std::map<std::string, CAbkClientDaq *>::iterator, bool> iterInsert; // result of the insert operation
			iterInsert = m_mapDaq.insert(std::pair<std::string, CAbkClientDaq *>(strDaqNameA, pAdd));
			assert(iterInsert.second); // error inserting the daq?
			pAdd->m_pOwner = this;
			bSuccess = true;
		}
		else
			AddLog(LOGSEVERITY_ERROR, _T("Tried to add a DAQ while another DAQ with same name exists: \"%s\""), (LPCTSTR)pAdd->m_strName);
	}
	else
		AddLog(LOGSEVERITY_ERROR, _T("Tried to add a DAQ without having a valid session ID"));
	return bSuccess;
}

//--------------------------------------------------------------------------
// FindDaq()               searches for a DAQ
// ---------
// Input: strDaqName = name of daq to search for
// Return: pointer to DAQ list, NULL if not found

CAbkClientDaq *CAbkClient::FindDaq(const std::string &strDaqName)
{
	CAbkSingleLock lockDaq(&m_mutexDaq, true, DAQ_TIMEOUT);
	assert(m_mutexDaq.IsLocked());
	std::map<std::string, CAbkClientDaq *>::iterator iterDaq;
	iterDaq = m_mapDaq.find(strDaqName);
	if (iterDaq == m_mapDaq.end())
		return NULL; // not found
	return (*iterDaq).second;
}

BOOL CAbkClient::PopLog(LOGSEVERITY & nSeverityGet, CString & strMessageGet)
{
	// TODO:
	return TRUE;
}

//--------------------------------------------------------------------------
// SendButtonEvent()           sends a button press/release event to the server
// -----------------
// Input: strButtonName = name of the button
//        bPressedState = TRUE if button is pressed, FALSE if released
//        nTime = time value of key event.
//                positive values indicate the time since key was pressed in ms
//                negative values indicate the time since key was released in ms
//        bPrivate: if true, client intends to process the reflected event
//                  by itself and the server shall not reflect the event
//                  to other clients
// Return: true on success, false on error

bool CAbkClient::SendButtonEvent(LPCTSTR pszButtonName, bool bPressedState, int nTime, bool bPrivate)
{
	return SendEvent(ABK_CLIENTEVENT_BUTTON, pszButtonName, (double)(bPressedState != 0), (double)nTime, bPrivate);
}

std::string CAbkClient::GetClientState(LPCTSTR pszFileExtension)
{
	CClientPtrRef pClientAux(m_pClientAux);
	//assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return NULL;
	CString strUrl;
	assert(pszFileExtension);
	assert(pszFileExtension[0] != '\0'); // please no empty extension
	assert(pszFileExtension[0] == '.'); // extension must start with delimiter dot
	strUrl.Format(_T("%s/%s_%s_%s%s"), _T(ABK_SERVICE_CLIENTSTATES), (LPCTSTR)CA2T(m_strClientClass.c_str()), (LPCTSTR)CA2T(m_strClientType.c_str()), (LPCTSTR)CA2T(m_strClientSerial.c_str()), pszFileExtension);
	std::string strResponse = pClientAux->NavigateGet(strUrl, -1); // read data
	int nStatus = pClientAux->GetStatus();
	if (nStatus != 200) // if not responded with OK (200)..
		strResponse.clear(); // .. devalidate the result
	return strResponse;
}

bool CAbkClient::SetClientState(const char * pConfigString, LPCTSTR pszFileExtension)
{
	bool bSuccess = false;
	CClientPtrRef pClientAux(m_pClientAux);
	if (pClientAux.IsValid() && IsConnected())
	{
		CString strUrl;
		assert(pszFileExtension);
		assert(pszFileExtension[0] != '\0'); // please no empty extension
		assert(pszFileExtension[0] == '.'); // extension must start with delimiter dot
		strUrl.Format(_T("%s/%s_%s_%s%s"), _T(ABK_SERVICE_CLIENTSTATES), (LPCTSTR)CA2T(m_strClientClass.c_str()), (LPCTSTR)CA2T(m_strClientType.c_str()), (LPCTSTR)CA2T(m_strClientSerial.c_str()), pszFileExtension);
		bool bSuccess = pClientAux->NavigatePut(strUrl, -1, std::string(pConfigString) /*, _T(MIME_TYPE_TEXT)*/);
		if (bSuccess)
		{
			int nStatus = pClientAux->GetStatus();
			if ((nStatus < 200) || (nStatus >= 300)) // if not responded with an OK-code (2xx)..
				bSuccess = false; // .. error in writing at the server
		}
	}
	return bSuccess;
}

bool CAbkClient::GetVarList(std::vector<CString> *pGet)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	return pClientAux->GetVarOrMailboxList(_T(ABK_REQUESTURL_VARLIST), pGet);
}

bool CAbkClient::GetClientFirmwareInfo(std::vector<CFirmwareInfo>& vectGet, LPCTSTR pszClientType)
{
	CClientPtrRef pClientAux(m_pClientAux);
	if (!pClientAux.IsValid())
		return false;
	bool bSuccess = true;
	vectGet.clear();

	CA2T strType(m_strClientType.c_str());

	// compose and send request
	if (!pszClientType) // if no client type name specified, use the stored one
		pszClientType = strType;
	CJsonFormatter jfReq;
	jfReq.WriteValue(ABK_REQ_FIRMWARE_CLASS, m_strClientClass);
	jfReq.WriteValue(ABK_REQ_FIRMWARE_TYPE, CT2A(pszClientType, CP_UTF8));
	jfReq.WriteValue("Serial", m_strClientSerial); // send serial unsolicitedly
	std::string strResponse = pClientAux->NavigatePost(_T(ABK_REQUESTURL_FIRMWARE), -1, jfReq.GetStream()->str()); // send own info and get list of firmware files file info
	if (strResponse.empty())
		return false;

	// decode response
	CJsonParserAtl jpResp(strResponse.c_str());
	bool bAnyVersionOmitted = false; // if we found at least one entity without version info
	for (; !jpResp.IsDone(); ++jpResp)
	{
		if (jpResp.TestArray(ABK_RSP_FIRMWARE_IMAGELIST)) // is there "Images":[
		{
			CFirmwareInfo fwi;
			bool bUrlDecoded = false;
			bool bMd5Decoded = false;
			bool bVersionDecoded = false;
			for (; !jpResp.IsDone(); ++jpResp)
			{
				bUrlDecoded |= jpResp.ExtractValueAtl(ABK_RSP_FIRMWARE_URL, fwi.m_strUrl); // get URL
				bMd5Decoded |= jpResp.ExtractValueAtl(ABK_RSP_FIRMWARE_MD5, fwi.m_strMd5); // get MD5 hash
				bVersionDecoded |= jpResp.ExtractValueAtl(ABK_RSP_FIRMWARE_VERSION, fwi.m_strVersion); // get version info
			}
			if (!bVersionDecoded)
				bAnyVersionOmitted = true;
			if (bUrlDecoded && bMd5Decoded) // at least the server has to fill in these fields
			{
				vectGet.push_back(fwi);
			}
			else
			{
				if (!bUrlDecoded)
					AddLog(LOGSEVERITY_ERROR, _T("GetClientFwInfo: Firmware info response: URL field is missing"));
				if (!bMd5Decoded)
					AddLog(LOGSEVERITY_ERROR, _T("GetClientFwInfo: Firmware info response: MD5 field is missing"));
				bSuccess = FALSE;
			}
			jpResp.SkipItem();
		}
	}
	if (bAnyVersionOmitted && vectGet.size() > 1) // if more than one entity returned and a version field was omitted
	{
		AddLog(LOGSEVERITY_ERROR, _T("GetClientFwInfo: Firmware info response: Version field is missing while returning multiple entities"));
		bSuccess = FALSE;
	}
	if (!bSuccess)
		vectGet.clear(); // discard decoded content if an error occured
	return bSuccess;
}

void CAbkClient::AddLogHttp(LOGSEVERITY nSeverity, int nHttpStatusCode, LPCTSTR pszUrl, LPCTSTR pszMethod, const char *pcszResponse, const char *pcszoPostPutData/*=NULL*/)
{
	//ASSERT(nHttpStatusCode>=0);
	if (pcszoPostPutData)
		AddLog(LOGSEVERITY_ERROR, _T("HTTP status code %d. Url: \"%s\", Method: %s, Request: \"%s\", Response: \"%s\""), nHttpStatusCode, pszUrl, pszMethod, (LPCTSTR)CA2T(pcszoPostPutData, CP_UTF8), (LPCTSTR)CA2T(pcszResponse, CP_UTF8));
	else
		AddLog(LOGSEVERITY_ERROR, _T("HTTP status code %d. Url: \"%s\", Method: %s, Response: \"%s\""), nHttpStatusCode, pszUrl, pszMethod, (LPCTSTR)CA2T(pcszResponse, CP_UTF8));
	if (nHttpStatusCode < 0)
	{
		// 23.10.18: Desktop-PC, Fehler in c:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\atlmfc\include\atlspriv.inl Zeile 218, inline bool ZEvtSyncSocket::Read()=> WSARecv()-Fehler. WSAGetLastError(): 10053
		DWORD dwError = GetLastError();
		AddLog(LOGSEVERITY_ERROR, _T("HTTP status code %d => Error-Code %d"), nHttpStatusCode, (int)dwError);
	}
}

/** Cleans Object
	@param bLostConnection
		true, if connection is no longer available and remote objects
			shall not be tidied up
		false, if connection is apparently avilable
			and remote objects shall be tidied up via http
*/
void CAbkClient::TidyUp(bool bLostConnection)
{
	bool bLongPollSelfTerminated = m_bTerminateLongPoll; // if true, indicates that the longpoll thread terminated itself due to an error

	// stop the long-polling thread
	if (1)
	{
		CClientPtrRef pClientAux(m_pClientAux);
		CClientPtrRef pClientEvent(m_pClientEvent);
		if (pClientEvent.IsValid())
		{
			m_bTerminateLongPoll = true;
			m_evLongPollEnable.Set(); // in case the long-poll-thread is stalled, wake it up so it can terminate
			//WaitForSingleObject(m_evLongPollDone.m_hObject, LONGPOLL_FAILURE_TOLERANCE_MS * 2); // wait until terminated
			m_evLongPollDone.Wait(LONGPOLL_FAILURE_TOLERANCE_MS * 2);
			pClientEvent->Close(); // close connection so request of long-polling gets interrrupted
			m_bTerminateLongPoll = false;
		}

		if (bLostConnection)
		{
			if (pClientAux.IsValid())
				pClientAux->SetServerAddr("", 0); // inhibit further requests since connection is dead
			if (pClientEvent.IsValid())
				pClientEvent->SetServerAddr("", 0);
		}

		// tidy-up aux items
		if (pClientAux.IsValid())
		{
			// delete the daqs
			std::map<std::string, CAbkClientDaq *>::iterator iterDaq;
			for (iterDaq = m_mapDaq.begin(); iterDaq != m_mapDaq.end(); ++iterDaq)
			{
				if (bLongPollSelfTerminated) // if it is likely that the server is inresponsive..
					iterDaq->second->m_pOwner = NULL; // prevent the daq from deleting at the server
				delete iterDaq->second;
			}
			m_mapDaq.clear();

			// delete session
			if (IsConnected() && !bLongPollSelfTerminated)
				pClientAux->DeleteSession(m_nSessionId);

			pClientAux->Close();
		}

	}

	// finally delete the clients
	m_pClientEvent.Delete();
	m_pClientAux.Delete();

	m_strServerAddress = "";
	m_nPort = 0;
	m_strPort.clear();
	m_nSessionId = -1;
}

void CAbkClient::SetServerAddr(LPCTSTR pszServerAddress, int nPort)
{
	if ((m_nPort != nPort) || (boost::equals(pszServerAddress, m_strServerAddress))) // if changes in address or port
	{
		// TODO:
		Create(pszServerAddress, nPort, m_pNextEventData, CA2T(m_strClientClass.c_str()), CA2T(m_strClientType.c_str()), CA2T(m_strClientSerial.c_str()), CA2T(m_strClientFwRev.c_str()), CA2T(m_strClientHwRev.c_str()));
	}
}

int CAbkClient::GetServerPort(void) const
{
	return m_nPort;
}

int CAbkClient::GetSessionId()
{
	return m_nSessionId;
}

//--------------------------------------------------------------------------
// GetClientConfigInfo()      queries the client configuration/app information
// ---------------------
// Input: strUrl = [out] URL where to download the configuration file. if no file
//                 is provided, this string will get empty
//        strMd5 = [out] MD5 of the file, only valid if strUrl is not empty
//        pszClientType=NULL = [in, optional] client type name when querying
//                             the info. If NULL, the standard client type which
//                             was specified in Create() will be used
// Return: true on success, even if no config file is available and the strUrl was
//              emptied
//         false on error or if server does not support client config hosting

bool CAbkClient::GetClientConfigInfo(CString &strUrl, CString &strMd5, LPCTSTR pszClientType/*=NULL*/)
{
	CClientPtrRef pClientAux(m_pClientAux);
	//assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;

	// compose and send request
	if (!pszClientType) // if no client type name specified, use the stored one
		pszClientType = (LPCTSTR)m_strClientType.c_str();
	CJsonFormatter jfReq;
	jfReq.WriteValue(ABK_REQ_CLIENTCONFIG_CLASS, m_strClientClass);
	jfReq.WriteValue(ABK_REQ_CLIENTCONFIG_TYPE, CT2A(pszClientType));
	jfReq.WriteValue(ABK_REQ_CLIENTCONFIG_SERIAL, m_strClientSerial);
	std::string strResponse = pClientAux->NavigatePost(_T(ABK_REQUESTURL_CLIENTCONFIG_INFO), -1, jfReq.GetStream()->str()); // send own info and get config file info
	if (strResponse.empty())
		return false;

	// decode response
	CJsonParserAtl jpResp(strResponse.c_str());
	bool bUrlDecoded = false;
	bool bMd5Decoded = false;
	for (; !jpResp.IsDone(); ++jpResp)
	{
		bUrlDecoded |= jpResp.ExtractValueAtl(ABK_RSP_CLIENTCONFIG_URL, strUrl); // get URL
		bMd5Decoded |= jpResp.ExtractValueAtl(ABK_RSP_CLIENTCONFIG_MD5, strMd5); // get MD5 hash
	}
	if (!bUrlDecoded)
	{
		AddLog(LOGSEVERITY_ERROR, _T("ClientConfig info response: URL field is missing"));
		return false;
	}
	if (!strUrl.IsEmpty() && (!bMd5Decoded || strMd5.IsEmpty())) // if there was an URL returned, a valid MD5 must be there too
	{
		AddLog(LOGSEVERITY_ERROR, _T("ClientConfig info response: MD5 field is missing"));
		return false;
	}
	return true;
}

bool CAbkClient::GetForm(LPCTSTR pszFormName, std::vector<CFormElement>& vectGet, CString & strCaptionGet, int & nPersitenceMs)
{
	CClientPtrRef pClientAux(m_pClientAux);
	assert(pClientAux.IsValid());
	if (!pClientAux.IsValid())
		return false;
	CString strUrl;
	strUrl.Format(_T("%s/%s"), _T(ABK_SERVICE_FORMS), pszFormName);
	std::string strResponse = pClientAux->NavigateGet(strUrl, -1);
	if (strResponse.empty())
		return false;
	CJsonParserAtl jpForm(strResponse.c_str());
	bool bCaptionDecoded = false;
	nPersitenceMs = 0; // default: infinite display time
	for (; !jpForm.IsDone(); ++jpForm)
	{
		bCaptionDecoded |= jpForm.ExtractValueAtl(ABK_RSP_FORMS_CAPTION, strCaptionGet); // get caption of the form
		jpForm.ExtractValue(ABK_RSP_FORMS_PERSISTENCE, &nPersitenceMs); // get persistence time
		if (jpForm.TestArray(ABK_RSP_FORMS_CONTROLS)) // is there "Controls":[
		{
			for (++jpForm; !jpForm.IsDone(); ++jpForm) // each element
			{
				CFormElement elGet;
				if (!elGet.DecodeJson(jpForm)) // decode the element
					return false; // error in element
				vectGet.push_back(elGet);
			}
		}
		jpForm.SkipItem();
	}
	if (!bCaptionDecoded)
	{
		AddLog(LOGSEVERITY_ERROR, _T("Caption is missing in form \"%s\""), pszFormName);
		return false;
	}
	return true;
}

/** Overload to consume messages on queue */
/*virtual*/ void CAbkClient::OnLogAdded(void)
{

}

DWORD Abk::CAbkClient::OnLongPollErrorResponse(int nHttpStatusCode, int nSessionId)
{
	// in derived classes, handle the error. No need to call the base class implementation.
	DWORD dwWaitBeforeResume = 0;
	if (nHttpStatusCode >= 400 && nHttpStatusCode <= 499)
		dwWaitBeforeResume = INFINITE; // default: no further requests
	return dwWaitBeforeResume;
}

//---------------------------------------------------------------------------------------

CAbkClient::CFormElement::CFormElement()
{
	m_nType = TYPE_INVALID;
	m_nMaxLen = 0;
}


CAbkClient::CFormElement::~CFormElement()
{
	VariantClear(&m_varValue);  // clear byte array e.g. when m_varValue holds a string
}


/** Decodes a JSON and sets member variables accordingly
	@param jpElement
		JSON parser containing a form element
	@return
		TRUE on success, FALSE on decode error
*/
bool CAbkClient::CFormElement::DecodeJson(CJsonParserAtl &jpElement)
{
	bool bNameSent = false;
	bool bCaptionSent = false;
	bool bTypeSent = false;
	bool bValueSent = false;
	CString strType;
	m_vectOptions.clear(); // empty the option list
	m_nMaxLen = 0;
	m_nFlags = 0;
	m_nType = TYPE_INVALID;

	for (++jpElement; !jpElement.IsDone(); ++jpElement) // each attribute
	{
		bNameSent |= jpElement.ExtractValueAtl(ABK_RSP_FORMS_CONTROL_NAME, m_strName);
		bCaptionSent |= jpElement.ExtractValueAtl(ABK_RSP_FORMS_CONTROL_CAPTION, m_strCaption);
		bTypeSent |= jpElement.ExtractValueAtl(ABK_RSP_FORMS_CONTROLTYPE, strType);
		bValueSent |= jpElement.ExtractValueAtl(ABK_RSP_FORMS_CONTROL_INITIALVALUE, m_varValue);
		jpElement.ExtractValue(ABK_RSP_FORMS_CONTROL_MAXLEN, &m_nMaxLen); // ocassionally decode the maximum len
		if (jpElement.TestArray(ABK_RSP_FORMS_CONTROL_OPTIONS)) // is there "Options":[
		{
			CString strOption;
			for (++jpElement; !jpElement.IsDone(); ++jpElement) // each option
			{
				jpElement.ExtractValueAtl(strOption);
				m_vectOptions.push_back(strOption);
			}
		}

		// test for additional attributes
		bool bAttribute;
		if (jpElement.ExtractValue(ABK_RSP_FORMS_CONTROL_READONLY, &bAttribute) && bAttribute)
			m_nFlags |= READONLY;
		if (jpElement.ExtractValue(ABK_RSP_FORMS_CONTROL_PASSWORD, &bAttribute) && bAttribute)
			m_nFlags |= PASSWORD;
		if (jpElement.ExtractValue(ABK_RSP_FORMS_CONTROL_NUMERIC, &bAttribute) && bAttribute)
			m_nFlags |= NUMERIC;
		if (jpElement.ExtractValue(ABK_RSP_FORMS_CONTROL_SUBMIT, &bAttribute) && bAttribute)
			m_nFlags |= SUBMIT;
		if (jpElement.ExtractValue(ABK_RSP_FORMS_CONTROL_CANCEL, &bAttribute) && bAttribute)
			m_nFlags |= CANCEL;
		if (jpElement.ExtractValue(ABK_RSP_FORMS_CONTROL_UPDATEABLE, &bAttribute) && bAttribute)
			m_nFlags |= UPDATEABLE;
	}

	// decode type
	if (!strType.Compare(_T(ABK_RSP_FORMS_CONTROLTYPE_INPUT)))
		m_nType = TYPE_EDIT;
	else if (!strType.Compare(_T(ABK_RSP_FORMS_CONTROLTYPE_CHECKBOX)))
		m_nType = TYPE_CHECKBOX;
	else if (!strType.Compare(_T(ABK_RSP_FORMS_CONTROLTYPE_COMBO)))
		m_nType = TYPE_COMBO;
	else if (!strType.Compare(_T(ABK_RSP_FORMS_CONTROLTYPE_BUTTON)))
		m_nType = TYPE_BUTTON;
	else
		return false;

	// test if all mandatory fields were present
	if (!bNameSent || !bCaptionSent || !bTypeSent)
		return false;
	if ((m_nType != TYPE_BUTTON) && (!bValueSent)) // all excapt button requires a initial value field
		return false;

	return true;
}


BOOL CAbkClient::CClientPtr::Delete(void)
{
	if (!m_pClient)
		return TRUE; // successfully deleted nothing
	BOOL bSuccess = FALSE;
	for (int nRetry = 0; nRetry < 300; nRetry++)
	{
		if (m_nUsage.load(boost::memory_order_acquire) == 0)
		{
			delete m_pClient;
			m_pClient = NULL;
			bSuccess = TRUE;
			break;
		}
		Sleep(10);
	}
  assert(m_nUsage==0);
	return bSuccess;
}

}