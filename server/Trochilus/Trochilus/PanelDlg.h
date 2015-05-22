#pragma once
#include "afxcmn.h"
#include "FileMgrDlg.h"
#include "TransferDlg.h"
#include "CmdDlg.h"
// CPanelDlg 对话框



class CPanelDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CPanelDlg)

public:
	CPanelDlg(CWnd* pParent = NULL);
	virtual ~CPanelDlg();
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
	void InitOutsideModule();
	virtual BOOL OnInitDialog();

	//处理模块回调消息
	void HandleModuleMsg(UINT nMsg, LPVOID lpContext, LPVOID lpParameter);

	//处理模块状态信息
	void HandleMsgNotify(UINT nType, LPCTSTR lpContext);

	//当Tab框选择改变
	afx_msg void OnTcnSelchangeTabPanel(NMHDR *pNMHDR, LRESULT *pResult);

	//设置客户端ID
	void SetClientID(LPCTSTR clientid)
	{
		m_clientid = clientid;
	}

private:

	CTabCtrl m_TabCtrl;
	CListCtrl m_msgList;
	CImageList m_ImageList;

	typedef std::map<tstring,tstring> ModuleCallBack;
	ModuleCallBack m_moduleinfo;

	CString m_clientid;
	ModuleMap m_moduleMap;
	CriticalSection m_csModuleMap;

	typedef std::list<CString> ClientModList;
	ClientModList m_clientModList;


	typedef std::list<CDialogEx*> NewMemList;

	NewMemList m_MemList;

public:
	afx_msg void OnClose();
};
