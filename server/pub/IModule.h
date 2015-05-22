#pragma once

class IModule
{
public:
	CString m_clientid;
	CString m_modname;
	CLIENT_INFO m_clientinfo;
	FnModuleNotifyProc m_handleMsg;

	static BOOL InitModule(LPCTSTR clientid,CLIENT_INFO& info,FnModuleNotifyProc fnCallback,LPVOID lpParameter)
	{
		IModule* lpModule = dynamic_cast<IModule*>((CDialogEx*)lpParameter);
		lpModule->m_clientid = clientid;
		lpModule->m_clientinfo = info;
		lpModule->m_handleMsg = fnCallback;

		return TRUE;
	}

	void CommitNotifyMsg(int nType,LPCTSTR content)
	{
		m_handleMsg(m_clientid,MODULE_MSG_NOTIFYMSG,(LPVOID)content,(LPVOID)nType);
	}
};