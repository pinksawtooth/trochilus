#pragma once
#include <map>
#include <list>
#include "PanelDlg.h"

class ClientControlPanelManager
{
	DECLARE_SINGLETON(ClientControlPanelManager);
public:
	BOOL OpenControlPanel(LPCTSTR clientid);
	static void CALLBACK HandleModuleMsg(LPCTSTR clientid,UINT nMsg, LPVOID lpContext, LPVOID lpParameter);
	void NotifyControlPanelClosed(LPCTSTR clientid);
	CPanelDlg* GetControlPanel(LPCTSTR clientid);
	void AddWorkThread(LPCTSTR clientid,DWORD id);
	void DelWorkThread(LPCTSTR clientid,DWORD id);
	BOOL ModuleCallBack(LPCTSTR clientid, LPCTSTR strFunc, const CStringArray& paramArray, CString& ret);
	BOOL ClosePanelDlg(LPCTSTR clientid);
private:
	typedef std::map<CString, CPanelDlg*> PanelMap;
	typedef std::list<DWORD> ThreadIdList;
	typedef std::map<CString,ThreadIdList> ThreadIdMap;
private:
	CriticalSection	m_mapSection;
	PanelMap		m_map;
	ThreadIdMap m_threadList;
};
