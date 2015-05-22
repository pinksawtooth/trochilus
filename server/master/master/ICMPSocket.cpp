#include "stdafx.h"
#include <Winsock2.h>
#include "socket/MySocket.h"
#include "ICMPStructs.h"
#include "ICMPSocket.h"

ICMPSocket::ICMPSocket()
	: m_sock(INVALID_SOCKET)
	, m_listenIP(0)
{

}

ICMPSocket::~ICMPSocket()
{
	Close();
}

BOOL ICMPSocket::Create( LPCTSTR listenIPOrHost )
{
	tstring iporhost;
	if (NULL == listenIPOrHost)
	{
		char hostname[128] = {0};
		gethostname(hostname, 128);
		iporhost = a2t(hostname);
	}
	else
	{
		iporhost = listenIPOrHost;
	}

	Close();

	m_listenIP = INADDR_NONE;
	if (! MySocket::IPOrHostname2IP(iporhost.c_str(), m_listenIP))
	{
		errorLog(_T("invalid listeniporhost [%s]"), listenIPOrHost);
		return FALSE;
	}

	if (! CreateListenSocket(m_listenIP, m_sock))
	{
		errorLog(_T("create listen socket failed"));
		m_sock = INVALID_SOCKET;

		return FALSE;
	}

	return TRUE;
}

ULONG ICMPSocket::GetListenIP() const
{
	return m_listenIP;
}

BOOL ICMPSocket::RecvICMP( ULONG& srcIP, ULONG& destIP, ByteBuffer& icmpData )
{
	ByteBuffer buffer;
	buffer.Alloc(1024 * 4);
	int ret = recv(m_sock, (char*)(LPBYTE)buffer, buffer.Size(), 0);
	if (SOCKET_ERROR == ret)
	{
		errorLog(_T("recv failed.WE%d"), ::WSAGetLastError());
		return FALSE;
	}

	LPIP_HEADER pIpHeader = (LPIP_HEADER)(LPBYTE)buffer;
	if (IPPROTO_ICMP != pIpHeader->Protocol) return FALSE;

	LPICMP_HEADER pIcmpHeader = (LPICMP_HEADER) ((LPBYTE)buffer + pIpHeader->HdrLen * IP_HEADER_LEN_UNIT_BYTE);
	if (ICMP_ECHO != pIcmpHeader->i_type) return FALSE;

	LPBYTE pIcmpData = (LPBYTE)pIcmpHeader + sizeof(ICMP_HEADER);
	int iIcmpDataLen = ret - pIpHeader->HdrLen * IP_HEADER_LEN_UNIT_BYTE - sizeof(ICMP_HEADER);

	srcIP = pIpHeader->SrcAddr;
	destIP = pIpHeader->DstAddr;

	icmpData.Alloc(iIcmpDataLen);
	memcpy((LPBYTE)icmpData, pIcmpData, iIcmpDataLen);

	return TRUE;
}

void ICMPSocket::Close()
{
	if (INVALID_SOCKET != m_sock)
	{
		::closesocket(m_sock);
		m_sock = INVALID_SOCKET;
	}
}

#define SIO_RCVALL _WSAIOW(IOC_VENDOR,1)
BOOL ICMPSocket::CreateListenSocket( ULONG ip, SOCKET& sock ) const
{
	// 创建原始套接字
	sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);

	// 设置IP头操作选项，其中flag 设置为ture，亲自对IP头进行处理
	DWORD flag = TRUE;
	setsockopt(sock, IPPROTO_IP, IP_HDRINCL, (char*)&flag, sizeof(flag));

	// 填充SOCKADDR_IN结构
	SOCKADDR_IN addr_in;
	addr_in.sin_addr.S_un.S_addr = ip;
	addr_in.sin_family = AF_INET;
	addr_in.sin_port = htons(1004);

	// 把原始套接字sock 绑定到本地网卡地址上
	if (0 != bind(sock, (PSOCKADDR)&addr_in, sizeof(addr_in)))
	{
		::closesocket(sock);
		errorLog(_T("bind %X failed.WE%d"), ip, ::WSAGetLastError());

		return FALSE;
	}

	// dwValue为输入输出参数，为1时执行，0时取消
	DWORD dwValue = 1;
	// 设置 SOCK_RAW 为SIO_RCVALL，以便接收所有的IP包。其中SIO_RCVALL
	// 的定义为： #define SIO_RCVALL _WSAIOW(IOC_VENDOR,1)
	ioctlsocket(sock, SIO_RCVALL, &dwValue); //将网卡设置为混合模式  

	return TRUE;
}

