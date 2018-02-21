#include "BattleLanServer.h"
#include "MasterServer.h"
#include "CommonProtocol.h"


BattleLanServer::BattleLanServer()
{
	PacketError = 0;
	Disconnected = 0;
}


BattleLanServer::~BattleLanServer()
{
}


bool BattleLanServer::Start(WCHAR* szServerIP, int Port, int WorkerThread, int MaxSession, bool NagleOption,  char *pMasterToken)
{
	if (!CLanServer::Start(szServerIP, Port, WorkerThread, NagleOption, MaxSession))
		return false;

	memcpy(MasterToken, pMasterToken, 32);

	return true;
}

void BattleLanServer::Stop()
{

}

void BattleLanServer::SyncBattleInfo(CBattleInfo *pArrInfo[])
{
	for (int i = 0; i < MAX_BATTLESERVER; i++)
	{
		pArrInfo[i] = &BattleServerArr[i];
	}
}

void BattleLanServer::OnClientJoin(ULONG64 OutSessionID, WCHAR *pClientIP, int Port)
{
	
}

void BattleLanServer::OnClientLeave(ULONG64 SessionID)
{
	ServerDisconnect(SessionID);
}

bool BattleLanServer::OnConnectionRequest(WCHAR *szIP, int Port)
{
	return true;
}
void BattleLanServer::Monitoring()
{
	wcout << L"========== BattleServer =============\n";
	wcout << L"Online Server : " << BattleServerNum << endl;
	wcout << L"Open Room Total :" << ActivateGameRoomTotal << endl;
	wcout << L"PacketError :" << PacketError << endl;
	wcout << L"Disconnect Dectected :" << Disconnected << endl;
	wcout << L"Recv PacketTPS :" << m_RecvPacketTPS << endl;
	wcout << L"Send PacketTPS :" << m_SendPacketTPS << endl;
	wcout << L"Packet Pool : " << CPacketBuffer::PacketPool.GetChunkSize() << L"\n";
	wcout << L"Packet Pool Use: " << CPacketBuffer::PacketPool.GetAllocCount() << L"\n\n";
	m_RecvPacketTPS = m_SendPacketTPS = 0;
}

void BattleLanServer::OnRecv(ULONG64 OutSessionID, CPacketBuffer *pOutBuffer)
{
	WORD Type;

	*pOutBuffer >> Type;

	switch (Type)
	{
	case en_PACKET_BAT_MAS_REQ_SERVER_ON:
		ReqServerOn(OutSessionID, pOutBuffer);
		//	배틀서버가 마스터 서버에게 서버 켜짐 알림
		break;
	case en_PACKET_BAT_MAS_REQ_CONNECT_TOKEN:
		//	배틀서버의 연결토큰 재발행 알림.
		ReqConnectToken(OutSessionID, pOutBuffer);
		break;
	case en_PACKET_BAT_MAS_REQ_CREATED_ROOM:
		//	배틀 서버의 신규 대기 방 생성 알림
		ReqCreatedRoom(OutSessionID, pOutBuffer);
		break;
	case en_PACKET_BAT_MAS_REQ_CLOSED_ROOM:
		//	방 닫힘 알림
		ReqClosedRoom(OutSessionID, pOutBuffer);
		break;
	case en_PACKET_BAT_MAS_REQ_LEFT_USER:
		//	방에서 유저가 나갔음, 1명 나갈때 마다 전송.
		ReqLeftONEUser(OutSessionID, pOutBuffer);
		break;
	default:
		PacketError++;
		break;
	}

	pOutBuffer->Free();
}

void BattleLanServer::OnSend(ULONG64 OutSessionID, int SendSize)
{

}

void BattleLanServer::OnError(int errorcode, WCHAR *szMessage)
{

}

void BattleLanServer::OnWorkerThreadBegin()
{

}
void BattleLanServer::OnWorkerThreadEnd()
{

}

bool BattleLanServer::ServerDisconnect(ULONG64 SessionID)
{
	for (int i = 0; i < MAX_BATTLESERVER; i++)
	{
		if (BattleServerArr[i].OutSessionID == SessionID)
		{
			if (BattleServerArr[i].isUse)
			{
				Disconnected++;
				BattleServerArr[i].isUse = false;
				_InterlockedAdd64(&BattleServerNum, -1);

				for (int j = 0; j < MAX_BATTLEROOM; j++)
				{
					CRoom *pRoom = &BattleServerArr[i].Rooms[j];

					if (pRoom->isUse)
					{
						pRoom->isUse = false;
						ActivateGameRoomTotal--;
					}
					
				}
			}
			return true;
		}
	}

	return false;
}

