#pragma once
#include "socket/MySocket.h"
#include "MessageDefines.h"
#include "IComm.h"
#include "winhttp/http.h"

class HttpComm : public IComm
{
public:
	HttpComm();
	~HttpComm();
	//实现IComm接口
	virtual COMM_NAME GetName() {return COMMNAME_HTTP; };
	virtual DWORD GetMaxDataSizePerPacket() {return HTTP_COMM_REQUEST_MAXSIZE;};
	virtual BOOL Send( ULONG targetIP, const LPBYTE pData, DWORD dwSize );
	virtual BOOL SendAndRecv( ULONG targetIP, const LPBYTE pSendData, DWORD dwSendSize, LPBYTE* pRecvData, DWORD& dwRecvSize );

private:
	BOOL Connect(ULONG targetIP);
private:
	MySocket	m_sock;
	ctx::http*	m_http;
};
