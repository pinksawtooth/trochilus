#include "stdafx.h"
#include "DnsComm.h"
#define DNS_QUERY_DONAME "www.baidu.com"

DnsComm::DnsComm() : m_dns(enCLIENT)
{

}

DnsComm::~DnsComm()
{

}

BOOL DnsComm::Send(ULONG targetIP, const LPBYTE pData, DWORD dwSize)
{
	m_dns.SetServerAddress(targetIP);
	return m_dns.SendDNSQuery(DNS_QUERY_DONAME, (char*)pData, dwSize, 0);
}

BOOL DnsComm::SendAndRecv(ULONG targetIP, const LPBYTE pSendData, DWORD dwSendSize, LPBYTE* pRecvData, DWORD& dwRecvSize)
{
	std::string strRecvData;
	BOOL bRet = FALSE;
	m_dns.SetServerAddress(targetIP);
	if ( !m_dns.SendDNSQuery(DNS_QUERY_DONAME, (char*)pSendData, dwSendSize, 2 * 1000) )
	{
		errorLog(_T("send dns query failed"));
		return FALSE;
	}
	if ( m_dns.ParseResponsePacket(strRecvData) )
	{
		dwRecvSize = strRecvData.size();
		*pRecvData = Alloc(dwRecvSize);
		memcpy(*pRecvData, strRecvData.data(), dwRecvSize);
		bRet = TRUE;
	}
	m_dns.ClearQuest();
	return bRet;
}