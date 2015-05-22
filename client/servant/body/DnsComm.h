#pragma once
#include "IComm.h"
#include "MessageDefines.h"
#include "DnsResolver.h"

class DnsComm : public IComm
{
public:
	DnsComm();
	~DnsComm();

	//对IComm接口的实现
	virtual DWORD GetMaxDataSizePerPacket() {return DNS_COMM_REQUEST_MAXSIZE;};
	virtual BOOL Send( ULONG targetIP, const LPBYTE pData, DWORD dwSize );
	virtual BOOL SendAndRecv( ULONG targetIP, const LPBYTE pSendData, DWORD dwSendSize, LPBYTE* pRecvData, DWORD& dwRecvSize );
	virtual COMM_NAME GetName() {return COMMNAME_DNS;};

private:
	CDnsResolver m_dns;
};