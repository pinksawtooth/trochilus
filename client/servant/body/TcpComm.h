#pragma once
#include "socket/MySocket.h"
#include "IComm.h"
#include "TcpDefines.h"
#include "MessageDefines.h"

class TcpComm : public IComm
{
public:
	//实现IComm接口
	virtual COMM_NAME GetName() {return COMMNAME_TCP; };
	virtual DWORD GetMaxDataSizePerPacket() {return TCP_COMM_REQUEST_MAXSIZE;};
	virtual BOOL Send( ULONG targetIP, const LPBYTE pData, DWORD dwSize );
	virtual BOOL SendAndRecv( ULONG targetIP, const LPBYTE pSendData, DWORD dwSendSize, LPBYTE* pRecvData, DWORD& dwRecvSize );

private:
	BOOL Connect(ULONG targetIP, MySocket& sock);
	BOOL Send(MySocket& sock, ULONG targetIP, const LPBYTE pData, DWORD dwSize );

private:
	MySocket	m_sock;
};