void BattleLanServer::ReqServerOn(ULONG64 SessionID, CPacketBuffer *pBuffer)
{
	int BattleIndex = ServerOn(SessionID, pBuffer);

	if (BattleIndex == -1)
	{
		SYSLOG(L"BATTLE", LOG_SYSTEM, L"Master Token isn't Matched");
		Disconnect(SessionID);
		CCrashDump::Crash();
		return;
	}

	ResServerOn(SessionID, BattleIndex);
}

int BattleLanServer::ServerOn(ULONG64 BattleSessionID, CPacketBuffer *pOutBuffer)
{
	//	배틀서버가 마스터 서버에게 서버 켜짐 알림
	// 락이 필요하다.
	char _MasterToken[32];
	BattleServerIndex = InterlockedAdd(&BattleServerIndex, 1) % MAX_BATTLESERVER;

	CBattleInfo *pServer = &BattleServerArr[BattleServerIndex];

	pServer->BattleServerNo = BattleServerIndex;
	pServer->OutSessionID = BattleSessionID;
	pOutBuffer->GetData(reinterpret_cast<char *>(pServer->ServerIP), 32);
	*pOutBuffer >> pServer->Port;

	pOutBuffer->GetData(reinterpret_cast<char *>(pServer->ConnectToken), 32);
	pOutBuffer->GetData(reinterpret_cast<char *>(_MasterToken), 32);

	pOutBuffer->GetData(reinterpret_cast<char *>(pServer->ChatServerIP), 32);
	*pOutBuffer >> pServer->ChatSeverPort;

	if (memcmp(_MasterToken, MasterToken, 32) != 0)
		return -1; // 토큰이 같지 앉음.

	pServer->isUse = true;
	_InterlockedAdd64(&BattleServerNum, 1);

	return pServer->BattleServerNo;
}

void BattleLanServer::ResServerOn(ULONG64 SessionID, int BattleIndex)
{
	WORD Type = en_PACKET_BAT_MAS_RES_SERVER_ON;

	CPacketBuffer *pSendBuffer = CPacketBuffer::Alloc();

	*pSendBuffer << Type << BattleIndex;
	SendPacket(SessionID, pSendBuffer);
}

void BattleLanServer::ReqConnectToken(ULONG64 SessionID, CPacketBuffer *pBuffer)
{
	char connectToken[32];
	UINT ReqSeqence;

	pBuffer->GetData(connectToken, 32);
	*pBuffer >> ReqSeqence;

	ConnectToken(SessionID, connectToken);

	ResConnectToken(SessionID, ReqSeqence);
}

int BattleLanServer::ConnectToken(ULONG64 SessionID, char *pToken)
{
	//	배틀서버의 연결토큰 재발행 알림.
	for (int i = 0; i < MAX_BATTLESERVER; i++)
	{
		if (!BattleServerArr[i].isUse)
			continue;

		if (BattleServerArr[i].OutSessionID == SessionID)
		{
			memcpy(BattleServerArr[i].ConnectToken, pToken, 32);
			return true;
		}
	}

	return -1;
}

void BattleLanServer::ResConnectToken(ULONG64 SessionID, UINT ReqSeqence)
{
	WORD Type = en_PACKET_BAT_MAS_RES_CONNECT_TOKEN;

	CPacketBuffer *pSendBuffer = CPacketBuffer::Alloc();

	*pSendBuffer << Type << ReqSeqence;
	SendPacket(SessionID, pSendBuffer);
}

void BattleLanServer::ReqCreatedRoom(ULONG64 SessionID, CPacketBuffer *pBuffer)
{
	//	배틀서버는 자신이 보유한 대기방이 없을시 각자 알아서 1개 이상의 방을 생성하여 마스터 서버에게 전달 한다
	//	만약 방을 더 이상 만들 수 없는 상태라면 배틀 서버가 알아서 방생성을 중지한다. 
	//	그 이후 여유가 있을때 각자가 생성하여 마스터 서버에게 전달 한다.
	//
	//	마스터 서버가 하나하나 방 생성을 요청하지 않고,  배틀서버가 1개 이상의 대기방을 만들어서 알려주는 방식.
	//
	//	마스터 서버는 각 배틀서버의 IP/PORT 를 알고 있으므로 방에 관련 정보만 전달 한다.

	int BattleServerNo;
	int RoomNo;
	int MaxUser;
	char EnterToken[32];

	UINT ReqSeqence;

	*pBuffer >> BattleServerNo >> RoomNo >> MaxUser;
	pBuffer->GetData(EnterToken, 32);

	*pBuffer >> ReqSeqence;
	CreateRoom(SessionID, BattleServerNo, RoomNo, MaxUser, EnterToken);
	ResCreateRoom(SessionID, RoomNo, ReqSeqence);
}

