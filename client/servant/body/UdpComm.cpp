#include "StdAfx.h"
#include <WS2tcpip.h>
#include "UdpComm.h"
#include "UdpDefines.h"
#include <string>


#ifdef _DEBUG
#pragma comment(lib,"udtd.lib")
#else
#pragma comment(lib,"udt.lib")
#endif

UdpComm::UdpComm(void):m_isConnected(FALSE)
{
	UDT::startup();
}

UdpComm::~UdpComm(void)
{
	CloseHandle(m_hRecvEvent);
}

BOOL UdpComm::SendAll(UDTSOCKET s,LPCVOID lpBuf, int nBufLen)
{
	if (UDT::INVALID_SOCK == s) 
	{
		errorLog(_T("socket is invalid. send failed"));
		return FALSE;
	}

	const char* p = (const char*) lpBuf;
	int iLeft = nBufLen;
	int iSent = UDT::send(s, p, iLeft, 0);
	while (iSent > 0 && iSent < iLeft)
	{
		iLeft -= iSent;
		p += iSent;

		iSent = UDT::send(s, p, iLeft, 0);
	}

	return (iSent > 0);
}

BOOL UdpComm::ReceiveAll(UDTSOCKET s, LPCVOID lpBuf,int nBufLen)
{
	if (UDT::INVALID_SOCK == s) 
	{
		errorLog(_T("socket is invalid. recv failed"));
		return FALSE;
	}

	char* p = (char*) lpBuf;
	int iLeft = nBufLen;
	int iRecv = UDT::recv(s, p, iLeft, 0);
	while (iRecv > 0 && iRecv < iLeft)
	{
		iLeft -= iRecv;
		p += iRecv;

		iRecv = UDT::recv(s, p, iLeft, 0);
	}

	return (iRecv > 0);
}

BOOL UdpComm::Send( ULONG targetIP, const LPBYTE pData, DWORD dwSize )
{
	IN_ADDR addr;
	addr.S_un.S_addr = targetIP;

	ByteBuffer sendByteBuffer;
	sendByteBuffer.Alloc(dwSize);
	memcpy((LPBYTE)sendByteBuffer, pData, dwSize);

	if ( !m_isConnected )
		m_isConnected = Connect(targetIP, g_ConfigInfo.nPort);

	if ( m_isConnected )
		m_isConnected = SendAll(m_sock,(LPBYTE)sendByteBuffer,sendByteBuffer.Size());

	return m_isConnected;
}


BOOL UdpComm::SendAndRecv( ULONG targetIP, const LPBYTE pSendData, DWORD dwSendSize, LPBYTE* pRecvData, DWORD& dwRecvSize )
{
	UDP_HEADER sendHead;
	sendHead.flag = UDP_FLAG;
	sendHead.nSize = dwSendSize;

	BOOL ret = FALSE;

	do 
	{
		if (! Send( targetIP, (PBYTE)&sendHead, sizeof(UDP_HEADER))) break;

		if (! Send( targetIP, pSendData, dwSendSize)) break;

		UDP_HEADER recvHead = {0};

		if ( !ReceiveAll(m_sock,(char*)&recvHead, sizeof(UDP_HEADER)))
		{
			m_isConnected = FALSE;
			break;
		}


		ByteBuffer buffer;
		buffer.Alloc(recvHead.nSize);

		if (! ReceiveAll(m_sock,(LPBYTE)buffer,recvHead.nSize))
		{
			m_isConnected = FALSE;
			errorLog(_T("recv udp failed WE%d"), ::WSAGetLastError());
			break;
		}

		//¸´ÖÆÊý¾Ý
		*pRecvData = Alloc(recvHead.nSize);
		memcpy(*pRecvData, (LPBYTE)buffer, recvHead.nSize);
		dwRecvSize =  recvHead.nSize;

		ret = TRUE;

	} while (FALSE);



	return ret;
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
	LPCSTR ip = std::string(inet_ntoa(addr)).c_str();

	char szPort[255] = {0};
	sprintf_s(szPort,"%d",port);

	if (0 != getaddrinfo(ip, szPort , &hints, &peer))
	{
		errorLog(_T("incorrect server/peer address. "));
		return FALSE;
	}

	if (UDT::ERROR == UDT::connect(m_sock, peer->ai_addr, peer->ai_addrlen))
	{
		return FALSE;
	}

	return TRUE;

}