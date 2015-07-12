#include "StdAfx.h"
#include "Udp.h"
#include <string>

#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#ifdef _DEBUG
#pragma comment(lib,"udtd.lib")
#else
#pragma comment(lib,"udt.lib")
#endif

CUdp::CUdp(void)
{
}


CUdp::~CUdp(void)
{
}


void CUdp::Init()
{
}


BOOL CUdp::Start(int port, udpHandler handler)
{
	bool ret = FALSE;

	addrinfo hints;
	addrinfo* res;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	char szPort[255] = {0};

	sprintf_s(szPort,"%d",port);

	std::string service(szPort);

	do 
	{
		if (0 != getaddrinfo(NULL, service.c_str(), &hints, &res))
			break;

		m_sock = UDT::socket(res->ai_family, res->ai_socktype, res->ai_protocol);

		int mss = UDP_COMM_REPLY_MAXSIZE * 2;
		UDT::setsockopt(m_sock, 0, UDT_MSS, &mss, sizeof(int));

		if (UDT::ERROR == UDT::bind(m_sock, res->ai_addr, res->ai_addrlen))
			break;

		UDP_ARGV* argv = new UDP_ARGV;

		argv->handler = handler;
		argv->s = m_sock;
		argv->lpParameter = this;

		UDT::listen(m_sock,25);
		_beginthread(Listen,0,argv);

		ret = true;

	} while (FALSE);

	return ret;
}

void CUdp::Stop()
{
	UDT::close(m_sock);

	m_cs.Enter();

	VecSocket::iterator it = m_vecSock.begin();

	for (; it != m_vecSock.end(); it++)
	{
		UDT::close(*it);
	}

	m_cs.Leave();

}

void CUdp::Worker(LPVOID lpParameter)
{
	UDP_ARGV *argv = (UDP_ARGV*)lpParameter;

	UDP_HEADER header;

	UDTSOCKET socket = argv->s;

	BOOL ret = TRUE;

	ByteBuffer toSender;

	while(ret)
	{
		int rsize = UDT::recv(socket,(char*)&header,sizeof(UDP_HEADER),0);
		if (ret && header.flag == UDP_FLAG)
		{
			LPBYTE lpData = (LPBYTE)malloc(header.nSize);

			rsize = UDT::recv(socket,(char*)lpData,header.nSize,0);

			if ( rsize == header.nSize )
			{
				if (argv->handler(lpData,header.nSize,argv->sin,toSender))
				{
					header.nSize = toSender.Size();
					UDT::send(socket,(char*)&header,sizeof(UDP_HEADER),0);
					UDT::send(socket,(char*)((LPBYTE)toSender),toSender.Size(),0);
				}

			}
			free(lpData);
		}
		else
		{
			break;
		}
	}

	UDT::close(socket);

	delete lpParameter;
}

void CUdp::ListenProc( UDP_ARGV *argv )
{
	SOCKADDR_IN sin;
	int addrlen = sizeof(sin);

	UDTSOCKET fhandle;

	while (true)
	{
		if (UDT::INVALID_SOCK == (fhandle = UDT::accept(m_sock, (sockaddr *)&sin, &addrlen)))
			break;

		m_cs.Enter();
		m_vecSock.push_back(fhandle);
		m_cs.Leave();

		UDP_ARGV * client_argv = new UDP_ARGV;

		client_argv->handler = argv->handler;
		client_argv->sin = sin;
		client_argv->s = fhandle;
		client_argv->lpParameter = argv->lpParameter;

		_beginthread(Worker,0,client_argv);

	}

	UDT::close(m_sock);

	delete argv;
}

void CUdp::Listen(LPVOID lpParameter)
{
	UDP_ARGV *argv = (UDP_ARGV*)lpParameter;
	CUdp* udp = (CUdp*)argv->lpParameter;

	return udp->ListenProc((UDP_ARGV*)lpParameter);
}