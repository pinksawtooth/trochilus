#include "StdAfx.h"
#include <WS2tcpip.h>
#include "UdpComm.h"
#include "UdpDefines.h"

#ifdef _DEBUG
#pragma comment(lib,"udtd.lib")
#else
#pragma comment(lib,"udt.lib")
#endif

UdpComm::UdpComm(void)
{
	UDT::startup();
}

UdpComm::~UdpComm(void)
{
	CloseHandle(m_hRecvEvent);
}

BOOL UdpComm::Send( ULONG targetIP, const LPBYTE pData, DWORD dwSize )
{
	IN_ADDR addr;
	addr.S_un.S_addr = targetIP;


	ByteBuffer sendByteBuffer;
	sendByteBuffer.Alloc(dwSize);
	memcpy((LPBYTE)sendByteBuffer, pData, dwSize);

	BOOL bSentOK = FALSE;

	int sent = UDT::send(m_sock,(char*)((LPBYTE)sendByteBuffer), sendByteBuffer.Size(),0);
	if ( sent != sendByteBuffer.Size() )
	{
		if ( Connect(targetIP, g_ConfigInfo.nPort))
		{
			UDT::send(m_sock,(char*)((LPBYTE)sendByteBuffer), sendByteBuffer.Size(),0);
			sent = sendByteBuffer.Size();
		}
		else
		{
			debugLog(_T("connect %x %s failed"), targetIP, a2t(inet_ntoa(addr)));
		}
	}

	return sent == sendByteBuffer.Size();
}


BOOL UdpComm::SendAndRecv( ULONG targetIP, const LPBYTE pSendData, DWORD dwSendSize, LPBYTE* pRecvData, DWORD& dwRecvSize )
{
	UDP_HEADER sendHead;
	sendHead.flag = UDP_FLAG;
	sendHead.nSize = dwSendSize;

	if (! Send( targetIP, (PBYTE)&sendHead, sizeof(UDP_HEADER))) return FALSE;

	if (! Send( targetIP, pSendData, dwSendSize)) return FALSE;

	UDP_HEADER recvHead = {0};

	int iRecv = UDT::recv(m_sock,(char*)&recvHead, sizeof(UDP_HEADER),0);

	if (iRecv < 0)
	{
		errorLog(_T("recv udp failed WE%d"), ::WSAGetLastError());
	}


	ByteBuffer buffer;
	buffer.Alloc(recvHead.nSize);

	iRecv = UDT::recv(m_sock,(char*)((LPBYTE)buffer),recvHead.nSize,0);

	if (iRecv < 0)
	{
		errorLog(_T("recv http failed WE%d"), ::WSAGetLastError());
	}

	//¸´ÖÆÊý¾Ý
	*pRecvData = Alloc(recvHead.nSize);
	memcpy(*pRecvData, (LPBYTE)buffer, recvHead.nSize);
	dwRecvSize =  recvHead.nSize;

	return TRUE;
}

BOOL UdpComm::Connect( ULONG targetIP,int port )
{
	struct addrinfo hints, *peer;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	m_sock = UDT::socket(hints.ai_family, hints.ai_socktype, hints.ai_protocol);

	IN_ADDR addr;
	addr.S_un.S_addr = targetIP;
	char* ip = inet_ntoa(addr);

	char szPort[255] = {0};
	sprintf_s(szPort,"%d",port);

	if (0 != getaddrinfo(ip, szPort , &hints, &peer))
	{
		errorLog(_T("incorrect server/peer address. "));
		return FALSE;
	}

	if (UDT::ERROR == UDT::connect(m_sock, peer->ai_addr, peer->ai_addrlen))
	{
		UDT::close(m_sock);
		return FALSE;
	}

	return TRUE;

}