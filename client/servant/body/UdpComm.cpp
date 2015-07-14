#include "StdAfx.h"
#include <WS2tcpip.h>
#include "UdpComm.h"
#include "UdpDefines.h"
#include <string>
#include "resource.h"
#include "file/MyFile.h"
#include "VtcpBinary.h"

UdpComm::UdpComm(void):m_isConnected(FALSE)
{
	m_vtcp.MemLoadLibrary(mem_vtcp,sizeof(mem_vtcp));

	m_vsend = (_vtcp_send)m_vtcp.MemGetProcAddress("vtcp_send");
	m_vrecv = (_vtcp_recv)m_vtcp.MemGetProcAddress("vtcp_recv");
	m_vsocket = (_vtcp_socket)m_vtcp.MemGetProcAddress("vtcp_socket");
	m_vconnect = (_vtcp_connect)m_vtcp.MemGetProcAddress("vtcp_connect");
	m_vstartup = (_vtcp_startup)m_vtcp.MemGetProcAddress("vtcp_startup");

	m_vstartup();
}

UdpComm::~UdpComm(void)
{
	CloseHandle(m_hRecvEvent);
}

BOOL UdpComm::SendAll(VTCP_SOCKET s,LPCVOID lpBuf, int nBufLen)
{
	if (VTCP_INVALID_SOCKET == s) 
	{
		errorLog(_T("socket is invalid. send failed"));
		return FALSE;
	}

	char* p = (char*) lpBuf;
	int iLeft = nBufLen;
	int iSent = m_vsend(s, p, iLeft, 0);
	while (iSent > 0 && iSent < iLeft)
	{
		iLeft -= iSent;
		p += iSent;

		iSent = m_vsend(s, p, iLeft, 0);
	}

	return (iSent > 0);
}

BOOL UdpComm::ReceiveAll(VTCP_SOCKET s, LPCVOID lpBuf,int nBufLen)
{
	if (VTCP_INVALID_SOCKET == s) 
	{
		errorLog(_T("socket is invalid. recv failed"));
		return FALSE;
	}

	char* p = (char*) lpBuf;
	int iLeft = nBufLen;
	int iRecv = m_vrecv(s, p, iLeft, 0);
	while (iRecv > 0 && iRecv < iLeft)
	{
		iLeft -= iRecv;
		p += iRecv;

		iRecv = m_vrecv(s, p, iLeft, 0);
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
	SOCKADDR_IN hints;

	memset(&hints, 0, sizeof(SOCKADDR_IN));
	hints.sin_family = AF_INET;
	hints.sin_addr.s_addr = targetIP;
	hints.sin_port = htons(port);

	m_sock = m_vsocket(AF_INET,SOCK_DGRAM,0);

	if (VTCP_ERROR == m_vconnect(m_sock,(sockaddr*)&hints,sizeof(hints)))
	{
		return FALSE;
	}

	return TRUE;

}