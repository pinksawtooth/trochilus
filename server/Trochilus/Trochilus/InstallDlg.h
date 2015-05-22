#pragma once
#include "afxcmn.h"


// CInstallDlg 对话框

class CInstallDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CInstallDlg)

public:
	CInstallDlg(CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CInstallDlg();

	void Init(LPCTSTR clientid,LPCTSTR ModuleName);

// 对话框数据
	enum { IDD = IDD_DIALOG_INSTALL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	//检查模块状况
	static DWORD WINAPI CheckModule(LPVOID lpParameter);
	DWORD WINAPI CheckModuleProc();


	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	CProgressCtrl m_insProgress;

	CString m_clientid;
	CString m_modname;

	RepeatTask m_CheckRepeat;
	afx_msg void OnBnClickedButtonInstall();
};
