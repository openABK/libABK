#include "stdafx.h"
#include <boost/test/unit_test.hpp>
#include "AbkClientBoost.h"

BOOST_AUTO_TEST_SUITE(Client)

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

BOOST_AUTO_TEST_SUITE_END()
