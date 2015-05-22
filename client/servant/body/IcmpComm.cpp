#include "stdafx.h"
#include <Winsock2.h>
#include <iphlpapi.h>
#include <icmpapi.h>
//#include "NetStructs.h"
#include "IcmpComm.h"

#pragma comment(lib, "Iphlpapi.lib")

IcmpComm::IcmpComm()
{

}

IcmpComm::~IcmpComm()
{

}

BOOL IcmpComm::Send( ULONG targetIP, const LPBYTE pData, DWORD dwSize )
{
	HANDLE hIcmpFile = ::IcmpCreateFile();
	if (INVALID_HANDLE_VALUE == hIcmpFile)
	{
		errorLogE(_T("create icmp failed"));
		return FALSE;
	}

	ByteBuffer reply;
	reply.Alloc(sizeof(ICMP_ECHO_REPLY) + dwSize);

	DWORD dwRetVal = ::IcmpSendEcho(hIcmpFile, targetIP, pData, (WORD)dwSize, NULL, (LPBYTE)reply, (WORD)reply.Size(), 1000);
	if (0 == dwRetVal)
	{
		DWORD dwLastError = ::GetLastError();
		if (ERROR_INSUFFICIENT_BUFFER != dwLastError)
		{
			errorLog(_T("send icmp failed. E%u"), dwLastError);
			return FALSE;
		}
	}
	/*
	SOCKET sockRaw = ::socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if(INVALID_SOCKET == sockRaw) 
	{
		errorLogE(_T("create socket: %d"), ::WSAGetLastError());
		return FALSE;
	}

	//设置超时时间
	DWORD dwTimeout = 5000;
	setsockopt(sockRaw, SOL_SOCKET, SO_RCVTIMEO, (char*)&dwTimeout, sizeof(dwTimeout));
	setsockopt(sockRaw, SOL_SOCKET, SO_SNDTIMEO, (char*)&dwTimeout, sizeof(dwTimeout));

	//设置目标地址
	SOCKADDR_IN dest = {0};
	dest.sin_addr.s_addr = targetIP;
	dest.sin_family = AF_INET;

	//填充包结构
	ByteBuffer packet;
	packet.Alloc(sizeof(ICMP_HEADER) + dwSize);
	LPICMP_HEADER pIcmpHeader = (LPICMP_HEADER)(LPBYTE)packet;
	LPBYTE pDataBegin = (LPBYTE)packet + sizeof(ICMP_HEADER);

	pIcmpHeader->i_type = ICMP_ECHO;
	pIcmpHeader->i_code = 0;
	pIcmpHeader->i_id = 123;//rctodo
	pIcmpHeader->i_cksum = 0;
	pIcmpHeader->i_seq = 66;//rctodo
	memcpy(pDataBegin, pData, dwSize);
	pIcmpHeader->i_cksum = Checksum((USHORT*)(LPBYTE)packet, packet.Size());

	int iSendto = ::sendto(sockRaw, (LPCSTR)(LPBYTE)packet, packet.Size(), 0, (struct sockaddr*)&dest, sizeof(dest));
	if(SOCKET_ERROR == iSendto)
	{
		errorLog(_T("sendto failed.E%d"), ::WSAGetLastError());
		::closesocket(sockRaw);

		return FALSE;
	}

	if(iSendto != packet.Size()) 
	{
		errorLog(_T("sent %d. expected %d"), iSendto, (int)packet.Size());
		::closesocket(sockRaw);

		return FALSE;
	}

	::closesocket(sockRaw);*/

	return TRUE;
}
/*
USHORT IcmpComm::Checksum(const USHORT* buffer, int size) const
{
	ULONG cksum = 0;
	while(size > 1) 
	{
		cksum += *buffer++;
		size -= sizeof(USHORT);
	}
	if(size > 0) 
	{
		cksum += *(UCHAR*)buffer;
	}

	cksum = (cksum >> 16) + (cksum & 0xffff);
	cksum += (cksum >>16);

	return (USHORT)(~cksum);
}
*/