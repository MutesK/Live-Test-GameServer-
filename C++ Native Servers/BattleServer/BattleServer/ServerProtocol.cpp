#include "BattleServer.h"
#include "Engine/MMOServer.h"
#include "Player.h"
#include "MasterLanClient.h"
#include "ChatLanServer.h"
#include "Engine/Parser.h"
#include "Engine/HttpPost.h"



void CBattleServer::ConnectToken_RamdomGenerator()
{
	char TokenStr[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()`~";
	int TokenLen = strlen(TokenStr);

	for (int i = 0; i < 32; i++)
	{
		ConnectToken[i] = TokenStr[rand() % TokenLen];
	}

	if (ConnectTokenUpdateCount != 0)
	{
		// 채팅서버와 마스터서버에게 연결토큰 재발행 알림.
		ChatServerReqConnectToken();
		MasterServerReqConnectToken();
	}

	ConnectTokenUpdateCount++;
}

void CBattleServer::RoomEnterToken_RamdomGenerator(CRoom *pRoom)
{
	char TokenStr[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()`~";
	int TokenLen = strlen(TokenStr);

	for (int i = 0; i < 32; i++)
	{
		pRoom->EnterToken[i] = TokenStr[rand() % TokenLen];
	}

}


/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

int CBattleServer::ChatServerReqServerOn(ULONG64 ChatSessionID, WCHAR *pServerIp, WORD ChatPort)
{
	if (!_ChatServer.isLogin)
	{
		_ChatServer.pChatServer->ChatDisconnect(_ChatServer.SessionID);
		_ChatServer.isLogin = false;
	}

	lstrcpyW(_ChatServer.ServerIP, pServerIp);
	_ChatServer.ServerPort = ChatPort;
	_ChatServer.SessionID = ChatSessionID;
	_ChatServer.ReqSequence = 0;

	//	채팅서버가 배틀서버에게 서버 켜짐 알림 확인 전송
	WORD Type = en_PACKET_CHAT_BAT_RES_SERVER_ON;
	CPacketBuffer *pSendBuffer = CPacketBuffer::Alloc();
	*pSendBuffer << Type;
	_ChatServer.pChatServer->ChatSendPacket(_ChatServer.SessionID, pSendBuffer);
	_ChatServer.isLogin = true;
	ChatServerReqConnectToken();

	//	배틀서버가 마스터 서버에게 서버 켜짐 알림 전송
	if (_MasterServer.isLogin && !_MasterServer.isServerOnSended)
		MasterServerReqServerON();
	
	for (int i = 0; i < MAX_BATTLEROOM; i++)
	{
		if (Rooms[i].RoomStatus != eNOT_USE && Rooms[i].RoomStatus != eCLOSE_MODE)
			ChatServerCreateRoom(&Rooms[i]);

		Sleep(100);
	}

	return true;
}

int CBattleServer::ChatServerReqConnectToken()
{
	if (!_ChatServer.isLogin)
		return false;

	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

	WORD Type = en_PACKET_CHAT_BAT_REQ_CONNECT_TOKEN;

	*pBuffer << Type;
	pBuffer->PutData(ConnectToken, 32);
	*pBuffer << _ChatServer.ReqSequence;

	_ChatServer.pChatServer->ChatSendPacket(_ChatServer.SessionID, pBuffer);

	return true;
}

int CBattleServer::ChatServerResConnectToken(UINT ReqSequence)
{
	_ChatServer.ReqSequence++;

	return true;

}

int CBattleServer::ChatServerCreateRoom(CRoom *pRoom)
{
	if (!_ChatServer.isLogin)
		return false;

	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

	WORD Type = en_PACKET_CHAT_BAT_REQ_CREATED_ROOM;

	*pBuffer << Type << BattleServerNo << pRoom->RoomNo << pRoom->MaxUser;
	pBuffer->PutData(pRoom->EnterToken, 32);
	*pBuffer << _ChatServer.ReqSequence;

	_ChatServer.pChatServer->ChatSendPacket(_ChatServer.SessionID, pBuffer);

	return true;
}

int CBattleServer::ChatServerDestoryRoom(CRoom *pRoom)
{
	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

	WORD Type = en_PACKET_CHAT_BAT_REQ_DESTROY_ROOM;

	*pBuffer << Type << BattleServerNo << pRoom->RoomNo << _ChatServer.ReqSequence;

	_ChatServer.pChatServer->ChatSendPacket(_ChatServer.SessionID, pBuffer);

	return true;
}

int CBattleServer::ChatServerResDestoryRoom(int RoomNo, UINT ReqSeqnece)
{
	_ChatServer.ReqSequence++;

	return true;
}

int CBattleServer::ChatServerResCreateRoom(int RoomNo, UINT ReqSequence)
{
	_ChatServer.ReqSequence++;

	return true;

}

void CBattleServer::ChatServerDisconnect()
{
	_ChatServer.isLogin = false;
}

int CBattleServer::MasterServerReqServerON()
{
	if (_ChatServer.isLogin)
	{
		_MasterServer.isServerOnSended = true;

		CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

		WORD Type = en_PACKET_BAT_MAS_REQ_SERVER_ON;

		*pBuffer << Type;
		pBuffer->PutData(reinterpret_cast<char *>(BattleServerIP), 32);
		*pBuffer << Port;
		pBuffer->PutData(reinterpret_cast<char *>(ConnectToken), 32);
		pBuffer->PutData(reinterpret_cast<char *>(MasterToken), 32);
		pBuffer->PutData(reinterpret_cast<char *>(_ChatServer.ServerIP), 32);
		*pBuffer << _ChatServer.ServerPort;

		_MasterServer.pMasterServer->MasterSendPacket(pBuffer);

		for (int i = 0; i < MAX_BATTLEROOM; i++)
		{
			if(Rooms[i].RoomStatus != eNOT_USE && Rooms[i].RoomStatus != eCLOSE_MODE)
				MasterServerCreateRoom(&Rooms[i]);

			Sleep(100);
		}
	}

	_MasterServer.isLogin = true;

	return true;
}

int CBattleServer::MastserServerResServerOn(int BattleServerNo)
{
	this->BattleServerNo = BattleServerNo;

	return true;
}

int CBattleServer::MasterServerReqConnectToken()
{
	if (!_MasterServer.isLogin)
		return false;

	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

	WORD Type = en_PACKET_BAT_MAS_REQ_CONNECT_TOKEN;

	*pBuffer << Type;
	pBuffer->PutData(ConnectToken, 32);
	*pBuffer << _ChatServer.ReqSequence;

	_MasterServer.pMasterServer->MasterSendPacket(pBuffer);

}

int CBattleServer::MasterServerResConnectToken(UINT ReqSequence)
{
	_MasterServer.ReqSequence++;

	return true;
}

int CBattleServer::MasterServerCreateRoom(CRoom *pRoom)
{
	//if (!_MasterServer.isLogin)
	//	return false;

	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

	WORD Type = en_PACKET_BAT_MAS_REQ_CREATED_ROOM;

	*pBuffer << Type << BattleServerNo << pRoom->RoomNo << pRoom->MaxUser;
	pBuffer->PutData(pRoom->EnterToken, 32);
	*pBuffer << _MasterServer.ReqSequence;

	_MasterServer.pMasterServer->MasterSendPacket(pBuffer);

	return true;
}

int CBattleServer::MasterServerResCreateRoom(int RoomNo, UINT ReqSequence)
{
	_MasterServer.ReqSequence++;

	return true;
}

int CBattleServer::MasterServerClosedRoom(CRoom *pRoom)
{
	if (!_MasterServer.isLogin)
		return false;

	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

	WORD Type = en_PACKET_BAT_MAS_REQ_CLOSED_ROOM;

	*pBuffer << Type << pRoom->RoomNo << _MasterServer.ReqSequence;
	_MasterServer.pMasterServer->MasterSendPacket(pBuffer);

	return true;
}

int CBattleServer::MasterServerResClosedRoom(int RoomNo, UINT ReqSequence)
{
	_MasterServer.ReqSequence++;

	return true;
}

int CBattleServer::MasterServerLeftUser(CRoom *pRoom)
{
	if (!_MasterServer.isLogin)
		return false;

	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();


	WORD Type = en_PACKET_BAT_MAS_REQ_LEFT_USER;

	*pBuffer << Type << pRoom->RoomNo << _MasterServer.ReqSequence;
	_MasterServer.pMasterServer->MasterSendPacket(pBuffer);

	return true;
}

int CBattleServer::MasterServerResLeftUser(int RoomNo, UINT ReqSequence)
{
	_MasterServer.ReqSequence++;

	return true;
}

void CBattleServer::MasterServerDisconnect()
{
	_MasterServer.isLogin = false;
	_MasterServer.isServerOnSended = false;
}