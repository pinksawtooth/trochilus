#include "StdAfx.h"
#include "Tcp.h"


CTcp::CTcp(void)
{
}


CTcp::~CTcp(void)
{
}


void CTcp::Init()
{
	WSADATA data;
	WSAStartup(MAKEWORD(2,2),&data);
}

bool CTcp::Start(int port , tcpHandler handler)
{
	bool ret = FALSE;

	MySocket socket;

	socket.Create();

	if (socket.Bind(port))
	{
		ARGV_LIST* argv = new ARGV_LIST;

		argv->handler = handler;
		argv->s = socket;
		argv->lpParameter = this;

		listen(socket,25);
		_beginthread(ListenProc,0,argv);

		ret = TRUE;
	}

	return ret;


}

void CTcp::Worker(LPVOID lpParameter)
{
	ARGV_LIST *argv = (ARGV_LIST*)lpParameter;

	TCP_HEADER header;

	MySocket socket(argv->s,TRUE);

	BOOL ret = TRUE;
	
	ByteBuffer toSender;

	while(ret)
	{
		ret = socket.ReceiveAll(&header,sizeof(TCP_HEADER));
		if (ret && header.flag == TCP_FLAG)
		{
			LPBYTE lpData = (LPBYTE)malloc(header.nSize);

			ret = socket.ReceiveAll(lpData,header.nSize);

			if ( ret )
			{
				if (argv->handler(lpData,header.nSize,argv->sin,toSender))
				{
					header.nSize = toSender.Size();
					socket.SendAll(&header,sizeof(TCP_HEADER));
					socket.SendAll(toSender,toSender.Size());
				}
				free(lpData);
			}
			else
			{
				free(lpData);
			}
		}
		else
		{
			break;
		}
	}

	delete lpParameter;
}

void CTcp::ListenProc(LPVOID lpParameter)
{
	ARGV_LIST *argv = (ARGV_LIST*)lpParameter;

	MySocket socket(argv->s,TRUE);

	SOCKADDR_IN sin;

	MySocket acc;

	while(socket.Accept(sin,acc))
	{
		ARGV_LIST * client_argv = new ARGV_LIST;

		client_argv->handler = argv->handler;
		client_argv->sin = sin;
		client_argv->s = acc;
		client_argv->lpParameter = argv->lpParameter;

		_beginthread(Worker,0,client_argv);
	}

	delete lpParameter;
}