#include "stdafx.h"
#include <boost/test/unit_test.hpp>
#include "AbkClient.h"
#include "AbkServerEvent.h"

BOOST_AUTO_TEST_SUITE(Client)

/*
BOOST_AUTO_TEST_CASE(Connection)
{
	CAbkClient testClient;
	CAbkServerEvent seAbk;
	testClient.Create(_T("localhost"), 8080, &seAbk, _T("Display"), _T("EMBU-Sys_EMBU-Boost"), _T("01-23-45-67-89-ab"));
	int sessionId = testClient.ObtainSessionId(_T("Display"), _T("EMBU-Sys_EMBU-Boost"), _T("12-34-45-67-89-0a"));
	BOOST_TEST(sessionId >= 0);

	bool bSuccess;
	bSuccess = testClient.SetVarValue(_T("Reifendruck"), 10.0);
	BOOST_TEST(!bSuccess);
	bSuccess = testClient.SetVarValue(_T("Vari3"), 10.0);
	BOOST_TEST(bSuccess);

	std::list<CString> varlist;
	testClient.GetVarList(&varlist);

	BOOST_TEST(!varlist.empty());
}
*/

class CMyClient : public Abk::CAbkClient
{
protected:
	Abk::CAbkServerEvent m_seRx;
public:
	bool Create(LPCTSTR pszServerAddress, int nPort)
	{
		return Abk::CAbkClient::Create(pszServerAddress, 8080, &m_seRx, _T("Display"), _T("Demo"), NULL);
	}

	/** called when server sent an event */
	/*virtual*/ Abk::CAbkServerEvent *OnServerEvent(Abk::CAbkServerEvent *pEventData) override
	{
		if (1) // this block can be handled in a deferred manner
		{
			Abk::CAbkServerEvent::CData dataEvent;
			pEventData->GetEvent(dataEvent); // get and release the event object
		}
		return pEventData; // use the same
	}

	/** notifies that the log queue has got new entities. may be called in any thread context! */
	/*virtual*/ void OnLogAdded(void) override
	{
		return;
	}
};

BOOST_AUTO_TEST_CASE(Connection)
{
	CMyClient testClient;
	Abk::CAbkServerEvent seAbk;
	testClient.Create(_T("127.0.0.1"), 8080);
	//int sessionId = testClient.ObtainSessionId(_T("Display"), _T("EMBU-Sys_EMBU-Boost"), _T("12-34-45-67-89-0a"));
	int sessionId = testClient.GetSessionId();
	BOOST_TEST(sessionId >= 0);

	bool bSuccess;
	bSuccess = testClient.SetVarValue(_T("Reifendruck"), 10.0);
	BOOST_TEST(!bSuccess);
	bSuccess = testClient.SetVarValue(_T("Vari3"), 10.0);
	BOOST_TEST(bSuccess);

	std::list<CString> varlist;
	testClient.GetVarList(&varlist);

	BOOST_TEST(!varlist.empty());
}

BOOST_AUTO_TEST_SUITE_END()
