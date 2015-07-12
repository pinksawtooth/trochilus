#include "stdafx.h"
#include "socket/MySocket.h"
#include "TcpComm.h"

BOOL TcpComm::Send( ULONG targetIP, const LPBYTE pData, DWORD dwSize )
{
	return Send(m_sock, targetIP, pData, dwSize);
}

BOOL TcpComm::SendAndRecv( ULONG targetIP, const LPBYTE pSendData, DWORD dwSendSize, LPBYTE* pRecvData, DWORD& dwRecvSize )
{
	TCP_HEADER sendHead;
	sendHead.flag = TCP_FLAG;
	sendHead.nSize = dwSendSize;

	if (! Send(m_sock, targetIP, (PBYTE)&sendHead, sizeof(TCP_HEADER))) return FALSE;

	if (! Send(m_sock, targetIP, pSendData, dwSendSize)) return FALSE;

	TCP_HEADER recvHead = {0};

	int iRecv = m_sock.ReceiveAll((LPBYTE)&recvHead, sizeof(TCP_HEADER));
	
	if (iRecv < 0)
	{
		errorLog(_T("recv http failed WE%d"), ::WSAGetLastError());
	}


	ByteBuffer buffer;
	buffer.Alloc(recvHead.nSize);

	iRecv = m_sock.ReceiveAll(buffer,recvHead.nSize);

	if (iRecv < 0)
	{
		errorLog(_T("recv tcp failed WE%d"), ::WSAGetLastError());
	}

	//¸´ÖÆÊý¾Ý
	*pRecvData = Alloc(recvHead.nSize);
	memcpy(*pRecvData, (LPBYTE)buffer, recvHead.nSize);
	dwRecvSize =  recvHead.nSize;
	
	return TRUE;
}

BOOL TcpComm::Connect( ULONG targetIP, MySocket& sock )
{
	sock.Close();
	if (! sock.Create(TRUE))
	{
		errorLogE(_T("create socket failed."));
		return FALSE;
	}

	if (! sock.Connect(targetIP, g_ConfigInfo.nPort, 10))
	{
		errorLog(_T("connect [%u] failed"), targetIP);
		return FALSE;
	}

	int value = 65535;
	if (0 != setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&value, sizeof(value)))
	{
		errorLog(_T("setsockopt rcvbuf failed.WE%d"), ::WSAGetLastError());
	}
	value = 65535;
	if (0 != setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&value, sizeof(value)))
	{
		errorLog(_T("setsockopt sndbuf failed.WE%d"), ::WSAGetLastError());
	}

	return TRUE;
}

BOOL TcpComm::Send( MySocket& sock, ULONG targetIP, const LPBYTE pData, DWORD dwSize )
{
	IN_ADDR addr;
	addr.S_un.S_addr = targetIP;


	ByteBuffer sendByteBuffer;
	sendByteBuffer.Alloc(dwSize);
	memcpy((LPBYTE)sendByteBuffer, pData, dwSize);

	BOOL bSentOK = FALSE;

	bSentOK = sock.SendAll((LPBYTE)sendByteBuffer, sendByteBuffer.Size());
	if (!bSentOK)
	{
		sock.Close();

		if ( Connect(targetIP, sock))
		{
			bSentOK = sock.SendAll((LPBYTE)sendByteBuffer, sendByteBuffer.Size());
		}
		else
		{
			debugLog(_T("connect %x %s failed"), targetIP, a2t(inet_ntoa(addr)));
		}
	}

	return bSentOK;
}