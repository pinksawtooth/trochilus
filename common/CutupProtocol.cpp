#include "stdafx.h"
#include "time.h"
#include <atlenc.h>
#include "CutupProtocol.h"

struct CPGUID CutupProtocol::m_slocalGuid = {0};

CutupProtocol::CutupProtocol()
{

}

CutupProtocol::~CutupProtocol()
{

}

BOOL CutupProtocol::Init()
{
	__time64_t now;
	_time64(&now);
	m_serial = (CPSERIAL) now;

	m_bWorking = TRUE;
	m_hRecvSemaphore = ::CreateSemaphore(NULL, 0, 5000, NULL);
	if (! m_hRecvSemaphore.IsValid())
	{
		return FALSE;
	}
		
	return TRUE;
}

void CutupProtocol::Deinit()
{
	m_bWorking = FALSE;
	::ReleaseSemaphore(m_hRecvSemaphore, 1, NULL);
}

void CutupProtocol::SetLocalGuid( const CPGUID& guid )
{
	m_slocalGuid = guid;
}

BOOL CutupProtocol::CreateCPGuid( CPGUID& cpguid )
{
	BOOL bRet = FALSE;
	GUID guid;
	CoInitialize(NULL);
	{
		if (S_OK == ::CoCreateGuid(&guid))
		{
			bRet = TRUE;
		}
	}
	CoUninitialize();

	if (bRet)
	{
		cpguid = guid;
	}

	return bRet;
}

void CutupProtocol::CPGuid2Str( const CPGUID& cpguid, tstring& str )
{
	tostringstream toss;

	LPBYTE p = (LPBYTE) &cpguid;
	for (int i = 0; i < (int)sizeof(CPGUID); i++, p++)
	{
		TCHAR buffer[8] = {0};
		_stprintf_s(buffer, _T("%02X"), *p);
		toss << buffer;
	}

	str = toss.str();
}

BOOL CutupProtocol::Str2CPGuid( LPCTSTR str, CPGUID& cpguid )
{
	tstring s = str;
	if (s.size() != 32) return FALSE;

	LPBYTE p = (LPBYTE) &cpguid;
	makeLower(s);
	for (int i = 0; i < 32; i+=2, p++)
	{
		BYTE b = 0;

		TCHAR ch = s[i];
		if (ch >= '0' && ch <= '9') b = ch - '0';
		else if (ch >= 'a' && ch <= 'f') b = ch - 'a' + 10;
		b <<= 4;

		ch = s[i + 1];
		if (ch >= '0' && ch <= '9') b |= ch - '0';
		else if (ch >= 'a' && ch <= 'f') b |= ch - 'a' + 10;

		*p = b;
	}

	return TRUE;
}

BOOL CutupProtocol::PutMessage( const CPGUID& to, const LPBYTE pData, DWORD dwSize, COMM_NAME expectedComm, DWORD dwDefaultPacketMaxSize /*= 0*/, CPSERIAL* pSerial /*= NULL*/, UINT64 uint64Flag /*= 0*/ )
{
	if (0 == dwSize || NULL == pData) return FALSE;

	m_sendQueueSection.Enter();
	{
		SEND_MSG sendMsg;
		sendMsg.to = to;
		sendMsg.dwDefaultPacketMaxSize = dwDefaultPacketMaxSize;
		sendMsg.expectedCommName = expectedComm;
		sendMsg.serial = ::InterlockedIncrement(&m_serial);
		sendMsg.bData = TRUE;
		sendMsg.params.dataParams.dwNextIndex = 0;
		sendMsg.params.dataParams.dwNextSendPos = 0;
		sendMsg.byteData.Alloc(dwSize);
		memcpy(sendMsg.byteData, pData, dwSize);
		sendMsg.finishTime = 0;
		sendMsg.bAllSent = FALSE;
		sendMsg.bIsStop = FALSE;
		sendMsg.uint64Flag = uint64Flag;
		sendMsg.lastGetDataTime = 0;

		if (NULL != pSerial) *pSerial = sendMsg.serial;

		m_sendQueue.push_back(sendMsg);
	}
	m_sendQueueSection.Leave();

	return TRUE;
}

