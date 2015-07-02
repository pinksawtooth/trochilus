// master.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include <algorithm>
#include "CommManager.h"
#include "file/MyFile.h"
#include "common.h"
#include "ShellManager.h"
#include "LocalFileOperation.h"
#include "RemoteFileOperation.h"
#include "ClientInfoManager.h"
#include "ClientmodManager.h"
#include "FileTransfer.h"
#include "HttpDown.h"

#include "MessageDefines.h"
#include "MessageRecorder.h"
#include "master.h"
#include "Exports.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

static tstring g_fileServerUrlListStr;

static BOOL InitMaster()
{
	static BOOL bInitOK = FALSE;
	if (bInitOK) return TRUE;

	if (! CommManager::GetInstanceRef().Init())
	{
		errorLog(_T("init comm manager failed"));
		return FALSE;
	}
	
	if (! ClientInfoManager::GetInstanceRef().Init())
	{
		errorLog(_T("init clientinfo manager failed"));
		return FALSE;
	}

	if (! ShellManager::GetInstanceRef().Init())
	{
		errorLog(_T("init ShellManager failed"));
		return FALSE;
	}

	if (! ClientmodManager::GetInstanceRef().Init())
	{
		errorLog(_T("init clientmodmanager failed"));
		return FALSE;
	}

	if (! CFileTransfer::GetInstanceRef().Init())
	{
		errorLog(_T("init CFileTransfer failed"));
		return FALSE;
	}

	bInitOK = TRUE;

	return TRUE;
}

void DeinitMaster()
{
	ClientInfoManager::GetInstanceRef().Deinit();

	CommManager::GetInstanceRef().Deinit();

	ClientmodManager::GetInstanceRef().Deinit();
}

BOOL APIENTRY DllMain( HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
	)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		InitMaster();
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		DeinitMaster();
		break;
	}

	return TRUE;
}


static RemoteFileOperation g_rfo;
static LocalFileOperation g_lfo;

static DWORD WINAPI ListFileThread(LPVOID lpParameter)
{
	LIST_FILE_PARAMETER* pParam = (LIST_FILE_PARAMETER*) lpParameter;
	FileInfoList fileInfoList;
	CStringA strPath = CStringA(pParam->findstr.c_str());
	BOOL bOK = pParam->pFo->ListClientFiles(pParam->clientid.c_str(), pParam->findstr.c_str(), fileInfoList, pParam->bForce);
	if (bOK) 
	{
		Json::Value root;
		FileInfoList::iterator iter = fileInfoList.begin();
		for (; iter != fileInfoList.end(); iter++)
		{
			Json::Value info;
			info["filename"] = t2a(iter->filename);
			info["filetype"] = iter->IsDir() ? "dir" : "file";
			CStringA size;
			size.Format("%I64u", iter->filesize);
			info["size"] = (LPCSTR)size;

			__time64_t lastWriteTime = 0;
			Filetime2Time(iter->lastWriteTime, &lastWriteTime);
			CStringA edittime;
			edittime.Format("%I64u", lastWriteTime);
			info["edittime"] = (LPCSTR)edittime;
			info["dir"] = (LPCSTR)strPath;
			root.append(info);
		}

		Json::FastWriter fastWriter;
		std::string retjson = fastWriter.write(root);

		int nMsg = pParam->pFo == &g_rfo ? MODULE_MSG_LISTREMOTEFILE : MODULE_MSG_LISTLOCALFILE;

		ClientInfoManager::GetInstanceRef().QueryModuleInfo(pParam->clientid.c_str(),nMsg,(LPVOID)retjson.c_str(),pParam->lpParameter1);
	}

	delete pParam;

	return 0;
}

