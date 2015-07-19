// PanelDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "Trochilus.h"
#include "FilePanelDlg.h"
#include "afxdialogex.h"

// CPanelDlg 对话框

IMPLEMENT_DYNAMIC(CFilePanelDlg, CDialogEx)

CFilePanelDlg::CFilePanelDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CFilePanelDlg::IDD, pParent)
{

}

CFilePanelDlg::~CFilePanelDlg()
{
}

void CFilePanelDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TAB_PANEL, m_TabCtrl);
}


BEGIN_MESSAGE_MAP(CFilePanelDlg, CDialogEx)
	ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_PANEL, &CFilePanelDlg::OnTcnSelchangeTabPanel)
	ON_WM_CLOSE()
END_MESSAGE_MAP()


// CPanelDlg 消息处理程序
void CFilePanelDlg::InitInsideModule()
{
	CRect rs;
	m_TabCtrl.GetClientRect(&rs);
	rs.top += 21;

	m_TabCtrl.InsertItem(0,_T("File Manager"));
	m_TabCtrl.InsertItem(1,_T("Transfer"));

	CLIENT_INFO info;
	GetClientInfo(m_clientid,&info);

	FileDlg.Create(IDD_DIALOG_FILE);
	TransDlg.Create(IDD_DIALOG_TRANSFER);

	CWnd* pWnd = CWnd::FromHandle(m_TabCtrl.GetSafeHwnd());

	FileDlg.SetParent(pWnd);
	TransDlg.SetParent(pWnd);

	TransDlg.MoveWindow(&rs);
	FileDlg.MoveWindow(&rs);

	FileDlg.ShowWindow(TRUE);
	TransDlg.ShowWindow(FALSE);
}


void CFilePanelDlg::InitData()
{
	InitInsideModule();
}

void CFilePanelDlg::InitView()
{	
	CString strTitle;
	CLIENT_INFO info;

	GetClientInfo(m_clientid,&info);

	IN_ADDR connectIP;
	connectIP.S_un.S_addr = info.connectIP;

	strTitle.Format(_T("Control Panel [%s][%s]"),info.computerName,CString(inet_ntoa(connectIP)).GetBuffer());

	SetWindowText(strTitle);
}

BOOL CFilePanelDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	InitView();
	InitData();
	return TRUE;
}

void CFilePanelDlg::OnTcnSelchangeTabPanel(NMHDR *pNMHDR, LRESULT *pResult)
{
	int nSel = m_TabCtrl.GetCurSel();

	if (0 == nSel)
	{
		FileDlg.ShowWindow(TRUE);
		TransDlg.ShowWindow(FALSE);
	}
	else
	{
		FileDlg.ShowWindow(FALSE);
		TransDlg.ShowWindow(TRUE);
	}
	*pResult = 0;
}

void CFilePanelDlg::OnClose()
{
	CDialogEx::OnClose();
}
