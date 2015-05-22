#include "stdafx.h"
#include "socket/MySocket.h"
#include "HttpComm.h"

HttpComm::HttpComm():
m_http(NULL)
{

}

BOOL HttpComm::Send( ULONG targetIP, const LPBYTE pData, DWORD dwSize )
{
	

	return TRUE;
}

BOOL HttpComm::SendAndRecv( ULONG targetIP, const LPBYTE pSendData, DWORD dwSendSize, LPBYTE* pRecvData, DWORD& dwRecvSize )
{
#define RECVDATA_MAXSIZE (2 * 1024 * 1024)

	BOOL ret = Connect(targetIP);
	LPBYTE content = NULL;

	if (ret)
	{
		m_http->SetAdditionalDataToSend(pSendData,dwSendSize);
		ret = m_http->SendHttpRequest(_T("POST"));
	}

	if (ret)
	{
		content = m_http->GetRawResponseContent();
		dwRecvSize = m_http->GetRawResponseContentLength();
		*pRecvData = (LPBYTE)malloc(dwRecvSize);
		memcpy(*pRecvData,content,dwRecvSize);
	}

	return ret;
}

BOOL HttpComm::Connect( ULONG targetIP )
{
#ifdef _DEBUG
	g_ConfigInfo.nPort = 8081;
#endif

	char szPort[255] = {0};

	sprintf_s(szPort,"%d",g_ConfigInfo.nPort);

	IN_ADDR connectIP;
	connectIP.S_un.S_addr = targetIP;

	tstring strIp = _T("http://");
	strIp += a2t(inet_ntoa(connectIP));
	strIp += _T(":");
	strIp += a2t(szPort);

	if (!m_http)
	{
		m_http = new ctx::http(strIp.c_str());
	}
	else
	{
		m_http->UpdateUrl(strIp.c_str());
	}

	return m_http != NULL;
}