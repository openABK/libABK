#include "stdafx.h"
#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include "AbkClient.h"
#include "AbkServerEvent.h"
#include "ValuesFromSpec.h"

BOOST_AUTO_TEST_SUITE(Client)

class CMyClient : public Abk::CAbkClient
{
protected:
	Abk::CAbkServerEvent m_seRx;
public:
	bool Create(LPCTSTR pszServerAddress, int nPort)
	{
		return Abk::CAbkClient::Create(pszServerAddress, 8080, &m_seRx, _T("Display"), _T("AbkBoostClientTest"), _T("12-34-45-67-89-0a"));
	}

	/** Called when server sent an event */
	/*virtual*/ Abk::CAbkServerEvent *OnServerEvent(Abk::CAbkServerEvent *pEventData) override
	{
		if (1) // this block can be handled in a deferred manner
		{
			Abk::CAbkServerEvent::CData dataEvent;
			pEventData->GetEvent(dataEvent); // get and release the event object
		}
		return pEventData; // use the same
	}

	/** Notifies that the log queue has received new entities.
		@note May be called in any thread context! */
	/*virtual*/ void OnLogAdded(void) override
	{
		return;
	}

	/** Gets called when a server responds with an error status code.
		@note Gets called from the long-poll thread context! */
	virtual DWORD OnLongPollErrorResponse (int nHttpStatusCode, int nSessionId) override
	{
		DWORD dwWaitBeforeResume = 100;
		if (nHttpStatusCode == 400)
		{
			// Would queue reconnect

			dwWaitBeforeResume = INFINITE;
		}
		return dwWaitBeforeResume;
	}
};

BOOST_AUTO_TEST_CASE(Connection)
{
	CMyClient testClient;
	Abk::CAbkServerEvent seAbk;
	testClient.Create(_T("127.0.0.1"), 8080);
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

	// Receive one event using the socket for long-polling
	// To ensure a connection has been established.
	bSuccess = testClient.ReceiveEvent();
	BOOST_TEST(bSuccess);

	bSuccess = testClient.IsConnected();
	BOOST_TEST(bSuccess);

	bSuccess = testClient.DownloadFile(_T(ABK_REQUESTURL_VARLIST), _T("varlist.txt"));
	BOOST_TEST(bSuccess);
	bSuccess = boost::filesystem::exists("varlist.txt");
	BOOST_TEST(bSuccess);
}

#ifndef BOOST_ABK
static Abk::CAbkClient::CClientPtr m_pClientAux; // auxiliary client for blocking non-long-polling actions

static void CreateAndDestroyPtrs()
{
	for(size_t i = 0; i < 10000; i++)
	{
		Abk::CAbkClient::CClientPtrRef pClientAux(m_pClientAux);
	}
}

BOOST_AUTO_TEST_CASE(ClientPtr)
{
	Abk::CAbkClient client;

	m_pClientAux = new Abk::CAbkClient::CBaseAbstraction(&client);
	//Abk::CAbkClient::CClientPtrRef pClientAux(m_pClientAux);

	boost::thread thread1(CreateAndDestroyPtrs);
	boost::thread thread2(CreateAndDestroyPtrs);

	thread1.join();
	thread2.join();

	m_pClientAux.Delete();
}
#endif

BOOST_AUTO_TEST_SUITE_END()
