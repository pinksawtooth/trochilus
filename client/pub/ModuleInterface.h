#pragma once
#include "MessageDefines.h"

#ifdef MODULE_IMPORTS
#define MODULE_API extern "C" __declspec(dllimport)
#else
#define MODULE_API extern "C" __declspec(dllexport)
#endif

typedef BOOL (*FnExecuteRCCommand)(MSGID msgid, const LPBYTE pData, DWORD dwSize, LPVOID lpParameter);

MODULE_API LPCTSTR GetModName();

MODULE_API BOOL InitModule();

MODULE_API BOOL StartModule();

MODULE_API void StopModule();

MODULE_API void DeinitModule();

MODULE_API BOOL QueryCommandHandler(MSGID msgid, FnExecuteRCCommand* ppHandler, LPVOID* ppParameter);
