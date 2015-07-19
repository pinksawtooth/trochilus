#pragma once
#include "ReportCtrl.h"

#include <list>
class CHostList : public CReportCtrl
{
public:
	CHostList(void);
	~CHostList(void);

	typedef std::map<tstring,CLIENT_INFO*> ClientList;
	typedef std::map<tstring,ClientList> GroupMap;

	typedef void (*FnClientCallBack)(CLIENT_INFO& info,LPVOID lpParameter);
	typedef std::map<CString,BOOL> IsAliveMap;
	IsAliveMap m_isAliveMap;

	void SetAlive(CString clientid,BOOL is)
	{
		m_isAliveMap[clientid] = is;
	}

	BOOL IsAlive(CString clientid)
	{
		if ( m_isAliveMap.find(clientid) != m_isAliveMap.end()) 
			return m_isAliveMap[clientid];

		return FALSE;
	}

public:
	//My function
	int GetInsertGroupsIndex(CString szGourps);
	BOOL AddClientInfo( CLIENT_INFO* pInfo );
	void DeleteClientInfo( CLIENT_INFO* pInfo);
	void InsertGroupsClient( int nIndex , ClientList& list );
	void DeleteGroupsClient( int nIndex , ClientList& list );

	virtual afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	virtual afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	GroupMap m_GroupsMap;

public:
	afx_msg void OnFilemanager();
	afx_msg void OnCommander();
	afx_msg void OnUninstall();

	DECLARE_MESSAGE_MAP()
};

