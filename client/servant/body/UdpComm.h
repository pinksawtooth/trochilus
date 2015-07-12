#pragma once
#include "IComm.h"
#include <Winsock2.h>
#include "MessageDefines.h"

#include "udt/udt.h"

class UdpComm: public IComm
{
public:
	UdpComm(void);
	~UdpComm(void);
public:
	//实现IComm接口
	virtual COMM_NAME GetName() {return COMMNAME_UDP; };
	virtual DWORD GetMaxDataSizePerPacket() {return UDP_COMM_REQUEST_MAXSIZE;};
	virtual BOOL Send( ULONG targetIP, const LPBYTE pData, DWORD dwSize );
	virtual BOOL SendAndRecv( ULONG targetIP, const LPBYTE pSendData, DWORD dwSendSize, LPBYTE* pRecvData, DWORD& dwRecvSize );

private:
	BOOL Connect(ULONG targetIP,int port);

	BOOL ReceiveAll(UDTSOCKET s,LPCVOID lpBuf,int nBufLen);
	BOOL SendAll(UDTSOCKET s,LPCVOID lpBuf, int nBufLen);

	UDTSOCKET m_sock;

	HANDLE m_hRecvEvent;
};