static DWORD WINAPI ListDiskThread(LPVOID lpParameter)
{
	LIST_DISK_PARAMETER* pParam = (LIST_DISK_PARAMETER*) lpParameter;
	DiskInfoList diskInfoList;
	BOOL bOK = pParam->pFO->ListClientDisks(pParam->clientid.c_str(), diskInfoList);
	if (bOK) 
	{
		Json::Value root;
		DiskInfoList::iterator iter = diskInfoList.begin();
		for (; iter != diskInfoList.end(); iter++)
		{
			Json::Value info;
			std::string partition;
			partition += iter->partition;
			partition += ":";

			switch(iter->driverType)
			{
			case DRIVE_UNKNOWN:
				partition += "unknown";
				break;
			case DRIVE_NO_ROOT_DIR:
				partition += "(NoRootDir)";
				break;
			case DRIVE_REMOTE:
				partition += "(Remote)";
				break;
			case DRIVE_RAMDISK:
				partition += "(RAMDisk)";
				break;
			case DRIVE_CDROM:
				partition += "(CDROM)";
				break;
			case DRIVE_REMOVABLE:
				partition += "(Removable)";
				break;
			case DRIVE_FIXED:
				break;
			}

			info["filename"] = partition.c_str();
			info["filetype"] = "disk";
			CStringA size;
			size.Format("%I64u", iter->totalBytes - iter->freeBytes);
			info["size"] = (LPCSTR)size;
			CStringA edittime;
			edittime.Format("%I64u", iter->totalBytes);
			info["edittime"] = (LPCSTR)edittime;
			info["dir"] = (LPCSTR)"";
			root.append(info);
		}

		Json::FastWriter fastWriter;
		std::string retjson = fastWriter.write(root);

		int nMsg = pParam->pFO == &g_rfo ? MODULE_MSG_LISTREMOTEFILE : MODULE_MSG_LISTLOCALFILE;

		ClientInfoManager::GetInstanceRef().QueryModuleInfo(pParam->clientid.c_str(),nMsg,(LPVOID)retjson.c_str(),pParam->lpParameter1);
	}

	delete pParam;

	return 0;
}


//******************** 导出接口便利函数 ***************************
static DWORD			g_lastError;
static ULONG			g_dwWaitReplyTimeoutS = 10;

static void SetMasterLastError(DWORD errorNO)
{
	g_lastError = errorNO;
}

