#pragma once

class ICMPSocket
{
public:
	ICMPSocket();
	~ICMPSocket();

	BOOL Create(LPCTSTR listenIPOrHost = NULL);
	ULONG GetListenIP() const;
	BOOL RecvICMP(ULONG& srcIP, ULONG& destIP, ByteBuffer& icmpData);
	void Close();

private:
	BOOL CreateListenSocket(ULONG ip, SOCKET& sock) const;

private:
	SOCKET	m_sock;
	ULONG	m_listenIP;
};