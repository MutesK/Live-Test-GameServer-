#include "LanBattle.h"
#include "ChatServer.h"

CLanBattle::CLanBattle(CChatServer *pServer, WCHAR* __ChatServerIP, int Port)
{
	lstrcpyW(ChatServerIP, __ChatServerIP);
	CharServerPort = Port;

	pChatServer = pServer;
}


CLanBattle::~CLanBattle()
{
}


bool CLanBattle::Connect(WCHAR *szOpenIP, int port, bool nagleOption)
{
	lstrcpyW(OpenIP, szOpenIP);
	Port = port;
	NagleOption = nagleOption;

	while (!CLanClient::Connect(OpenIP, Port, NagleOption))
	{
		wprintf(L"Disconnected BattleServer ReConnect Try\n");
	}

	return true;
}


void CLanBattle::Stop()
{
	CLanClient::Stop();
}
void CLanBattle::OnConnected()
{
	// 서버 켜짐 안내
	WORD Type = en_PACKET_CHAT_BAT_REQ_SERVER_ON;

	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

	*pBuffer << Type;

	pBuffer->PutData(reinterpret_cast<char *>(ChatServerIP), 32);
	*pBuffer << CharServerPort;

	SendPacket(pBuffer);
}

void CLanBattle::OnDisconnect()
{
	// 재접속 유도한다.
	pChatServer->DisconnectBattleServer();

	while (!CLanClient::Connect(OpenIP, Port, NagleOption))
	{
		wprintf(L"Disconnected MasterServer ReConnect Try\n");
	}
}

void CLanBattle::OnRecv(CPacketBuffer *pOutBuffer)
{
	// 패킷 프로시저
	WORD Type;

	*pOutBuffer >> Type;

	switch (Type)
	{
	case en_PACKET_CHAT_BAT_RES_SERVER_ON:
		break;
	case en_PACKET_CHAT_BAT_REQ_CONNECT_TOKEN:
	{
		char ConnectToken[32];
		UINT ReqSequence;

		pOutBuffer->GetData(ConnectToken, 32);
		*pOutBuffer >> ReqSequence;

		pChatServer->ReqConnectToken(ConnectToken);

		CPacketBuffer *pSendBuffer = CPacketBuffer::Alloc();

		Type = en_PACKET_CHAT_BAT_RES_CONNECT_TOKEN;

		*pSendBuffer << Type << ReqSequence;

		SendPacket(pSendBuffer);
	}
		break;
	case en_PACKET_CHAT_BAT_REQ_CREATED_ROOM:
	{
		int BattleServerNo;
		int RoomNo;
		int MaxUser;
		char EnterToken[32];

		UINT ReqSequence;

		*pOutBuffer >> BattleServerNo >> RoomNo >> MaxUser;
		pOutBuffer->GetData(EnterToken, 32);
		*pOutBuffer >> ReqSequence;

		pChatServer->ReqCreateRoom(BattleServerNo, RoomNo, MaxUser, EnterToken);

		CPacketBuffer *pSendBuffer = CPacketBuffer::Alloc();

		Type = en_PACKET_CHAT_BAT_RES_CREATED_ROOM;

		*pSendBuffer << Type << RoomNo << ReqSequence;

		SendPacket(pSendBuffer);

	}
		break;
	case en_PACKET_CHAT_BAT_REQ_DESTROY_ROOM:
	{
		int BattleServerNo;
		int RoomNo;
		UINT ReqSequence;

		*pOutBuffer >> BattleServerNo >> RoomNo >> ReqSequence;
		pChatServer->ReqDestoryRoom(BattleServerNo, RoomNo);

		CPacketBuffer *pSendBuffer = CPacketBuffer::Alloc();
		Type = en_PACKET_CHAT_BAT_RES_DESTROY_ROOM;

		*pSendBuffer << Type << RoomNo << ReqSequence;

		SendPacket(pSendBuffer);
	}
		break;
	default:
		wprintf(L"PacketError\n");
		CCrashDump::Crash();
		break;
	}

	pOutBuffer->Free();
}

void CLanBattle::OnSend(int SendSize)
{

}
void CLanBattle::OnError(int errorcode, WCHAR *msg)
{

}

bool CLanBattle::SendPacketToBattleServer(CPacketBuffer *pBuffer)
{
	return SendPacket(pBuffer);
}