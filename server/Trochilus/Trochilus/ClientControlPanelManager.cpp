#include "stdafx.h"
#include "PanelDlg.h"
#include "ClientControlPanelManager.h"

ClientControlPanelManager::ClientControlPanelManager()
{

}

ClientControlPanelManager::~ClientControlPanelManager()
{

}

BOOL ClientControlPanelManager::Init()
{
	return TRUE;
}

void ClientControlPanelManager::Deinit()
{

}

BOOL ClientControlPanelManager::OpenControlPanel( LPCTSTR clientid )
{
	BOOL bAlreadyExists = TRUE;
	CPanelDlg* pPanel = NULL;
	CString strClientId = clientid;

	m_mapSection.Enter();
	{
		PanelMap::iterator iter = m_map.find(strClientId);
		if (iter == m_map.end())
		{
			bAlreadyExists = FALSE;

			pPanel = new CPanelDlg();
			m_map.insert(PanelMap::value_type(strClientId, pPanel));
			pPanel->SetClientID(strClientId);
		}
		else
		{
			pPanel = iter->second;
			SwitchToThisWindow(pPanel->GetSafeHwnd(),TRUE);
			pPanel = NULL;
		}
	}
	m_mapSection.Leave();

	if (NULL != pPanel)
	{
		pPanel->DoModal();
		NotifyControlPanelClosed(strClientId);
	}
	return !bAlreadyExists;
}

CPanelDlg* ClientControlPanelManager::GetControlPanel( LPCTSTR clientid )
{
	CPanelDlg* pPanel = NULL;
	m_mapSection.Enter();
	{
		PanelMap::iterator iter = m_map.find(clientid);
		if (iter != m_map.end())
		{
			pPanel = iter->second;
		}
	}
	m_mapSection.Leave();

	return pPanel;
}

void ClientControlPanelManager::DelWorkThread(LPCTSTR clientid,DWORD id)
{
	m_mapSection.Enter();
	{
		ThreadIdMap::iterator it = m_threadList.find(clientid);

		if (it != m_threadList.end())
		{
			ThreadIdList::iterator listit = it->second.begin();
			for (; listit != it->second.end(); listit++)
			{
				if (*listit)
				{
					listit = it->second.erase(listit);
				}
				if (listit == it->second.end())
				{
					break;
				}
			}
		}
		else
		{
			return;
		}
	}
	m_mapSection.Leave();
}

void ClientControlPanelManager::AddWorkThread(LPCTSTR clientid,DWORD id)
{
	m_mapSection.Enter();
	{
		ThreadIdMap::iterator it = m_threadList.find(clientid);

		if (it != m_threadList.end())
		{
			it->second.push_back(id);
		}
		else
		{
			ThreadIdList list;
			list.push_back(id);
			m_threadList.insert(MAKE_PAIR(ThreadIdMap,clientid,list));
		}
	}
	m_mapSection.Leave();
}


void ClientControlPanelManager::HandleModuleMsg(LPCTSTR clientid, UINT nMsg, LPVOID lpContext, LPVOID lpParameter)
{
	DWORD id = GetCurrentThreadId();
	ClientControlPanelManager::GetInstanceRef().AddWorkThread(clientid,id);

	CPanelDlg* pPanel = ClientControlPanelManager::GetInstanceRef().GetControlPanel(clientid);

	if (pPanel == NULL)
	{
		return;
	}
	pPanel->HandleModuleMsg(nMsg,lpContext,lpParameter);
	ClientControlPanelManager::GetInstanceRef().DelWorkThread(clientid,id);
	
}
BOOL ClientControlPanelManager::ClosePanelDlg(LPCTSTR clientid)
{
	BOOL bRet = FALSE;
	CPanelDlg* pPanel = NULL;
	m_mapSection.Enter();
	{
		PanelMap::iterator iter = m_map.find(clientid);
		if (iter != m_map.end())
		{
			pPanel = iter->second;
			pPanel->SendMessage(WM_CLOSE,0,0);
			bRet = TRUE;
		}
	}
	m_mapSection.Leave();
	return bRet;
}
void ClientControlPanelManager::NotifyControlPanelClosed( LPCTSTR clientid )
{
	CPanelDlg* pPanel = NULL;
	m_mapSection.Enter();
	{
		ThreadIdMap::iterator it = m_threadList.find(clientid);
		if (it != m_threadList.end())
		{
			ThreadIdList::iterator listit = it->second.begin();
			for (; listit != it->second.end(); listit++)
			{
				HANDLE hThread = OpenThread(THREAD_ALL_ACCESS,FALSE,*listit);
				TerminateThread(hThread,0);
			}
			m_threadList.erase(it);
		}
		PanelMap::iterator iter = m_map.find(clientid);
		if (iter != m_map.end())
		{
			pPanel = iter->second;
			delete pPanel;
			m_map.erase(iter);
		}
	}
	m_mapSection.Leave();
}


