
// Generate_bin.cpp : Define the class behavior of the application
//

#include "stdafx.h"
#include "Generator.h"
#include "GeneratorDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CGenerate_binApp

BEGIN_MESSAGE_MAP(CGeneratorApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CGenerate_binApp structure

CGeneratorApp::CGeneratorApp()
{
	// Support for restarting the manager
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

	// TODO: Add the construction code here，
	// Place all important initializations in InitInstance
}


// The only one CGenerate_binApp object

CGeneratorApp theApp;


// CGenerate_binApp initialization

BOOL CGeneratorApp::InitInstance()
{
	// If a list of applications running on Windows XP specifies
	// Use ComCtl32.dll version 6 or higher to enable visualization
	// Then you need InitCommonControlsEx(). Otherwise, you will not be able to create a window.
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// Set it to include all the ones you want to use in your application
	// Public control class.
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();


	AfxEnableControlContainer();

	// Create a shell manager in case the dialog contains
	// Any shell tree view control or shell list view control
	CShellManager *pShellManager = new CShellManager;

	// Standard initialization
	// If you are not using these features and want to reduce
	// The size of the final executable should be removed
	// Specific initialization routines that are not needed
	// Change the registry key used to store settings
	// TODO: The string should be modified appropriately
	// For example, change to company or organization name
	SetRegistryKey(_T("应用程序向导生成的本地应用程序")); //Local application generated by the application wizard

	CGeneratorDlg dlg;
	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: When to put the processing here
		// "OK" to close the code of the dialog
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: When to put the processing here
		// "Cancel" to close the code of the dialog
	}

	// Delete the shell manager created above
	if (pShellManager != NULL)
	{
		delete pShellManager;
	}

	// Since the dialog is closed, it will return FALSE to exit the application
	// Instead of launching the application's message pump
	return FALSE;
}

