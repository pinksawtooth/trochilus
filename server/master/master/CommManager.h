#pragma once
#include <list>
#include <map>
#include "ICMPSocket.h"
#include "CutupProtocol.h"
#include "CommCallback.h"
#include "CommData.h"
#include "TcpDefines.h"

#ifdef DEBUG
#define MAX_HERAT_BEAT 5
#else
#define MAX_HERAT_BEAT 10
#endif

typedef struct
{
	LPVOID lpParameter1;
	LPVOID lpParameter2;
	LPVOID lpParameter3;
	int nCommName;
}COMMINFO;

typedef std::map<int,COMMINFO> COMM_MAP;

class CommManager
{
	DECLARE_SINGLETON(CommManager);
public:
	MSGSERIALID AddToSendMessage(LPCTSTR clientid, const CommData& commData, BOOL bNeedReply = TRUE);
	BOOL GetReplyMessage(LPCTSTR clientid, MSGSERIALID serialid, CommData& replyData);
	void CleanRequest(LPCTSTR clientid, MSGSERIALID serialid);
	void ListAvailableClient(TStringVector& clientidList, DWORD dwDiffS = MAX_HERAT_BEAT);
	BOOL GetLastConnectionAddr(LPCTSTR clientid, SOCKADDR_IN& addr);
	void RegisterMsgHandler(MSGID msgid, FnMsgHandler fnHandler, LPVOID lpParameter);
	BOOL QuerySendStatus(LPCTSTR clientid, MSGSERIALID serialid, DWORD& dwSentBytes, DWORD& dwTotalBytes);
	BOOL QueryRecvStatus(LPCTSTR clientid, CPSERIAL cpserial, DWORD& dwRecvBytes);

	int AddCommService(int port,int name);
	BOOL DeleteCommService(int serialid);

	BOOL ModifyPacketStatus(CPSERIAL serial,LPCTSTR clientid,BOOL status);
private:
	//消息发送和应答数据结构
	typedef struct  
	{
		CommData	sendData;
		CPSERIAL	cpSerial;
		BOOL		bReply;
		CommData	replyData;
	} SEND_AND_REPLY;
	typedef std::map<MSGSERIALID, SEND_AND_REPLY> DataMap;
	typedef std::map<tstring, DataMap> ClientDataMap;

	//待发送消息数据结构
	typedef std::list<PCP_PACKET> ToSendPacketQueue;
	typedef std::map<CPGUID, ToSendPacketQueue> ToSendPacketMap;

	//心跳数据结构
	typedef struct
	{
		__time64_t	time;
		SOCKADDR_IN	lastAddr;
	} HEARTBEAT_INFO;
	typedef std::map<tstring, HEARTBEAT_INFO> HeartbeatMap;

	//消息处理回调数据结构
	typedef struct  
	{
		FnMsgHandler	fnCallback;
		LPVOID			lpParameter;
	} MSG_HANDLER_INFO;
	typedef std::vector<MSG_HANDLER_INFO> MsgHandlerInfoList;
	typedef std::map<MSGID, MsgHandlerInfoList> MsgHandlerMap;
	
private:
	BOOL HandleMessage(SOCKADDR_IN fromAddr, const LPBYTE pData, DWORD dwDataSize, COMM_NAME commName, CPGUID& cpguid);
	BOOL HandleMessageAndReply(SOCKADDR_IN fromAddr, const LPBYTE pData, DWORD dwDataSize, COMM_NAME commName, BOOL& bValidData, DWORD replyMaxDataSize, ByteBuffer& replyBuffer);

	BOOL GetPacketForClient(const CPGUID& cpguid, PCP_PACKET* ppPacket);

	BOOL SetMessageToAnswer(const CommData& commData);
	void UpdateHeartbeat(LPCTSTR clientid, SOCKADDR_IN addr);

	static BOOL MsgHandler_AvailableComm(MSGID msgid, const CommData& commData, LPVOID lpParameter);

	//消息处理回调
	void HandleMsgByMsgHandler(MSGID msgid, const CommData& commData);

	//HTTP消息处理
	static int HttpMsgHandler(struct mg_connection *conn, enum mg_event ev);
	static void HttpPollThread(LPVOID lpParameter);

	static BOOL TcpMsgHandler(SOCKADDR_IN addr, SOCKET clientSocket, const LPBYTE pData, DWORD dwDataSize, LPBYTE pSessionData, LPVOID lpParameter);

private:
	ICMPSocket	m_icmpSocket;

	CutupProtocol	m_cp;

	CriticalSection	m_mapSection;
	ClientDataMap	m_clientDataMap;
	HeartbeatMap	m_heartbeatMap;
	ToSendPacketMap	m_tosendPacketMap;

	Thread			m_icmpRecvThread;
	BOOL			m_bListenIcmp;

	MsgHandlerMap	m_msgHandlerMap;
	CriticalSection	m_msgHandlerSection;

	COMM_MAP		m_commMap;
};