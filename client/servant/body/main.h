#pragma once

#include "../../../common/CommNames.h"

#ifdef SERVANT_EXPORTS
#	define SERVANT_API extern "C" 
#else
#	define SERVANT_API extern "C" __declspec(dllimport)
#endif

SERVANT_API BOOL InitServant(const PCONFIG_INFO pConfigInfo);

SERVANT_API void DeinitServant();

//SERVANT_API BOOL SendMsg(const LPBYTE pData, DWORD dwSize, COMM_NAME commname = COMMNAME_DEFAULT, ULONG targetIP = 0);

SERVANT_API GUID GetCID();