#pragma once
#include "IComm.h"
#include <Winsock2.h>
#include "memdll/MemLoadDll.h"
#include "MessageDefines.h"

#include "vtcp/vtcp.h"

class UdpComm: public IComm
{
public:
	UdpComm(void);
	~UdpComm(void);

public:
	typedef int (WINAPI *_vtcp_startup)();
	typedef VTCP_SOCKET (WINAPI *_vtcp_socket) (int af, int itype, int protocol);
	typedef int (WINAPI *_vtcp_connect)(VTCP_SOCKET s, const struct sockaddr * sai, socklen_t sailen);
	typedef int (WINAPI *_vtcp_send)(VTCP_SOCKET s, char * buffer, int cb, int flag);
	typedef int (WINAPI *_vtcp_recv)(VTCP_SOCKET s, char * buffer, int cb, int flag);

private:
	_vtcp_startup m_vstartup;
	_vtcp_socket m_vsocket;
	_vtcp_connect m_vconnect;
	_vtcp_send m_vsend;
	_vtcp_recv m_vrecv;

public:
	//实现IComm接口
	virtual COMM_NAME GetName() {return COMMNAME_UDP; };
	virtual DWORD GetMaxDataSizePerPacket() {return UDP_COMM_REQUEST_MAXSIZE;};
	virtual BOOL Send( ULONG targetIP, const LPBYTE pData, DWORD dwSize );
	virtual BOOL SendAndRecv( ULONG targetIP, const LPBYTE pSendData, DWORD dwSendSize, LPBYTE* pRecvData, DWORD& dwRecvSize );

private:
	BOOL Connect(ULONG targetIP,int port);

	BOOL ReceiveAll(VTCP_SOCKET s,LPCVOID lpBuf,int nBufLen);
	BOOL SendAll(VTCP_SOCKET s,LPCVOID lpBuf, int nBufLen);

	VTCP_SOCKET m_sock;

	BOOL m_isConnected;

	CMemLoadDll m_vtcp;

	HANDLE m_hRecvEvent;
};

