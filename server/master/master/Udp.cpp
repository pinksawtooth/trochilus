#include "StdAfx.h"
#include "Udp.h"
#include <string>

#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib,"vtcp.lib")

CUdp::CUdp(void)
{
	vtcp_startup();
}


CUdp::~CUdp(void)
{
}


BOOL SendAll(VTCP_SOCKET s,LPCVOID lpBuf, int nBufLen)
{
	if (VTCP_INVALID_SOCKET == s) 
	{
		errorLog(_T("socket is invalid. send failed"));
		return FALSE;
	}

	char* p = (char*) lpBuf;
	int iLeft = nBufLen;
	int iSent = vtcp_send(s, p, iLeft, 0);
	while (iSent > 0 && iSent < iLeft)
	{
		iLeft -= iSent;
		p += iSent;

		iSent = vtcp_send(s, p, iLeft, 0);
	}

	return (iSent > 0);
}

BOOL ReceiveAll(VTCP_SOCKET s, LPCVOID lpBuf,int nBufLen)
{
	if (VTCP_INVALID_SOCKET == s) 
	{
		errorLog(_T("socket is invalid. recv failed"));
		return FALSE;
	}

	char* p = (char*) lpBuf;
	int iLeft = nBufLen;
	int iRecv = vtcp_recv(s, p, iLeft, 0);
	while (iRecv > 0 && iRecv < iLeft)
	{
		iLeft -= iRecv;
		p += iRecv;

		iRecv = vtcp_recv(s, p, iLeft, 0);
	}

	return (iRecv > 0);
}

void CUdp::Init()
{
}


BOOL CUdp::Start(int port, udpHandler handler)
{
	bool ret = FALSE;

	SOCKADDR_IN hints;

	memset(&hints, 0, sizeof(SOCKADDR_IN));
	hints.sin_family = AF_INET;
	hints.sin_port = htons(port);

	char szPort[255] = {0};

	sprintf_s(szPort,"%d",port);

	std::string service(szPort);

	do 
	{
		m_sock = vtcp_socket(AF_INET,SOCK_DGRAM,0);

		if (VTCP_ERROR == vtcp_bind(m_sock, (sockaddr*)&hints, sizeof(hints)))
			break;

		UDP_ARGV* argv = new UDP_ARGV;

		argv->handler = handler;
		argv->s = m_sock;
		argv->lpParameter = this;

		vtcp_listen(m_sock,25);
		_beginthread(Listen,0,argv);

		ret = true;

	} while (FALSE);

	return ret;
}

void CUdp::Stop()
{
	vtcp_close(m_sock);

	m_cs.Enter();

	VecSocket::iterator it = m_vecSock.begin();

	for (; it != m_vecSock.end(); it++)
	{
		vtcp_close(*it);
	}

	m_cs.Leave();

}

void CUdp::Worker(LPVOID lpParameter)
{
	UDP_ARGV *argv = (UDP_ARGV*)lpParameter;

	UDP_HEADER header;

	VTCP_SOCKET socket = argv->s;

	BOOL ret = TRUE;

	ByteBuffer toSender;

	while(ret)
	{
		int ret = ReceiveAll(socket,(char*)&header,sizeof(UDP_HEADER));
		if (ret && header.flag == UDP_FLAG)
		{
			LPBYTE lpData = (LPBYTE)malloc(header.nSize);

			ret = ReceiveAll(socket,(char*)lpData,header.nSize);

			if ( ret )
			{
				if (argv->handler(lpData,header.nSize,argv->sin,toSender))
				{
					header.nSize = toSender.Size();
					SendAll(socket,(char*)&header,sizeof(UDP_HEADER));
					SendAll(socket,(char*)((LPBYTE)toSender),toSender.Size());
				}

			}
			free(lpData);
		}
		else
		{
			break;
		}
	}

	vtcp_close(socket);

	delete lpParameter;
}

void CUdp::ListenProc( UDP_ARGV *argv )
{
	SOCKADDR_IN sin;
	int addrlen = sizeof(sin);

	VTCP_SOCKET fhandle;

	while (true)
	{
		if (VTCP_INVALID_SOCKET == (fhandle = vtcp_accept(m_sock, (sockaddr *)&sin, &addrlen)))
			break;

// 		m_cs.Enter();
// 		m_vecSock.push_back(fhandle);
// 		m_cs.Leave();

		UDP_ARGV * client_argv = new UDP_ARGV;

		client_argv->handler = argv->handler;
		client_argv->sin = sin;
		client_argv->s = fhandle;
		client_argv->lpParameter = argv->lpParameter;

		_beginthread(Worker,0,client_argv);

	}

	vtcp_close(m_sock);

	delete argv;
}

void CUdp::Listen(LPVOID lpParameter)
{
	UDP_ARGV *argv = (UDP_ARGV*)lpParameter;
	CUdp* udp = (CUdp*)argv->lpParameter;

	return udp->ListenProc((UDP_ARGV*)lpParameter);
}