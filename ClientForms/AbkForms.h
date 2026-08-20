// SPDX-License-Identifier: MIT

// AbkForms.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

//#include "resource.h"		// moved to stdafx.h


// CAbkFormsApp:
// See AbkForms.cpp for the implementation of this class
//

class CAbkFormsApp : public CWinApp
{
public:
	CAbkFormsApp();

// Overrides
public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CAbkFormsApp theApp;