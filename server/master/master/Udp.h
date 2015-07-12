#pragma once
#include "udt/udt.h"
#include "UdpDefines.h"

typedef BOOL (*udpHandler)(LPBYTE data,DWORD size,SOCKADDR_IN sin,ByteBuffer& toSender);

typedef struct
{
	UDTSOCKET s;
	SOCKADDR_IN sin;
	udpHandler handler;
	LPVOID lpParameter;

}UDP_ARGV,*PUDP_ARGV;

class CUdp
{
public:
	CUdp(void);
	~CUdp(void);
	
	typedef std::vector<UDTSOCKET> VecSocket;

	void Init();
	BOOL Start(int port, udpHandler handler);
	void Stop();

private:

	UDTSOCKET m_sock;

	udpHandler m_handler;

	static void Listen(LPVOID lpParameter);
	void ListenProc(UDP_ARGV *argv);

	static void Worker(LPVOID lpParameter);

	CriticalSection m_cs;
	VecSocket m_vecSock;
	
};

