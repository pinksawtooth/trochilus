#pragma once
#include "IComm.h"
#include "MessageDefines.h"

class IcmpComm : public IComm
{
public:
	IcmpComm();
	~IcmpComm();

	//对IComm接口的实现
	virtual DWORD GetMaxDataSizePerPacket() {return ICMP_COMM_REQUEST_MAXSIZE;};
	virtual BOOL Send( ULONG targetIP, const LPBYTE pData, DWORD dwSize );
	virtual COMM_NAME GetName() {return COMMNAME_ICMP;};
	virtual BOOL CanRecv() const {return FALSE;};
};