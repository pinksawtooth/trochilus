#include "stdafx.h"
#include "ModuleManager.h"

BOOL ModuleManager::AddModule(ByteBuffer& content,LPCTSTR name)
{
	tstring modname(name);
	makeLower(modname);

	MODULE_INFO temp;
	std::pair<ModuleMap::iterator, bool> result = m_map.insert(ModuleMap::value_type(modname, temp));
	if (! result.second) 
	{
		errorLog(_T("addmodule [%s] failed. insert failed"), name);
		return FALSE;
	}

	MODULE_INFO& info = result.first->second;
	info.dllname = modname;

	if (info.module.Load(content))
	{
		infoLog(_T("add module [%s] success"), name);
		return TRUE;
	}
	else
	{
		m_map.erase(result.first);
		infoLog(_T("add module [%s] failed"), name);
		return FALSE;
	}
}

BOOL ModuleManager::AddModule( LPCTSTR name )
{
	tstring modname(name);
	makeLower(modname);

	MODULE_INFO temp;
	std::pair<ModuleMap::iterator, bool> result = m_map.insert(ModuleMap::value_type(modname, temp));
	if (! result.second) 
	{
		errorLog(_T("addmodule [%s] failed. insert failed"), name);
		return FALSE;
	}

	MODULE_INFO& info = result.first->second;
	info.dllname = modname;

	if (info.module.Load(info.dllname.c_str()))
	{
		infoLog(_T("add module [%s] success"), name);
		return TRUE;
	}
	else
	{
		m_map.erase(result.first);
		infoLog(_T("add module [%s] failed"), name);
		return FALSE;
	}
}

BOOL ModuleManager::DeleteModule(LPCTSTR name)
{
	tstring modname(name);
	makeLower(modname);

	ModuleMap::iterator it = m_map.find(modname);
	if (it != m_map.end())
	{
		it->second.module.StopModule();
		m_map.erase(it);
	}
	return TRUE;
}

void ModuleManager::DeinitAllModule()
{
	ModuleMap::iterator iter = m_map.begin();
	for (; iter != m_map.end(); iter++)
	{
		MODULE_INFO& info = iter->second;

		if (MODULESTATUS_STARTED == info.module.GetStatus())
		{
			debugLog(_T("stop module [%s]"), info.dllname.c_str());
			info.module.StopModule();
		}

		debugLog(_T("deinit module [%s]"), info.dllname.c_str());
		info.module.DeinitModule();
	}

	debugLog(_T("deinit all modules"));
}

BOOL ModuleManager::AdjustModules()
{
	//遍历moduleMap中的所有模块
	ModuleMap::iterator moditer = m_map.begin();
	for (; moditer != m_map.end(); moditer++)
	{
		MODULE_INFO& info = moditer->second;

		AdjustModule(info);
	}

	debugLog(_T("modules num : %u"), m_map.size());

	return TRUE;
}

void ModuleManager::AdjustModule(MODULE_INFO& info) const
{
	if (info.module.GetStatus() == MODULESTATUS_UNKNOWN)
	{
		info.module.Load(info.dllname.c_str());
	}
	if (info.module.GetStatus() == MODULESTATUS_LOADED)
	{
		info.module.InitModule();
	}

	if (info.module.GetStatus() == MODULESTATUS_STOPPED)
	{
		info.module.StartModule();
	}
}

void ModuleManager::ListModuleFileNames( TStringVector& names ) const
{
	ModuleMap::const_iterator iter = m_map.begin();
	for (; iter != m_map.end(); iter++)
	{
		names.push_back(iter->first);
	}
}

void ModuleManager::ListModuleNames( TStringVector& modnames ) const
{
	ModuleMap::const_iterator iter = m_map.begin();
	for (; iter != m_map.end(); iter++)
	{
		LPCTSTR modname = iter->second.module.GetModName();
		if (NULL != modname)
		{
			modnames.push_back(modname);
		}
	}
}

BOOL ModuleManager::QueryCommandHandler( MSGID msgid, FnExecuteRCCommand* ppHandler, LPVOID* ppParameter )
{
	ModuleMap::iterator iter = m_map.begin();
	for (; iter != m_map.end(); iter++)
	{
		if (iter->second.module.QueryCommandHandler(msgid, ppHandler, ppParameter))
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOL ModuleManager::GetModule( LPCTSTR modname, Module** ppModule )
{
	ModuleMap::iterator finditer = m_map.find(modname);
	if (finditer == m_map.end())
	{
		return FALSE;
	}

	*ppModule = &finditer->second.module;

	return TRUE;
}
