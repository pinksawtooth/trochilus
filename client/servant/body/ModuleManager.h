#pragma once
#include <map>
#include "Module.h"

class ModuleManager
{
public:
	BOOL AddModule(LPCTSTR name);
	BOOL AddModule(ByteBuffer& content,LPCTSTR name);
	BOOL DeleteModule(LPCTSTR name);
	BOOL AdjustModules();
	void DeinitAllModule();
	void ListModuleFileNames(TStringVector& names) const;
	void ListModuleNames(TStringVector& modnames) const;
	BOOL GetModule(LPCTSTR modname, Module** ppModule);
	BOOL QueryCommandHandler(MSGID msgid, FnExecuteRCCommand* ppHandler, LPVOID* ppParameter);

private:
	typedef struct  
	{
		tstring			dllname;
		Module			module;
	} MODULE_INFO;
	typedef std::map<tstring, MODULE_INFO> ModuleMap;

private:
	void AdjustModule(MODULE_INFO& info) const;

private:
	ModuleMap	m_map;
};
