// InstallDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "Trochilus.h"
#include "InstallDlg.h"
#include "afxdialogex.h"


// CInstallDlg 对话框

IMPLEMENT_DYNAMIC(CInstallDlg, CDialogEx)

CInstallDlg::CInstallDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CInstallDlg::IDD, pParent)
{

}

CInstallDlg::~CInstallDlg()
{
}

void CInstallDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PROGRESS_INSTALL, m_insProgress);
}


BEGIN_MESSAGE_MAP(CInstallDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BUTTON_INSTALL, &CInstallDlg::OnBnClickedButtonInstall)
END_MESSAGE_MAP()


// CInstallDlg 消息处理程序


BOOL CInstallDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// TODO:  在此添加额外的初始化
	m_insProgress.ShowWindow(SW_HIDE);

	m_CheckRepeat.Init(CheckModule,this,_T("checkmodule"),0,1);

	return TRUE;
}
DWORD WINAPI CInstallDlg::CheckModule(LPVOID lpParameter)
{
	CInstallDlg* lpDlg = (CInstallDlg*)lpParameter;
	return lpDlg->CheckModuleProc();
}

DWORD WINAPI CInstallDlg::CheckModuleProc()
{
	MODULE_INST_STATUS status;
	UINT progress;

	QueryModuleInstallStatus(m_clientid,m_modname,&status,&progress);

	if (status == MODULESTATUS_INSTALLING)
	{
		m_insProgress.SetPos(progress);
	}
	else if(status == MODULESTATUS_INSTALLED)
	{
		ClientControlPanelManager::GetInstanceRef().HandleModuleMsg(m_clientid,MODULE_MSG_INSTALLED,m_modname.GetBuffer(),NULL);
		m_CheckRepeat.Stop();
	}
	return 0;
}

void CInstallDlg::OnBnClickedButtonInstall()
{
	if (InstallClientModule(m_clientid,m_modname))
	{
		m_insProgress.ShowWindow(SW_SHOW);
	
		m_CheckRepeat.Start();
	}
	else
	{
		AfxMessageBox(_T("Request install module faild!"));
	}
}

void CInstallDlg::Init(LPCTSTR clientid,LPCTSTR ModuleName)
{
	m_clientid = clientid;
	m_modname = ModuleName;
}