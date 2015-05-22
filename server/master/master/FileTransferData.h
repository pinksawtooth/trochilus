#pragma once
#include <vector>

typedef struct _TRANS_STATUS
{
	WCHAR strSPath[255];
	WCHAR strCPath[255];
	UINT64	nTotal;
	UINT64	nCurPos;
	BOOL	isDown;
}TRANS_STATUS,*PTRANS_STATUS;

typedef std::vector<TRANS_STATUS> TransStatusVector;