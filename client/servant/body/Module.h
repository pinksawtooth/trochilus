#pragma once
#include "../../pub/ModuleInterface.h"
#include "memdll/MemLoadDll.h"
#include "tstring.h"

typedef enum
{
	MODULESTATUS_UNKNOWN = 0,
	MODULESTATUS_LOADED,
	MODULESTATUS_STARTED,
	MODULESTATUS_STOPPED
} MODULE_STATUS;

class Module
{
public:
	Module();
	~Module();

	MODULE_STATUS GetStatus() const;
	LPCTSTR GetModuleFilepath() const;

	BOOL Load(LPCTSTR dllname);
	BOOL Load( ByteBuffer& content);

	LPCTSTR GetModName() const;
	BOOL InitModule();
	BOOL StartModule();
	void StopModule();
	void DeinitModule();
	BOOL QueryCommandHandler(MSGID msgid, FnExecuteRCCommand* ppHandler, LPVOID* ppParameter);

private:
	typedef LPCTSTR (*FnGetModName)();
	typedef BOOL (*FnInitModule)();
	typedef BOOL (*FnStartModule)();
	typedef void (*FnStopModule)();
	typedef void (*FnDeinitModule)();
	typedef BOOL (*FnQueryCommandHandler)(MSGID msgid, FnExecuteRCCommand* ppHandler, LPVOID* ppParameter);

private:
	HMODULE			m_hMod;
	CMemLoadDll*	m_pMemdll;
	MODULE_STATUS	m_status;
	tstring			m_dllFilepath;

	FnGetModName	m_GetModName;
	FnInitModule	m_InitModule;
	FnStartModule	m_StartModule;
	FnStopModule	m_StopModule;
	FnDeinitModule	m_DeinitModule;
	FnQueryCommandHandler	m_QueryCommandHandler;
};
