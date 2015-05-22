#pragma once
#include "IComm.h"
#include "CommData.h"
#include "CutupProtocol.h"
#include "../../pub/ModuleInterface.h"

class CommManager
{
	DECLARE_SINGLETON(CommManager);
public:
	void SetDefaultComm(COMM_NAME commName);
	COMM_NAME GetDefaultComm() const;

	BOOL Str2Commname(LPCTSTR str, COMM_NAME& commName) const;
	BOOL Commname2Str(COMM_NAME commName, tstring& str) const;

	BOOL PushMsgToMaster(COMM_NAME commName, const CommData& data, CPSERIAL* pCPSerial = NULL);
	void CleanMsgByMSGID(MSGID msgid);
	void SendCommTestMessages();
	BOOL IsCommAvailable(COMM_NAME commName) const;
	BOOL IsConnected() const;

	BOOL StartMessageWorker(ULONG testIntervalMS, DWORD dwSendIntervalMS = 2000, DWORD dwMaxNoDataTimeMS = 2000);
	void SetConfig(DWORD testIntervalMS, DWORD dwMaxNoDataTimeMS);
	
	BOOL Send(COMM_NAME commName, ULONG targetIP, const LPBYTE pData, DWORD dwSize);
	BOOL SendAndRecv(COMM_NAME commName, ULONG targetIP, const LPBYTE pSendData, DWORD dwSendSize, ByteBuffer& recvData);

	BOOL ModifyPacketStatus(CPSERIAL serial,CPGUID& guid,BOOL status);
	/*BOOL DeleteMsgBySerial*/
private:
	typedef struct  
	{
		COMM_NAME	commName;
		ULONG		targetIP;
		ByteBuffer	byteData;
	} TOSEND_ITEM;

	typedef struct  
	{
		FnExecuteRCCommand	fnCallback;
		MSGID				msgid;
		ByteBuffer			data;
		LPVOID				lpParameter;
	} EXECUTOR_PARAMETER, *PEXECUTOR_PARAMETER;

private:
	ULONG GetIPByCpguid(const CPGUID& cpguid);
	static DWORD WINAPI MessageSender(LPVOID lpParameter);
	void MessageSenderProc();
	static DWORD WINAPI CmdExcutor(LPVOID lpParameter);

	void DisconnectedNotify();
	void ConnectedNotify();

private:
	CutupProtocol	m_cp;
	IComm*			m_commList[COMMNAME_MAX];
	BOOL			m_commAvailableList[COMMNAME_MAX];
	COMM_NAME		m_defaultCommName;
	volatile LONG	m_bConnected;
	
	Handle			m_hExitEvent;
	BOOL			m_bWorking;
	DWORD			m_dwMsgIntervalMS;
	Thread			m_messageSenderThread;
};