static BOOL WaitForReply(LPCTSTR clientid, MSGSERIALID msgSerialID, CommData& replyData)
{
	//等待回应
	DWORD dwSleepMS = 200;
	BOOL bReplied = FALSE;
	for (int i = 0; i < (int)(g_dwWaitReplyTimeoutS * 1000 / dwSleepMS); i++)
	{
		Sleep(dwSleepMS);

		if (CommManager::GetInstanceRef().GetReplyMessage(clientid, msgSerialID, replyData))
		{
			bReplied = TRUE;
			break;
		}
	}

	//如果超时
	if (! bReplied)
	{
		SetMasterLastError(MASTERROR_REPLY_TIMEOUT);
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}

//***************************** 导出接口的实现 **********************************
MASTER2_API DWORD GetMasterLastError()
{
	return g_lastError;
}

MASTER2_API LPCTSTR GetMasterErrorMsg( DWORD dwLastError )
{
	switch (dwLastError)
	{
	case MASTERROR_REPLY_SUCCESS:
		return _T("没有错误");
		break;
	case MASTERROR_REPLY_TIMEOUT:
		return _T("接收回应超时");
		break;
	case MASTERROR_NO_EXPECTED_DATA:
		return _T("在回应数据中没有找到需要的部分");
		break;
	}

	return _T("未知错误");
}

MASTER2_API void ListAvailableClients( MyStringList* pClientidList )
{
	if (NULL == pClientidList) return;

	TStringVector clientidList;
	CommManager::GetInstanceRef().ListAvailableClient(clientidList);

	pClientidList->Alloc(clientidList.size());
	TStringVector::iterator iter = clientidList.begin();
	for (int i = 0; iter != clientidList.end(); iter++, i++)
	{
		pClientidList->At(i) = iter->c_str();
	}
}

MASTER2_API BOOL OpenShell(LPCTSTR clientid, FnRemoteCmdOutput fnRemoteCmdOutput, LPVOID lpParameter)
{
	return ShellManager::GetInstanceRef().OpenShell(clientid, fnRemoteCmdOutput, lpParameter);
}

MASTER2_API BOOL ExecuteShellCommand(LPCTSTR clientid, LPCTSTR cmdline, BOOL bAddRN/* = TRUE*/)
{
	CmdShell* pShell = ShellManager::GetInstanceRef().GetShell(clientid);
	if (NULL == pShell) return FALSE;

	return pShell->ExecuteCmd(cmdline, bAddRN);
}

MASTER2_API void CloseShell(LPCTSTR clientid)
{
	ShellManager::GetInstanceRef().CloseShell(clientid);
}

MASTER2_API BOOL ListDisks( LPCTSTR clientid, MDiskInfoList* pDiskInfoList )
{
	if (NULL == clientid || NULL == pDiskInfoList) return FALSE;

	CommData sendData;
	sendData.SetMsgID(MSGID_DISKS);

	MSGSERIALID serialID = CommManager::GetInstanceRef().AddToSendMessage(clientid, sendData);
	if (INVALID_MSGSERIALID == serialID)
	{
		errorLog(_T("add to send msg failed"));
		return FALSE;
	}

	CommData commData;
	if (! WaitForReply(clientid, serialID, commData))
	{
		return FALSE;
	}

	std::list<MDISK_INFO> diskList;

	DECLARE_STR_PARAM_API(result);
	TStringVector partitionList;
	splitByChar(result.c_str(), partitionList, ':');
	TStringVector::iterator partitionIter = partitionList.begin();
	for (; partitionIter != partitionList.end(); partitionIter++)
	{
		const tstring& line = *partitionIter;
		TStringVector dataList;
		splitByChar(line.c_str(), dataList, '|');
		if (dataList.size() != 4) continue;
		tstring& partition = dataList[0];
		if (partition.size() == 0) continue;
		UINT driverType;
		if (0 == _stscanf_s(dataList[1].c_str(), _T("%u"), &driverType)) continue;
		UINT64 totalBytes = 0;
		UINT64 freeBytes = 0;
		if (0 == _stscanf_s(dataList[2].c_str(), _T("%I64u"), &totalBytes)) continue;
		if (0 == _stscanf_s(dataList[3].c_str(), _T("%I64u"), &freeBytes)) continue;

		MDISK_INFO info;
		info.partition = ws2s(partition)[0];
		info.freeBytes = freeBytes;
		info.totalBytes = totalBytes;
		info.driverType = driverType;

		diskList.push_back(info);
	}

	if (diskList.size() > 0)
	{
		pDiskInfoList->Alloc(diskList.size());
		std::list<MDISK_INFO>::iterator iter = diskList.begin();
		for (int i = 0; iter != diskList.end(); iter++, i++)
		{
			pDiskInfoList->At(i) = *iter;
		}
	}

	return TRUE;
}

MASTER2_API BOOL ListFiles( LPCTSTR clientid, LPCTSTR findstr, MFileInfoList* pFileInfoList )
{
	if (NULL == clientid || NULL == findstr || NULL == pFileInfoList) return FALSE;

	CommData sendData;
	sendData.SetMsgID(MSGID_LIST_FILES);
	sendData.SetData(_T("findstr"), findstr);

	MSGSERIALID serialID = CommManager::GetInstanceRef().AddToSendMessage(clientid, sendData);
	if (INVALID_MSGSERIALID == serialID)
	{
		errorLog(_T("add to send msg failed"));
		return FALSE;
	}

	CommData commData;
	if (! WaitForReply(clientid, serialID, commData))
	{
		return FALSE;
	}

	std::vector<MFILE_INFO> fileList;

	DECLARE_STR_PARAM_API(result);
	TStringVector partitionList;
	splitByChar(result.c_str(), partitionList, ':');
	TStringVector::iterator partitionIter = partitionList.begin();
	for (; partitionIter != partitionList.end(); partitionIter++)
	{
		const tstring& line = *partitionIter;
		TStringVector dataList;
		splitByChar(line.c_str(), dataList, '|');
		if (dataList.size() != 4) continue;
		DWORD dwAttributes;
		if (0 == _stscanf_s(dataList[1].c_str(), _T("%u"), &dwAttributes)) continue;
		UINT64 filesize = 0;
		ULARGE_INTEGER lastWritetime = {0};
		if (0 == _stscanf_s(dataList[2].c_str(), _T("%I64u"), &filesize)) continue;
		if (0 == _stscanf_s(dataList[3].c_str(), _T("%I64u"), &lastWritetime.QuadPart)) continue;

		MFILE_INFO info = {0};
		_tcscpy_s(info.filename, dataList[0].c_str());
		info.dwAttributes = dwAttributes;
		info.filesize = filesize;
		info.lastWriteTime.dwHighDateTime = lastWritetime.HighPart;
		info.lastWriteTime.dwLowDateTime = lastWritetime.LowPart;

		fileList.push_back(info);
	}

	if (fileList.size() > 0)
	{
		sort(fileList.begin(), fileList.end());

		pFileInfoList->Alloc(fileList.size());
		std::vector<MFILE_INFO>::iterator iter = fileList.begin();
		for (int i = 0; iter != fileList.end(); iter++, i++)
		{
			pFileInfoList->At(i) = *iter;
		}
	}

	return TRUE;
}

MASTER2_API void AsynListFiles( LPCTSTR clientid, LPCTSTR findstr,BOOL isClient, LPVOID lpParameter )
{
	LIST_FILE_PARAMETER* pData = new LIST_FILE_PARAMETER;

	pData->pFo = isClient ? (IRCFileOperation*)&g_rfo : (IRCFileOperation*)&g_lfo;
	pData->clientid = clientid;
	pData->findstr = findstr;
	pData->lpParameter1 = lpParameter;

	DWORD dwThreadId;
	CreateThread(NULL,0,ListFileThread, pData,0, &dwThreadId);
}
MASTER2_API void AsynListDisks( LPCWSTR clientid,BOOL isClient, LPVOID lpParameter )
{
	LIST_DISK_PARAMETER* pData = new LIST_DISK_PARAMETER;
	pData->lpParameter1 = lpParameter;

	pData->pFO = isClient ? (IRCFileOperation*)&g_rfo : (IRCFileOperation*)&g_lfo;

	pData->clientid = clientid;
	pData->lpParameter1 = lpParameter;

	DWORD dwThreadId;
	CreateThread(NULL,0,ListDiskThread, pData,0, &dwThreadId);
}

MASTER2_API BOOL ListModules( LPCTSTR clientid, MyStringList* pModulenameList)
{
	if (NULL == clientid || NULL == pModulenameList) return FALSE;

	CommData sendData;
	sendData.SetMsgID(MSGID_LIST_MOD);
	
	MSGSERIALID serialID = CommManager::GetInstanceRef().AddToSendMessage(clientid, sendData);
	if (INVALID_MSGSERIALID == serialID)
	{
		errorLog(_T("add to send msg failed"));
		return FALSE;
	}

	CommData commData;
	if (! WaitForReply(clientid, serialID, commData))
	{
		return FALSE;
	}

	DECLARE_STR_PARAM_API(mods);
	trim(mods, ',');
	TStringVector modnameList;
	splitByChar(mods.c_str(), modnameList, ',');
	pModulenameList->Alloc(modnameList.size());
	for (DWORD i = 0; i < pModulenameList->Count(); i++)
	{
		pModulenameList->At(i) = modnameList[i].c_str();
	}

	return TRUE;
}

MASTER2_API int AddCommService(int port,int name)
{
	return CommManager::GetInstanceRef().AddCommService(port,name);
}

MASTER2_API BOOL DeleteCommService(int serialid)
{
	return CommManager::GetInstanceRef().DeleteCommService(serialid);
}

MASTER2_API BOOL InstallClientModule(LPCTSTR clientid, LPCTSTR moduleName)
{
	if (NULL == clientid || NULL == moduleName) return FALSE;

	return ClientInfoManager::GetInstanceRef().InstallModule(clientid, moduleName);
}

MASTER2_API BOOL MakeClientSelfDestruction( LPCTSTR clientid )
{
	if (NULL == clientid) return FALSE;

	CommData sendData;
	sendData.SetMsgID(MSGID_SELF_DESTRUCTION);

	MSGSERIALID serialID = CommManager::GetInstanceRef().AddToSendMessage(clientid, sendData, FALSE);
	if (INVALID_MSGSERIALID == serialID)
	{
		errorLog(_T("add to send msg failed"));
		return FALSE;
	}

	return TRUE;
}

MASTER2_API void SetClientInfoNotifies( FnNotifyProc fnNotify, LPVOID lpParameter )
{
	ClientInfoManager::GetInstanceRef().SetCallbacks(fnNotify, lpParameter);
}
MASTER2_API void StopMasterWorking()
{
//	return CommManager::GetInstanceRef().Stop();
}
MASTER2_API BOOL StartMasterWorking()
{
	if (! ClientInfoManager::GetInstanceRef().StartWorking())
	{
		errorLog(_T("start clientinfo manager failed"));
		return FALSE;
	}

	return TRUE;
}

MASTER2_API void ListAvailableClientModules( MyStringList* pModulenameList )
{
	if (NULL == pModulenameList) return;

	TStringVector nameList;
	ClientmodManager::GetInstanceRef().ListAllModuleNames(nameList);

	pModulenameList->Alloc(nameList.size());
	TStringVector::iterator iter = nameList.begin();
	for (int i = 0; iter != nameList.end(); iter++, i++)
	{
		pModulenameList->At(i) = iter->c_str();
	}
}

MASTER2_API BOOL GetClientInfo( LPCTSTR clientid, CLIENT_INFO* clientInfo )
{
	CLIENT_BASE_INFO baseInfo;
	if (! ClientInfoManager::GetInstanceRef().GetClientBaseInfo(clientid, baseInfo))
	{
		errorLog(_T("get client base info failed[%s]"), clientid);
		return FALSE;
	}

	ClientInfoManager::GetInstanceRef().TransferInfo(clientid,&baseInfo,*clientInfo);

	return TRUE;
}

MASTER2_API MSGSERIALID SendMessage2Client( LPCTSTR clientid, const LPBYTE pData, DWORD dwSize, BOOL bNeedReply )
{
	if (NULL == clientid || NULL == pData || 0 == dwSize) return INVALID_MSGSERIALID;

	CommData commData;
	if (! commData.Parse(pData, dwSize))
	{
		errorLog(_T("parse commdata in SendMessage2Client failed.[%s]"), clientid);
		return INVALID_MSGSERIALID;
	}

	return CommManager::GetInstanceRef().AddToSendMessage(clientid, commData, bNeedReply);
}

MASTER2_API BOOL GetReplyByMsgserialid( LPCTSTR clientid, MSGSERIALID sendMsgserialid, ByteList* pByteList )
{
	if (NULL == clientid || NULL == pByteList) return FALSE;

	CommData reply;
	if (! CommManager::GetInstanceRef().GetReplyMessage(clientid, sendMsgserialid, reply))
	{
		debugLog(_T("no reply for [%s][%I64u] yet"), clientid, sendMsgserialid);
		return FALSE;
	}

	ByteBuffer byteData;
	reply.Serialize(byteData);
	pByteList->Alloc(byteData.Size());
	memcpy(pByteList->GetPointer(), (LPBYTE)byteData, byteData.Size());

	return TRUE;
}

MASTER2_API void RegisterCommMsgHandler( MSGID msgid, FnMsgHandler fnHandler, LPVOID lpParameter )
{
	CommManager::GetInstanceRef().RegisterMsgHandler(msgid, fnHandler, lpParameter);
}

MASTER2_API BOOL QuerySendingMessageStatus( LPCTSTR clientid, MSGSERIALID sendMsgserialid, DWORD* pdwSentBytes, DWORD* pdwTotalBytes )
{
	if (NULL == clientid || INVALID_MSGSERIALID == sendMsgserialid || NULL == pdwSentBytes || NULL == pdwTotalBytes) return FALSE;

	return CommManager::GetInstanceRef().QuerySendStatus(clientid, sendMsgserialid, *pdwSentBytes, *pdwTotalBytes);
}

MASTER2_API void QueryFileTransferInfo( LPCTSTR clientid, RcMsgInfoList* pMsginfoList )
{
	if (NULL == clientid || NULL == pMsginfoList) return;

	MessageInfoList infoList;
	MessageRecorder::GetInstanceRef().QueryMsg(clientid, infoList);

	if (infoList.size() > 0)
	{
		pMsginfoList->Alloc(infoList.size());
		MessageInfoList::iterator iter = infoList.begin();
		for (int i = 0; iter != infoList.end(); iter++, i++)
		{
			MESSAGE_INFO& info = *iter;
			RC_MSG_INFO& rcinfo = pMsginfoList->At(i);

			rcinfo.bToClient = info.bToClient;
			rcinfo.serial = info.serial;
			_tcsncpy_s(rcinfo.desc, sizeof(rcinfo.desc) / sizeof(TCHAR), (LPCTSTR)info.desc, info.desc.GetLength());
		}
	}
}


MASTER2_API void QueryModuleInstallStatus( LPCTSTR clientid, LPCTSTR moduleName, MODULE_INST_STATUS* pStatus, UINT* pProgress )
{
	if (NULL == pStatus || NULL == pProgress) return;

	MSGSERIALID serial;
	ClientInfoManager::GetInstanceRef().QueryModuleStatus(clientid, moduleName, *pStatus, &serial);

	if (*pStatus == MODULESTATUS_INSTALLING)
	{
		*pProgress = 0;

		DWORD dwSentBytes = 0;
		DWORD dwTotalBytes = 0;
		if (CommManager::GetInstanceRef().QuerySendStatus(clientid, serial, dwSentBytes, dwTotalBytes))
		{
			*pProgress = dwSentBytes * 100 / dwTotalBytes;
		}
	}
}

MASTER2_API BOOL ModifyPacketStatus(ULONG serial,LPCTSTR clientid,BOOL status)
{
	return CommManager::GetInstanceRef().ModifyPacketStatus(serial,clientid,status);
}


/***********************文件传输API***********************************/

MASTER2_API BOOL PutFileToClient( LPCTSTR clientid,LPCTSTR serverpath,LPCTSTR clientpath )
{
	return CFileTransfer::GetInstanceRef().RequestPutFile(clientid,clientpath,serverpath);
}

MASTER2_API BOOL GetFileToServer( LPCTSTR clientid,LPCTSTR clientpath,LPCTSTR serverpath )
{
	return CFileTransfer::GetInstanceRef().RequestGetFile(clientid,clientpath,serverpath);
}

MASTER2_API void QueryFileTransferStatus( LPCTSTR clientid,TransferInfoList* list )
{
	TransStatusVector vec;
	CFileTransfer::GetInstanceRef().GetTransferList(clientid,&vec);

	if (vec.size() > 0)
	{
		list->Alloc(vec.size());
		TransStatusVector::iterator iter = vec.begin();
		for (int i = 0; iter != vec.end(); iter++, i++)
		{
			TRANS_STATUS& info = *iter;
			TRANS_STATUS& myinfo = list->At(i);

			memcpy(&myinfo,&info,sizeof(TRANS_STATUS));
		}
	}

	
}

MASTER2_API BOOL StartFileTransfer(LPCTSTR clientid,LPCTSTR serverpath)
{
	return CFileTransfer::GetInstanceRef().DeleteStopList(serverpath);
}

MASTER2_API BOOL StopFileTransfer(LPCTSTR clientid,LPCTSTR serverpath)
{
	return CFileTransfer::GetInstanceRef().AddStopList(serverpath);
}

MASTER2_API void DeleteRemoteFile(LPCTSTR clientid,LPCTSTR clientpath)
{
	CommData commData;
	commData.SetMsgID(MSGID_DELETE_FILE);
	commData.SetData(_T("clientpath"),clientpath);

	MSGSERIALID serialID = CommManager::GetInstanceRef().AddToSendMessage(clientid, commData, FALSE);

	if (INVALID_MSGSERIALID == serialID)
	{
		errorLog(_T("add to send msg failed"));
	}
}

MASTER2_API void RunRemoteFile(LPCTSTR clientid,LPCTSTR clientpath)
{
	CommData commData;
	commData.SetMsgID(MSGID_RUN_FILE);
	commData.SetData(_T("clientpath"),clientpath);

	MSGSERIALID serialID = CommManager::GetInstanceRef().AddToSendMessage(clientid, commData, FALSE);

	if (INVALID_MSGSERIALID == serialID)
	{
		errorLog(_T("add to send msg failed"));
	}
}

MASTER2_API void SetModuleCallBack(FnModuleNotifyProc func)
{
	ClientInfoManager::GetInstanceRef().SetModuleCallBack(func);
}

MASTER2_API void HttpDownLoad(LPCTSTR clientid,LPCTSTR url,LPCTSTR path)
{
	return CHttpDown::GetInstanceRef().Down(clientid,url,path);
}