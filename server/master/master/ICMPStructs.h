#pragma once

#pragma pack(push, 1)
typedef struct _TCP_HEADER  //应用层，传输层层
{
	WORD SrcPort; // 源端口
	WORD DstPort; // 目的端口
	DWORD SeqNum; // 顺序号
	DWORD AckNum; // 确认号
	BYTE DataOff; // TCP头长
	BYTE Flags; // 标志（URG、ACK等）
	WORD Window; // 窗口大小
	WORD Chksum; // 校验和
	WORD UrgPtr; // 紧急指针
} TCP_HEADER, *LPTCP_HEADER;
typedef TCP_HEADER UNALIGNED * ULPTCP;

typedef struct _IP_HEADER  //传输层，网络层
{
	BYTE HdrLen:4; // IHL
	BYTE Version:4; // 版本
	BYTE ServiceType; // 服务类型
	WORD TotalLen; // 总长
	WORD ID; // 标识
	union
	{
		WORD Flags; // 标志
		WORD FragOff; // 分段偏移
	};
	BYTE TimeToLive; // 生命期
	BYTE Protocol; // 协议
	WORD HdrChksum; // 头校验和
	DWORD SrcAddr; // 源地址
	DWORD DstAddr; // 目的地址
	BYTE Options; // 选项
} IP_HEADER, *LPIP_HEADER;
typedef IP_HEADER UNALIGNED * ULPIP;

#define IP_HEADER_LEN_UNIT_BYTE 4 

// ICMP头部
typedef struct _ICMP_HEADER
{
	BYTE i_type; //8位類型
	BYTE i_code; //8位代碼
	USHORT i_cksum; //16位校驗和 
	USHORT i_id; //識別號（一般用進程號作為識別號）
	USHORT i_seq; //報文序列號 
	//	ULONG timestamp; //時間戳
} ICMP_HEADER, *LPICMP_HEADER;

#pragma pack(pop)


#define ICMP_ECHO        8
#define ICMP_ECHOREPLY   0