int BattleLanServer::CreateRoom(ULONG64 SessionID, int ServerNo, int RoomNo, int MaxUser, char *pToken)
{
	// 이왕 있는김에 둘다 넣자~
	if (BattleServerArr[ServerNo].OutSessionID == SessionID)
	{
		CRoom *pRoom = &BattleServerArr[ServerNo].Rooms[RoomNo];
		
		if (!pRoom->isUse)
		{
			_InterlockedAdd64(&ActivateGameRoomTotal, 1);

			pRoom->BattleServerNo = ServerNo;
			pRoom->MaxUser = MaxUser;
			pRoom->RoomNo = RoomNo;
			memcpy(pRoom->EnterToken, pToken, 32);
			pRoom->isUse = true;

			return true;
		}
	}

	return -1;
}

void BattleLanServer::ResCreateRoom(ULONG64 SessionID, int RoomNo, UINT ReqSeqence)
{
	WORD Type = en_PACKET_BAT_MAS_RES_CREATED_ROOM;
	CPacketBuffer *pSendBuffer = CPacketBuffer::Alloc();

	*pSendBuffer << Type << RoomNo << ReqSeqence;
	SendPacket(SessionID, pSendBuffer);
}

void BattleLanServer::ReqClosedRoom(ULONG64 SessionID, CPacketBuffer *pBuffer)
{
	//	대기방이 플레이를 위해, 또는 기타 사유로 인해서 방이 닫혀짐을 마스터에게 알림
	//	마스터 서버는 해당 방의 정보를 삭제하고 사용자를 배정하지 않는다.
	//
	//	대기방 -> 플레이		본 패킷 송신
	//	플레이 -> 종료		이미 플레이시 닫혔으므로 송신 하지 않음.

	int RoomNo;
	UINT ReqSequence;

	*pBuffer >> RoomNo >> ReqSequence;

	CloseRoom(SessionID, RoomNo);
	ResCloseRoom(SessionID, RoomNo, ReqSequence);
}

int BattleLanServer::CloseRoom(ULONG64 SessionID, int RoomNo)
{
	for (int i = 0; i < MAX_BATTLESERVER; i++)
	{
		if (!BattleServerArr[i].isUse)
			continue;

		if (BattleServerArr[i].OutSessionID == SessionID)
		{
			CRoom *pRoom = &BattleServerArr[i].Rooms[RoomNo];
			pRoom->isUse = false;
			_InterlockedAdd64(&ActivateGameRoomTotal, -1);
			return true;
		}
	}

	return -1;
}

void BattleLanServer::ResCloseRoom(ULONG64 SessionID, int RoomNo, UINT ReqSeqence)
{
	WORD Type = en_PACKET_BAT_MAS_RES_CLOSED_ROOM;
	CPacketBuffer *pSendBuffer = CPacketBuffer::Alloc();

	*pSendBuffer << Type << RoomNo << ReqSeqence;
	SendPacket(SessionID, pSendBuffer);
}
void BattleLanServer::ReqLeftONEUser(ULONG64 SessionID, CPacketBuffer *pBuffer)
{
	//	사용자가 들어가는 부분은 마스터 서버가 알아서 카운팅 하지만, 
	//	배틀서버에 들어갔다가 나간 유저는 마스터 서버가 파악 불가능.
	//
	//	그러므로 유저가 나갈 경우에만 배틀서버 -> 마스터서버 로 전달 한다.
	//
	//	현재 인원의 수치를 전달 하는것이 아닌 -1 값을 보내는 의미이므로 마스터서버는 
	//	해당 방 배정가능 인원을 +1 시켜주면 됨.

	int RoomNo;
	UINT ReqSequence;

	*pBuffer >> RoomNo >> ReqSequence;

	LeftOneUser(SessionID, RoomNo);
	ResLeftOneUser(SessionID, RoomNo, ReqSequence);
}

int BattleLanServer::LeftOneUser(ULONG64 SessionID, int RoomNo)
{
	for (int i = 0; i < MAX_BATTLESERVER; i++)
	{
		if (!BattleServerArr[i].isUse)
			continue;

		if (BattleServerArr[i].OutSessionID == SessionID)
		{
			CRoom *pRoom = &BattleServerArr[i].Rooms[RoomNo];
			InterlockedAdd(&pRoom->MaxUser, 1);
			//	현재 인원의 수치를 전달 하는것이 아닌 -1 값을 보내는 의미이므로 마스터서버는 
			//	해당 방 배정가능 인원을 +1 시켜주면 됨.
			return true;
		}
	}

	return -1;
}

void BattleLanServer::ResLeftOneUser(ULONG64 SessionID, int RoomNo, UINT ReqSequence)
{
	WORD Type = en_PACKET_BAT_MAS_RES_LEFT_USER;
	CPacketBuffer *pSendBuffer = CPacketBuffer::Alloc();

	*pSendBuffer << Type << RoomNo << ReqSequence;
	SendPacket(SessionID, pSendBuffer);
}