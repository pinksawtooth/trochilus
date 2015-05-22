#include "stdafx.h"
#include "MasterModuleManager.h"

MasterModuleManager::MasterModuleManager()
{

}

MasterModuleManager::~MasterModuleManager()
{

}

BOOL MasterModuleManager::Init()
{
	return TRUE;
}

void MasterModuleManager::Deinit()
{
}

// BOOL MasterModuleManager::CallModuleFunction( LPCTSTR moduleName, LPCTSTR funcname, LPCTSTR clientid, LPCTSTR argument, CString& result )
// {
// 	CString modname(moduleName);
// 	modname.MakeLower();
// 	modname.Trim();
// 
// 	//查找对应的模块
// 	BOOL bFoundModule = FALSE;
// 	MASTER_MODULE masterModule;
// 	m_mapSection.Enter();
// 	{
// 		MasterModuleMap::iterator iter = m_map.find(modname);
// 
// 		//如果map里没有这个模块，则尝试加载
// 		if (iter != m_map.end())
// 		{
// 			masterModule = iter->second;
// 			bFoundModule = TRUE;
// 		}
// 		else
// 		{
// 			CString dllpath;
// 			dllpath.Format(_T("%s%s.dll"), getBinFilePath(), modname);
// 			MASTER_MODULE mod;
// 			mod.hMod = ::LoadLibrary(dllpath);
// 			if (NULL != mod.hMod)
// 			{
// 				COMMANDER_API_LIST apilist;
// //				apilist.fnCallJs = old_CallApi;
// 				mod.fnInitMasterModule = (FnInitMasterModule) ::GetProcAddress(mod.hMod, "InitMasterModule");
// 				mod.fnDeinitMasterModule = (FnDeinitMasterModule) ::GetProcAddress(mod.hMod, "DeinitMasterModule");
// 				mod.fnQueryWincAPI = (FnQueryWincAPI) ::GetProcAddress(mod.hMod, "QueryWincAPI");
// 				if (mod.fnInitMasterModule != NULL 
// 					&& mod.fnDeinitMasterModule != NULL 
// 					&& mod.fnQueryWincAPI != NULL)
// 				{
// 					BOOL bInitRet = mod.fnInitMasterModule(&apilist);
// 					if (bInitRet)
// 					{
// 						m_map.insert(MasterModuleMap::value_type(modname, mod));
// 						masterModule = mod;
// 						bFoundModule = TRUE;
// 					}
// 					else
// 					{
// 						errorLog(_T("init master module[%s] failed"), dllpath);
// 					}
// 				}
// 			}
// 			else 
// 			{
// 				errorLogE(_T("load %s failed."), dllpath);
// 			}
// 		}
// 	}
// 	m_mapSection.Leave();
// 
// 	if (! bFoundModule)
// 	{
// 		errorLog(_T("Can not find module[%s]"), modname);
// 		return FALSE;
// 	}
// 
// 	//查询该模块是否提供了该回调函数
// 	FnWincCallback fnCallback = NULL;
// 	LPVOID lpParameter = NULL;
// 	if (! masterModule.fnQueryWincAPI(funcname, &fnCallback, &lpParameter))
// 	{
// 		errorLog(_T("mod[%s] has no function named[%s]"), modname, funcname);
// 		return FALSE;
// 	}
// 
// 	MyString myResult;
// 	if (! fnCallback(clientid, argument, lpParameter, &myResult))
// 	{
// 		errorLog(_T("call [%s][%s] failed"), modname, funcname);
// 		return FALSE;
// 	}
// 
// 	result = (LPCTSTR) myResult;
// 
// 	return TRUE;
// }

void MasterModuleManager::GetModuleNames( TStringVector& names )
{
}

BOOL MasterModuleManager::CheckModuleInfo(MODULE_INFO& info)
{
	return info.alloc && info.deinit && info.free && info.handler && info.init;
}
void MasterModuleManager::LoadAllModules( ModuleMap& moduleMap )
{
	WCHAR szPath[MAX_PATH] = {0};

	getBinFilePath(szPath,MAX_PATH);

	CString strPath = szPath;

	CString strFind = strPath +_T("mod*.dll");
	
	WIN32_FIND_DATA fData;
	HANDLE hFind = FindFirstFile(strFind,&fData);;
	do 
	{
		if (hFind == INVALID_HANDLE_VALUE)
			break;

		CString strTemp = strPath + fData.cFileName;

		HMODULE hMod = LoadLibrary(strTemp);

		MODULE_INFO info = {0};

		info.isInside = FALSE;

		info.alloc = (fnAllocWindow)GetProcAddress(hMod,"AllocWindow");
		info.free = (fnFreeWindow)GetProcAddress(hMod,"FreeWindow");
		info.init = (fnInitModule)GetProcAddress(hMod,"InitModule");
		info.deinit = (fnDeinitModule)GetProcAddress(hMod,"DeinitModule");
		info.handler = (fnHandleMsg)GetProcAddress(hMod,"HandleMsg");
		info.getname = (fnGetDisplayName)GetProcAddress(hMod,"GetModName");

		if (CheckModuleInfo(info))
		{
			moduleMap.insert(MAKE_PAIR(ModuleMap,moduleMap.size(),info));
		}
	} 
	while(FindNextFile(hFind,&fData));

	
}