#include "MornitoringAgent.h"
#include "MatchServer.h"


CMornitoringAgent::CMornitoringAgent(CMatchServer& Server, WORD ServerNo, const array<BYTE, 32>& LoginSessionKey)
	:Server(Server), ServerNo(ServerNo), LoginSessionKey(const_cast<array<BYTE, 32>&>(LoginSessionKey))
{
}


CMornitoringAgent::~CMornitoringAgent()
{
}


bool CMornitoringAgent::Connect(const array<WCHAR, 16>& ServerIP, int port, bool nagleOption)
{
	_OpenIP = ServerIP;
	_port = port;

	while (!CLanClient::Connect(const_cast<WCHAR *>(ServerIP.data()), _port, nagleOption))
	{
		wcout << L"Disconnect Mornitoring Server ReTry \n";
	}

	return true;
}

void CMornitoringAgent::Stop()
{
	CLanClient::Stop();
}

void CMornitoringAgent::OnConnected()
{
	// 서버 켜짐 알림 전송
	CPacketBuffer& Buffer = *CPacketBuffer::Alloc();

	WORD Type = en_PACKET_SS_MONITOR_LOGIN;

	Buffer << Type << ServerNo;

	SendPacket(&Buffer);
	isMornMode = true;
}

void CMornitoringAgent::OnDisconnect()
{
	isMornMode = false;
}

void CMornitoringAgent::OnRecv(CPacketBuffer *pOutBuffer)
{
		// 일방적 전송 타입
}

void CMornitoringAgent::OnSend(int SendSize)
{

}
void CMornitoringAgent::OnError(int errorcode, WCHAR *msg)
{

}

bool CMornitoringAgent::Send(const BYTE& DataType, const int& DataValue, const int& TimeStamp)
{
	if (isMornMode)
	{
		CPacketBuffer& Buffer = *CPacketBuffer::Alloc();

		WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;

		Buffer << Type << DataType << DataValue << TimeStamp;

		return SendPacket(&Buffer);
	}

	return false;
}