BOOL CutupProtocol::GetMessageToSendById( const CPGUID& to, DWORD dwMaxSize, ByteBuffer& byteData, COMM_NAME* pExpectedComm /*= NULL*/ )
{
	if (dwMaxSize <= CPHEADER_SIZE) return FALSE;

	CleanAllSentMessage();

	BOOL bFound = FALSE;
	m_sendQueueSection.Enter();
	{
		SEND_MSG* pMsg = NULL;
		__time64_t theMin = MAXLONGLONG;

		//查找给to的消息或消息队列头的消息
		SendQueue::iterator iter = m_sendQueue.begin();
		for (; iter != m_sendQueue.end(); iter++)
		{
			if (iter->to == to && !iter->bAllSent)
			{
				if (iter->lastGetDataTime < theMin)
				{
					theMin = iter->lastGetDataTime;
					pMsg = &(*iter);
				}
			}
		}
		
		//如果有符合条件待发送的消息
		if (NULL != pMsg)
		{
			GetDataFromSendMsg(*pMsg, dwMaxSize, byteData, pExpectedComm);
			bFound = TRUE;
		}
	}
	m_sendQueueSection.Leave();

	return bFound;
}

BOOL CutupProtocol::GetMessageToSend( DWORD dwMaxSize, ByteBuffer& byteData, CPGUID* pTo, COMM_NAME* pExpectedComm /*= NULL*/ )
{
	if (dwMaxSize != 0 && dwMaxSize <= CPHEADER_SIZE) return FALSE;

	CleanAllSentMessage();

	BOOL bFound = FALSE;
	m_sendQueueSection.Enter();
	{
		SEND_MSG* pMsg = NULL;
		__time64_t theMin = MAXLONGLONG;

		//查找给to的消息或消息队列头的消息
		SendQueue::iterator iter = m_sendQueue.begin();
		for (; iter != m_sendQueue.end(); iter++)
		{
			if (! iter->bAllSent)
			{
				if (iter->lastGetDataTime < theMin)
				{
					theMin = iter->lastGetDataTime;
					pMsg = &(*iter);
				}
			}
		}
		
		//如果有符合条件待发送的消息
		if (NULL != pMsg)
		{
			GetDataFromSendMsg(*pMsg, dwMaxSize, byteData, pExpectedComm);
			bFound = TRUE;
		}
	}
	m_sendQueueSection.Leave();

	return bFound;
}

BOOL CutupProtocol::ModifyPacketStatus( const CPGUID& to, CPSERIAL serial,BOOL isStop )
{
	BOOL bFound = FALSE;
	m_sendQueueSection.Enter();
	{
		SendQueue::const_iterator iter = m_sendQueue.begin();
		for (; iter != m_sendQueue.end(); iter++)
		{
			SEND_MSG& sendMsg = (SEND_MSG)*iter;
			if (sendMsg.to == to && sendMsg.serial == serial)
			{
				sendMsg.bIsStop = isStop;
				bFound = TRUE;
				break;
			}
		}
	}
	m_sendQueueSection.Leave();

	return bFound;
}

BOOL CutupProtocol::QuerySendStatus( const CPGUID& to, CPSERIAL serial, DWORD& dwSentBytes, DWORD& dwTotalBytes )
{
	BOOL bFound = FALSE;
	m_sendQueueSection.Enter();
	{
		SendQueue::const_iterator iter = m_sendQueue.begin();
		for (; iter != m_sendQueue.end(); iter++)
		{
			const SEND_MSG& sendMsg = *iter;
			if (sendMsg.to == to && sendMsg.serial == serial)
			{
				dwTotalBytes = sendMsg.byteData.Size();
				dwSentBytes = sendMsg.bAllSent ? dwTotalBytes : sendMsg.params.dataParams.dwNextSendPos;
				bFound = TRUE;

				break;
			}
		}
	}
	m_sendQueueSection.Leave();

	return bFound;
}

