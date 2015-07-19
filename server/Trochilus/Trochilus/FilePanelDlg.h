#pragma once
#include "afxcmn.h"
#include "FileMgrDlg.h"
#include "TransferDlg.h"
#include "CmdDlg.h"
// CPanelDlg 对话框



class CFilePanelDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CFilePanelDlg)

public:
	CFilePanelDlg(CWnd* pParent = NULL);
	virtual ~CFilePanelDlg();
	// 对话框数据
	enum { IDD = IDD_DIALOG_PANEL };

protected:

	virtual void DoDataExchange(CDataExchange* pDX);
	DECLARE_MESSAGE_MAP()

public:
	//初始化界面和工作状态
	void InitView();
	void InitData();
	void InitInsideModule();
	virtual BOOL OnInitDialog();

	//处理模块状态信息
	void HandleMsgNotify(UINT nType, LPCTSTR lpContext);

	//当Tab框选择改变
	afx_msg void OnTcnSelchangeTabPanel(NMHDR *pNMHDR, LRESULT *pResult);

	//设置客户端ID
	void SetClientID(LPCTSTR clientid)
	{
		m_clientid = clientid;
		
		FileDlg.InitModule(clientid);
		TransDlg.InitModule(clientid);

	}

private:

	CTabCtrl m_TabCtrl;

	typedef std::map<tstring,tstring> ModuleCallBack;
	ModuleCallBack m_moduleinfo;

	CString m_clientid;
	CriticalSection m_csModuleMap;

	CFileMgrDlg FileDlg;
	CTransferDlg TransDlg;

public:
	afx_msg void OnClose();
};
