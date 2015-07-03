#include "stdafx.h"
#include <Winsock2.h>
#include "file/MyFile.h"
#include "socket/MySocket.h"
#include "BinNames.h"
#include "MessageDefines.h"
#include "common.h"
#include "CommManager.h"
#include "Tcp.h"
#include "DnsResolver.h"
#include "mongoose/mongoose.h"

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


void CommManager::HttpPollThread(LPVOID lpParameter)
{
	mg_server* server  = (mg_server*)lpParameter;
	for (;;) mg_poll_server(server, 1000);

};

int CommManager::AddCommService(int port,int name)
{
	COMM_MAP::iterator it;
	int ret = 0;
	int serial = 0;

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
			struct mg_server *server;

			// Create and configure the server
			server = mg_create_server(NULL, HttpMsgHandler);

			char szPort[255];

			memset(szPort,0,255);

			itoa(port,szPort,10);

			if( mg_set_option(server, "listening_port", szPort) != NULL )
			{
				mg_destroy_server(&server);
				break;
			}

			DWORD tid = _beginthread(HttpPollThread,0,server);
			
			info.lpParameter1 = server;
			info.lpParameter2 = (LPVOID)tid;
			info.nCommName = COMMNAME_HTTP;

			m_commMap.insert(MAKE_PAIR(COMM_MAP,serial,info));

			ret = serial;

			break;
		}
// 	case COMMNAME_DNS:
// 		{
// 			UdpServer* DnsServer = new UdpServer;
// 			DnsServer->Init(UdpMsgHandler,this);
// 
// 			if (! DnsServer->Start(port))
// 			{
// 				delete DnsServer;
// 				return 0;
// 			}
// 
// 			info.lpParameter = DnsServer;
// 			info.nCommName = COMMNAME_DNS;
// 
// 			m_commMap.insert(MAKE_PAIR(COMM_MAP,serial,info));
// 
// 			break;
// 		}
	case COMMNAME_TCP:
		{
			CTcp *tcp = new CTcp;
			tcp->Init();
			if (!tcp->Start(port,TcpMsgHandler))
			{
				delete tcp;
				break;
			}

			info.nCommName = COMMNAME_TCP;
			info.lpParameter1 = tcp;

			m_commMap.insert(MAKE_PAIR(COMM_MAP,serial,info));

			ret = serial;

			break;
		}
	default:
		return ret;
	}

	return ret;
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
				TerminateThread(info.lpParameter2,0);
				mg_destroy_server((mg_server**)&info.lpParameter1);

				m_commMap.erase(it);

				break;
			}
		case COMMNAME_TCP:
			{
				CTcp *tcp = (CTcp *)info.lpParameter1;
				tcp->Stop();
				delete tcp;

				m_commMap.erase(it);

				break;
			}
		default:
			bRet = FALSE;
			break;
		}
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
BOOL CommManager::TcpMsgHandler( LPBYTE data,DWORD size,SOCKADDR_IN sin,ByteBuffer& toSender )
{
	BOOL bValidData = FALSE;

	return CommManager::GetInstanceRef().HandleMessageAndReply(sin,data , size, COMMNAME_TCP, bValidData, TCP_COMM_REPLY_MAXSIZE, toSender);
}
int CommManager::HttpMsgHandler( struct mg_connection *conn, enum mg_event ev )
{
	BOOL bValidData = FALSE;
	BOOL bNeedReply = FALSE;
	BOOL bHasRecv = FALSE;

	ByteBuffer buffer;
	int nOutSize = 0;
	LPBYTE pOutBuf = NULL;

	ByteBuffer toSendBuffer;
	SOCKADDR_IN addr = {0};
	char szLength[255] = {0};

	switch (ev) 
	{
	case MG_AUTH: return MG_TRUE;

	case MG_REQUEST:

		bNeedReply = CommManager::GetInstanceRef().HandleMessageAndReply(addr,(LPBYTE)conn->content , conn->content_len, COMMNAME_HTTP, bValidData, HTTP_COMM_REPLY_MAXSIZE, toSendBuffer);

		sprintf_s(szLength,"%d",toSendBuffer.Size());

		mg_send_header(conn,
			"Content-Length",
			szLength);

		if (bNeedReply)
		{
			mg_send_data(conn,toSendBuffer,toSendBuffer.Size());
		}
		return MG_TRUE;

	default: return MG_FALSE;
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