void CutupProtocol::CleanMessageByFlag(UINT64 uint64Flag)
{
	m_sendQueueSection.Enter();
	{
		SendQueue::iterator iter = m_sendQueue.begin();
		while (iter != m_sendQueue.end())
		{
			if (iter->uint64Flag == uint64Flag)
			{
				iter = m_sendQueue.erase(iter);
			}
			else
			{
				iter++;
			}
		}
	}
	m_sendQueueSection.Leave();
}

BOOL CutupProtocol::HasReceivedMsg()
{
	BOOL bHasReceivedMsg = FALSE;
	m_recvQueueSection.Enter();
	{
		bHasReceivedMsg = (m_recvQueue.size() > 0);
	}
	m_recvQueueSection.Leave();

	return bHasReceivedMsg;
}

BOOL CutupProtocol::RecvMsg( ByteBuffer& data, CPGUID& from )
{
	::WaitForSingleObject(m_hRecvSemaphore, INFINITE);

	if (! m_bWorking) return FALSE;

	BOOL bGetOne = FALSE;
	m_recvQueueSection.Enter();
	{
		if (m_recvQueue.size() > 0) 
		{
			RECV_BYTE_MSG& recvmsg = m_recvQueue.front();
			data = recvmsg.byteData;
			from = recvmsg.fromCPGuid;

			m_recvQueue.pop_front();

			bGetOne = TRUE;
		}
	}
	m_recvQueueSection.Leave();
	
	return bGetOne;
}

BOOL CutupProtocol::QueryRecvStatus( const CPGUID& from, CPSERIAL serial, DWORD& dwRecvBytes )
{
	BOOL bFound = FALSE;
	m_recvQueueSection.Enter();
	{
		GuidRecvMap::const_iterator mapiter = m_guidRecvMap.find(from);
		if (mapiter != m_guidRecvMap.end())
		{
			const SerialMsgMap& smm = mapiter->second;
			SerialMsgMap::const_iterator recviter = smm.find(serial);
			if (recviter != smm.end())
			{
				bFound = TRUE;
				dwRecvBytes = 0;

				const RECV_MSG& recvMsg = recviter->second;
				PacketList::const_iterator packetIter = recvMsg.packetList.begin();
				for (; packetIter != recvMsg.packetList.end(); packetIter++)
				{
					PCP_PACKET pPacket = *packetIter;
					dwRecvBytes += pPacket->header.size;
				}
			}
		}
	}
	m_recvQueueSection.Leave();

	return bFound;
}

void CutupProtocol::CreateEmptyPacket( ByteBuffer& data ) const
{
	PCP_PACKET pPacket = CreatePacket(0, 0, CPCMD_NO_DATA, NULL, 0);
	DWORD dwPacketSize = PACKET_SIZE(pPacket);
	data.Alloc(dwPacketSize);
	memcpy(data, pPacket, dwPacketSize);
	FreePacket(pPacket);
}

BOOL CutupProtocol::AddRecvPacket( const LPBYTE pData, DWORD dwSize, COMM_NAME fromComm, CPGUID* pCpguid /*= NULL*/ )
{
	if (NULL == pData || 0 == dwSize) return FALSE;

	PCP_PACKET pPacket = (PCP_PACKET) pData;
	//if (pPacket->header.size == 0 && pPacket->header.index == 0) return TRUE;
	DWORD dwPacketSize = PACKET_SIZE(pPacket);
	
	if (dwPacketSize != dwSize) 
	{
		errorLog(_T("invalid packetsize %u %u"), dwPacketSize, dwSize);
		return FALSE;
	}

	BYTE cmd = pPacket->header.cmd;
	const CPGUID& fromGuid = pPacket->header.guid;
	CPSERIAL serial = pPacket->header.serial;

	//返回 包中含带的cpguid
	if (NULL != pCpguid) *pCpguid = fromGuid;

	switch (cmd)
	{
	case CPCMD_ABORT_SEND:
		AbortSending(fromGuid, serial);
		break;
	case CPCMD_RESEND:
		if (pPacket->header.size == sizeof(DWORD))
		{
			DWORD dwReceivedSize = *(LPDWORD)pPacket->data;
			ResendMsg(fromGuid, serial, pPacket->header.index, dwReceivedSize);
		}
		break;
	case CPCMD_DATA_MORE:
	case CPCMD_DATA_END:
		return HandleDataPacket(pPacket, fromComm);
		break;
	case CPCMD_NO_DATA:
		break;
	}

	return TRUE;
}

