#pragma once
#include "TcpDefines.h"
#include "socket/MySocket.h"
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")

typedef BOOL (*tcpHandler)(LPBYTE data,DWORD size,SOCKADDR_IN sin,ByteBuffer& toSender);

typedef struct
{
	SOCKET s;
	SOCKADDR_IN sin;
	tcpHandler handler;
	LPVOID lpParameter;

}ARGV_LIST,*PARGV_LIST;

class CTcp
{
public:
	CTcp(void);
	~CTcp(void);

	void Init();
	bool Start(int port , tcpHandler handler);
	bool Stop();

private:

	static void ListenProc(LPVOID lpParameter);

	static void Worker(LPVOID lpParameter);

};

