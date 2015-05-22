#pragma once
#include <list>
#include <map>
#include "tstring.h"
#include "CutupProtocolStructs.h"
#include "CommNames.h"

class CutupProtocol
{
public:
	CutupProtocol();
	~CutupProtocol();

	BOOL Init();
	void Deinit();

	static void SetLocalGuid(const CPGUID& guid);

	//CPGuid相关函数
	static BOOL CreateCPGuid(CPGUID& cpguid);
	static void CPGuid2Str(const CPGUID& cpguid, tstring& str);
	static BOOL Str2CPGuid(LPCTSTR str, CPGUID& cpguid);

	//发送消息相关函数
	BOOL PutMessage(const CPGUID& to, const LPBYTE pData, DWORD dwSize, COMM_NAME expectedComm, DWORD dwDefaultPacketMaxSize = 0, CPSERIAL* pSerial = NULL, UINT64 uint64Flag = 0);
	BOOL GetMessageToSendById(const CPGUID& to, DWORD dwMaxSize, ByteBuffer& byteData, COMM_NAME* pExpectedComm = NULL);
	BOOL GetMessageToSend(DWORD dwMaxSize, ByteBuffer& byteData, CPGUID* pTo, COMM_NAME* pExpectedComm = NULL);
	BOOL QuerySendStatus(const CPGUID& to, CPSERIAL serial, DWORD& dwSentBytes, DWORD& dwTotalBytes);
	void CleanMessageByFlag(UINT64 uint64Flag);

	//接收消息相关函数
	BOOL AddRecvPacket(const LPBYTE pData, DWORD dwSize, COMM_NAME fromComm, CPGUID* pCpguid = NULL);
	BOOL HasReceivedMsg();
	BOOL RecvMsg(ByteBuffer& data, CPGUID& from);
	BOOL QueryRecvStatus(const CPGUID& from, CPSERIAL serial, DWORD& dwRecvBytes);

	//构造空消息
	void CreateEmptyPacket(ByteBuffer& emptyData) const;
	//控制某个消息是否继续发送
	BOOL ModifyPacketStatus(const CPGUID& to, CPSERIAL serial,BOOL isStop);
private:
	//接收消息的数据结构
	typedef std::list<PCP_PACKET> PacketList;
	typedef struct  
	{
		PacketList	packetList;
		__time64_t	begintime;
		DWORD		dwDataSize;
		DWORD		dwExpectedIndex;
		int			iErrorCountPerPacket;
		COMM_NAME	lastCommname;
	} RECV_MSG;
	typedef std::map<CPSERIAL, RECV_MSG> SerialMsgMap;
	typedef std::map<CPGUID, SerialMsgMap> GuidRecvMap;

	typedef struct  
	{
		ByteBuffer	byteData;
		CPGUID		fromCPGuid;
	} RECV_BYTE_MSG;
	typedef std::list<RECV_BYTE_MSG> RecvQueue;

	//待发送的消息
	typedef struct  
	{
		CPGUID		to;
		DWORD		dwDefaultPacketMaxSize;
		COMM_NAME	expectedCommName;
		CPSERIAL	serial;
		ByteBuffer	byteData;
		BOOL		bData;

		union
		{
			struct  
			{
				DWORD		dwNextSendPos;
				DWORD		dwNextIndex;
			} dataParams;
			struct  
			{
				BYTE		cmd;
				DWORD		index;
			} packetParams;
		} params;

		BOOL		bAllSent;
		BOOL		bIsStop;
		__time64_t	finishTime;

		UINT64		uint64Flag;
		__time64_t	lastGetDataTime;
	} SEND_MSG;
	typedef std::list<SEND_MSG> SendQueue;

private:
	PCP_PACKET CreatePacket(CPSERIAL serial, DWORD index, BYTE cmd, const LPBYTE pData, DWORD dwDataSize) const;
	void MergePackets(const PacketList& packetList, ByteBuffer& byteData) const;
	void FreePackets(PCP_PACKET* ppPackets, DWORD dwPacketCount) const;
	void FreePacket(PCP_PACKET pPacket) const;
	void FreePackets(PacketList& packetList) const;
	void AddCmdPacket(const CPGUID& to, COMM_NAME commName, CPSERIAL serial, BYTE cmd, DWORD index, const LPBYTE pData, DWORD dwDataSize, BOOL bFront);

	BOOL HandleDataPacket(const PCP_PACKET pPacket, COMM_NAME fromComm);
	void AbortSending(const CPGUID& to, CPSERIAL serial);
	BOOL ResendMsg(const CPGUID& to, CPSERIAL serial, DWORD dwExpectedIndex, DWORD dwReceivedDataSize);

	void GetDataFromSendMsg(SEND_MSG& sendMsg, DWORD dwMaxSize, ByteBuffer& byteData, COMM_NAME* pExpectedComm = NULL);
	void CleanAllSentMessage(DWORD dwKeepTimeS = 30);

private:
	volatile CPSERIAL	m_serial;
	static CPGUID	m_slocalGuid;

	GuidRecvMap		m_guidRecvMap;
	RecvQueue		m_recvQueue;
	BOOL			m_bWorking;
	CriticalSection	m_recvQueueSection;
	Handle			m_hRecvSemaphore;

	SendQueue		m_sendQueue;
	CriticalSection	m_sendQueueSection;
};