PCP_PACKET CutupProtocol::CreatePacket( CPSERIAL serial, DWORD index, BYTE cmd, const LPBYTE pData, DWORD dwDataSize ) const
{
	PCP_PACKET pPacket = (PCP_PACKET) malloc(CPHEADER_SIZE + dwDataSize);
	pPacket->header.guid = m_slocalGuid;
	pPacket->header.serial = serial;
	pPacket->header.cmd = cmd;
	pPacket->header.index = index;
	pPacket->header.size = dwDataSize;

	if (dwDataSize > 0) memcpy(pPacket->data, pData, dwDataSize);

	return pPacket;
}

void CutupProtocol::FreePacket( PCP_PACKET pPacket ) const
{
	if (NULL != pPacket) free(pPacket);
}

void CutupProtocol::FreePackets( PCP_PACKET* ppPackets, DWORD dwPacketCount ) const
{
	for (DWORD i = 0; i < dwPacketCount; i++)
	{
		FreePacket(ppPackets[i]);
	}
}

void CutupProtocol::FreePackets( PacketList& packetList ) const
{
	PacketList::iterator iter = packetList.begin();
	for (; iter != packetList.end(); iter++)
	{
		FreePacket(*iter);
	}

	packetList.clear();
}

void CutupProtocol::MergePackets( const PacketList& packetList, ByteBuffer& byteData ) const
{
	DWORD dwDataSize = 0;
	PacketList::const_iterator iter = packetList.begin();
	for (; iter != packetList.end(); iter++)
	{
		dwDataSize += (*iter)->header.size;
	}
	
	byteData.Alloc(dwDataSize);
	LPBYTE pData = byteData;

	iter = packetList.begin();
	for (; iter != packetList.end(); iter++)
	{
		if ((*iter)->header.size > 0)
		{
			memcpy(pData, (*iter)->data, (*iter)->header.size);
			pData += (*iter)->header.size;
		}
	}
}

void CutupProtocol::AddCmdPacket( const CPGUID& to, COMM_NAME commName, CPSERIAL serial, BYTE cmd, DWORD index, const LPBYTE pData, DWORD dwDataSize, BOOL bFront )
{
	SEND_MSG msg;
	msg.to = to;
	msg.dwDefaultPacketMaxSize = 128;
	msg.expectedCommName = commName;
	msg.serial = serial;
	msg.bData = FALSE;
	msg.params.packetParams.cmd = cmd;
	msg.params.packetParams.index = index;
	if (dwDataSize > 0 && pData != NULL)
	{
		msg.byteData.Alloc(dwDataSize);
		memcpy(msg.byteData, pData, dwDataSize);
	}
	msg.finishTime = 0;
	msg.bAllSent = FALSE;
	
	m_sendQueueSection.Enter();
	{
		if (bFront) m_sendQueue.push_front(msg);
		else m_sendQueue.push_back(msg);
	}
	m_sendQueueSection.Leave();
}

