#pragma once
#include <map>
#include "MasterModuleInterface.h"
#include "ClientControlPanelManager.h"

class MasterModuleManager
{
	DECLARE_SINGLETON(MasterModuleManager);
public:
	void GetModuleNames(TStringVector& names);
	void LoadAllModules(ModuleMap& moduleMap);
	void GetModuleWndClass();

private:
	BOOL CheckModuleInfo(MODULE_INFO& info);
	CriticalSection	m_mapSection;
	ModuleMap m_moduleMap;
};