BOOL CutupProtocol::HandleDataPacket( const PCP_PACKET pPacket, COMM_NAME fromComm )
{
	BYTE cmd = pPacket->header.cmd;
	const CPGUID& fromGuid = pPacket->header.guid;
	CPSERIAL serial = pPacket->header.serial;
	DWORD packetIndex = pPacket->header.index;

	//根据guid查找对应的serialMsgMap
	GuidRecvMap::iterator guidIter = m_guidRecvMap.find(fromGuid);
	if (guidIter == m_guidRecvMap.end())
	{
		SerialMsgMap temp;
		std::pair<GuidRecvMap::iterator, bool> res = m_guidRecvMap.insert(GuidRecvMap::value_type(fromGuid, temp));
		if (res.second) guidIter = res.first;
	}
	if (guidIter == m_guidRecvMap.end()) return FALSE;

	//查找或创建该序列号serial对应的消息结构
	SerialMsgMap& serialMsgMap = guidIter->second;
	SerialMsgMap::iterator serialIter = serialMsgMap.find(serial);
	if (serialIter == serialMsgMap.end())
	{
		RECV_MSG temp;
		std::pair<SerialMsgMap::iterator, bool> res = serialMsgMap.insert(SerialMsgMap::value_type(serial, temp));
		if (res.second)
		{
			serialIter = res.first;
			_time64(&serialIter->second.begintime);
			serialIter->second.dwDataSize = 0;
			serialIter->second.dwExpectedIndex = 0;
			serialIter->second.iErrorCountPerPacket = 0;
		}
	}
	if (serialIter == serialMsgMap.end()) return FALSE;

	//检查消息序列号的正确性
	if (pPacket->header.index != serialIter->second.dwExpectedIndex)
	{
		errorLog(_T("serial:%u. expected index:%u. recv index:%u"), serial, serialIter->second.packetList.size(), pPacket->header.index);
		serialIter->second.iErrorCountPerPacket++;
		if (serialIter->second.iErrorCountPerPacket > 5)
		{
			debugLog(_T("request abort %u"), serial);
			AddCmdPacket(fromGuid, fromComm, serial, CPCMD_ABORT_SEND, 0, NULL, 0, TRUE);
		}
		else
		{
			debugLog(_T("request resend %u %u %u"), serial, serialIter->second.dwExpectedIndex, serialIter->second.dwDataSize);
			AddCmdPacket(fromGuid, fromComm, serial, CPCMD_RESEND, serialIter->second.dwExpectedIndex, (const LPBYTE)&serialIter->second.dwDataSize, sizeof(serialIter->second.dwDataSize), TRUE);
		}

		return TRUE;
	}

	//调整数据参数
	serialIter->second.dwExpectedIndex++;
	serialIter->second.dwDataSize += pPacket->header.size;
	serialIter->second.iErrorCountPerPacket = 0;
	serialIter->second.lastCommname = fromComm;

	//将消息复制并追加到packetList末尾
	DWORD dwPacketSize = PACKET_SIZE(pPacket);
	PCP_PACKET pClone = (PCP_PACKET) malloc(dwPacketSize);
	memcpy(pClone, pPacket, dwPacketSize);
	serialIter->second.packetList.push_back(pClone);

	//检查是否还有数据
	if (CPCMD_DATA_END == cmd)
	{
		//消息数据包已收齐,做合并处理
		ByteBuffer byteData;
		MergePackets(serialIter->second.packetList, byteData);
		m_recvQueueSection.Enter();
		{
			RECV_BYTE_MSG msg;
			msg.fromCPGuid = fromGuid;
			msg.byteData = byteData;

			m_recvQueue.push_back(msg);
		}
		m_recvQueueSection.Leave();
		::ReleaseSemaphore(m_hRecvSemaphore, 1, NULL);

		//清理消息
		FreePackets(serialIter->second.packetList);
		serialMsgMap.erase(serialIter);
	}

	return TRUE;
}

void CutupProtocol::AbortSending( const CPGUID& to, CPSERIAL serial )
{
	debugLog(_T("abort sending serial[%u]"), serial);
	m_sendQueueSection.Enter();
	{
		SendQueue::iterator iter = m_sendQueue.begin();
		for (; iter != m_sendQueue.end(); iter++)
		{
			const SEND_MSG& sendMsg = *iter;
			if (sendMsg.to == to && sendMsg.serial == serial)
			{
				m_sendQueue.erase(iter);
				break;
			}
		}
	}
	m_sendQueueSection.Leave();
}

BOOL CutupProtocol::ResendMsg( const CPGUID& to, CPSERIAL serial, DWORD dwExpectedIndex, DWORD dwReceivedDataSize )
{
	BOOL bSuccess = FALSE;

	m_sendQueueSection.Enter();
	{
		SendQueue::iterator iter = m_sendQueue.begin();
		for (; iter != m_sendQueue.end(); iter++)
		{
			SEND_MSG& sendMsg = *iter;
			if (sendMsg.to == to && sendMsg.serial == serial)
			{
				if (sendMsg.bData 
					&& dwReceivedDataSize < sendMsg.params.dataParams.dwNextSendPos
					&& dwExpectedIndex < sendMsg.params.dataParams.dwNextIndex)
				{
					sendMsg.params.dataParams.dwNextIndex = dwExpectedIndex;
					sendMsg.params.dataParams.dwNextSendPos = dwReceivedDataSize;
					sendMsg.bAllSent = FALSE;

					bSuccess = TRUE;
				}
								
				break;
			}
		}
	}
	m_sendQueueSection.Leave();

	debugLog(_T("Resend [%u] index:%u size:%u"), serial, dwExpectedIndex, dwReceivedDataSize);

	return bSuccess;
}

void CutupProtocol::GetDataFromSendMsg( SEND_MSG& sendMsg, DWORD dwMaxSize, ByteBuffer& byteData, COMM_NAME* pExpectedComm /*= NULL*/ )
{
	if (dwMaxSize == 0) dwMaxSize = sendMsg.dwDefaultPacketMaxSize;
	if (NULL != pExpectedComm) *pExpectedComm = sendMsg.expectedCommName;

	_time64(&sendMsg.lastGetDataTime);

	if (sendMsg.bData)
	{
		DWORD dwReadSize = min(dwMaxSize, sendMsg.byteData.Size() - sendMsg.params.dataParams.dwNextSendPos);
		DWORD dwNextSendPos = sendMsg.params.dataParams.dwNextSendPos + dwReadSize;
		BOOL bSentAll = (dwNextSendPos >= sendMsg.byteData.Size());
		BYTE cmd = bSentAll ? CPCMD_DATA_END : CPCMD_DATA_MORE;

		PCP_PACKET pPacket = CreatePacket(sendMsg.serial, sendMsg.params.dataParams.dwNextIndex, 
			cmd, (LPBYTE)sendMsg.byteData + sendMsg.params.dataParams.dwNextSendPos, dwReadSize);
		pPacket->ToByteBuffer(byteData);
		FreePacket(pPacket);

		if (! bSentAll)
		{
			//还有未发完的数据
			sendMsg.params.dataParams.dwNextIndex++;
			sendMsg.params.dataParams.dwNextSendPos = dwNextSendPos;
		}
		else
		{
			//内容已经全部发出，记录时间，随后清除，保留一段时间以备重发的请求
			sendMsg.bAllSent = TRUE;
			_time64(&sendMsg.finishTime);
		}
	}
	else
	{
		//如果是packet类型的数据，byteData字段不能过长，这里就不再切分了
		PCP_PACKET pPacket = CreatePacket(sendMsg.serial, sendMsg.params.packetParams.index, sendMsg.params.packetParams.cmd,
			sendMsg.byteData, sendMsg.byteData.Size());
		pPacket->ToByteBuffer(byteData);
		FreePacket(pPacket);

		//下次清理掉
		sendMsg.bAllSent = TRUE;
		sendMsg.finishTime = 0;
	}
}

void CutupProtocol::CleanAllSentMessage( DWORD dwKeepTimeS /*= 30*/ )
{
	__time64_t now;
	_time64(&now);

	m_sendQueueSection.Enter();
	{
		SendQueue::iterator iter = m_sendQueue.begin();
		while (iter != m_sendQueue.end())
		{
			if (iter->bAllSent)
			{
				__time64_t diff = max(now, iter->finishTime) - min(now, iter->finishTime);

				if (diff > dwKeepTimeS)
				{
					//debugLog(_T("clean allsent message serial:%u finishtime:%I64u"), iter->serial, iter->finishTime);
					iter = m_sendQueue.erase(iter);
					continue;
				}
			}

			iter++;
		}
	}
	m_sendQueueSection.Leave();
}
