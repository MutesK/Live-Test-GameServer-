#include "MatchLanServer.h"
#include "MasterServer.h"
#include "Engine/Parser.h"
#include "Engine/HttpPost.h"
#include "BattleLanServer.h"
#include <queue>
CMatchLanServer::CMatchLanServer(BattleLanServer *pBattleServer)
	:pBattleServer(pBattleServer)
{
	MatchServerNum = 0;
	PacketError = 0;
	InitializeSRWLock(&PlayerLock);
}

CMatchLanServer::~CMatchLanServer()
{
}

bool CMatchLanServer::Start(WCHAR* szServerIP, int Port, int WorkerThread, int MaxSession, bool NagleOption, char *pMasterToken)
{
	if (!CLanServer::Start(szServerIP, Port, WorkerThread, NagleOption, MaxSession))
		return false;

	pBattleServer->SyncBattleInfo(BattleServerArr);
	memcpy(MasterToken, pMasterToken, 32);

	return true;
}
void CMatchLanServer::Stop()
{
	CLanServer::Stop();
}

void CMatchLanServer::OnClientJoin(ULONG64 OutSessionID, WCHAR *pClientIP, int Port)
{

}

void CMatchLanServer::OnClientLeave(ULONG64 SessionID)
{
	ServerDisconnect(SessionID);
}

bool CMatchLanServer::OnConnectionRequest(WCHAR *szIP, int Port)
{
	return true;
}

void CMatchLanServer::OnRecv(ULONG64 OutSessionID, CPacketBuffer *pOutBuffer)
{
	WORD Type;

	*pOutBuffer >> Type;

	switch (Type)
	{
	case en_PACKET_MAT_MAS_REQ_SERVER_ON:
		ReqMatchServerOn(OutSessionID, pOutBuffer);
		break;
	case en_PACKET_MAT_MAS_REQ_GAME_ROOM:
		ReqGameRoom(OutSessionID, pOutBuffer);
		break;
	case en_PACKET_MAT_MAS_REQ_ROOM_ENTER_SUCCESS:
		ReqGameRoomEnterSuccess(OutSessionID, pOutBuffer);
		break;
	case en_PACKET_MAT_MAS_REQ_ROOM_ENTER_FAIL:
		ReqGameRoomEnterFail(OutSessionID, pOutBuffer);
		break;
	default:
		//CCrashDump::Crash();
		PacketError++;
		break;
	}


	pOutBuffer->Free();
}

void CMatchLanServer::Monitoring()
{
	wcout << L"========== MatchServer =============\n";
	wcout << L"Online Server : " << MatchServerNum << endl;
	wcout << L"Disconnect Dectected :" << Disconnected << endl;
	wcout << L"Request Player : " << PlayerPool.GetAllocCount() << endl;
	wcout << L"Room Request : " << RoomRequest << endl;
	wcout << L"PacketError : " << PacketError << endl;
	wcout << L"Recv PacketTPS : " << m_RecvPacketTPS << endl;
	wcout << L"Send PacketTPS : " << m_SendPacketTPS << endl;

	m_RecvPacketTPS = m_SendPacketTPS = 0;
}


void CMatchLanServer::SendMatchPacket(ULONG64 SessionID, CPacketBuffer *pBuffer)
{
	if (!SendPacket(SessionID, pBuffer))
		pBuffer->Free();
}

void CMatchLanServer::OnSend(ULONG64 OutSessionID, int SendSize)
{

}

void CMatchLanServer::OnError(int errorcode, WCHAR *szMessage)
{

}

void CMatchLanServer::OnWorkerThreadBegin()
{

}

void CMatchLanServer::OnWorkerThreadEnd()
{

}

bool CMatchLanServer::ServerDisconnect(ULONG64 SessionID)
{
	for (int i = 0; i < MAX_MATCHSERVER; i++)
	{
		if (MatchServerInfo[i].SessionID == SessionID)
		{
			Disconnected++;

			MatchServerInfo[i].Used = false;
			_InterlockedAdd64(&MatchServerNum, -1);

			for (auto iter = PlayerMap.begin(); iter != PlayerMap.end(); ++iter)
			{
				CPlayer *pPlayer = iter->second;

				PlayerMap.erase(pPlayer->ClinetKey);
				PlayerPool.Free(pPlayer);
			}

			return true;
		}
	}

	return false;
}

void CMatchLanServer::ReqMatchServerOn(ULONG64 SessionID, CPacketBuffer *pBuffer)
{
	//	매치메이킹 서버가 마스터 서버에게 서버 켜짐 알림 (로그인)
	int ServerNo;
	char pMasterToken[32];

	*pBuffer >> ServerNo;
	pBuffer->GetData(pMasterToken, 32);

	int ret = 0;
	if (memcmp(pMasterToken, MasterToken, 32) != 0)
		ret == -1;
	else if (MatchServerInfo[ServerNo].Used)
		ret == -2;

	MatchServerInfo[ServerNo].Used = true;
	MatchServerInfo[ServerNo].ServerNo = ServerNo;
	MatchServerInfo[ServerNo].SessionID = SessionID;

	MatchServerNum = _InterlockedAdd64(&MatchServerNum, 1) % MAX_BATTLESERVER;

	switch (ret)
	{
	case -1:
		SYSLOG(L"MATCH", LOG_SYSTEM, L"Master Server Token isn't Matched");
		Disconnect(SessionID);
		CCrashDump::Crash();
		return;
	case -2:
		SYSLOG(L"MATCH", LOG_SYSTEM, L"Already Server %d is Using Now", ServerNo);
		Disconnect(SessionID);
		CCrashDump::Crash();
		return;
	}

	ResMatchServerOn(SessionID, ServerNo);
}

void CMatchLanServer::ResMatchServerOn(ULONG64 SessionID, WORD ServerNo)
{
	CPacketBuffer *pSendBuffer = CPacketBuffer::Alloc();

	WORD Type = en_PACKET_MAT_MAS_RES_SERVER_ON;
	*pSendBuffer << Type << ServerNo;

	SendPacket(SessionID, pSendBuffer);
}

void CMatchLanServer::ReqGameRoom(ULONG64 SessionID, CPacketBuffer *pBuffer)
{
	//	매치메이킹 서버가 마스터 서버에게 게임방 정보를 요청
	// 예외적으로 MasterServer 클래스가 따로 패킷을 전송한다.

	UINT64 ClientKey;
	*pBuffer >> ClientKey;

	InterlockedAdd64(&RoomRequest, 1);

	CBattleInfo *pInfo = nullptr;

	for (int retry = 100; retry > 0; retry--)
	{
		for (int BattleIndex = 0; BattleIndex < MAX_BATTLESERVER; BattleIndex++)
		{
			if (!BattleServerArr[BattleIndex]->isUse)
				continue;

			pInfo = BattleServerArr[BattleIndex];
			break;
		}

		if (pInfo == nullptr)
		{
			Sleep(100);
			continue;
		}

		bool isfind = false;
		
		priority_queue<CRoom *, vector<CRoom *>, cmp> Roompq;
		for (int RoomIndex = 0; RoomIndex < MAX_BATTLEROOM; RoomIndex++)
		{
			CRoom *pRoom = &pInfo->Rooms[RoomIndex];

			if (!pRoom->isUse)
				continue;
			
			if (pRoom->MaxUser != 0)
				Roompq.push(pRoom);
		}

		if (Roompq.empty())
			continue;

		while (!Roompq.empty())
		{
			CRoom *pRoom = Roompq.top();

			pRoom->Lock();
			if (!pRoom->isUse)
			{
				pRoom->UnLock();
				continue;
			}

			if (pRoom->MaxUser == 0)
			{
				pRoom->UnLock();
				continue;
			}

			pRoom->MaxUser--;

			CPlayer *pPlayer = PlayerPool.Alloc();
			pPlayer->ClinetKey = ClientKey;
			pPlayer->RoomNo = pRoom->RoomNo;
			pPlayer->BattleServerNo = pInfo->BattleServerNo;
			pPlayer->TickCheck = CUpdateTime::GetTickCount();

			pair <UINT64, CPlayer *> PlayerPair = { ClientKey, pPlayer };

			AcquireSRWLockExclusive(&PlayerLock);
			PlayerMap.insert(PlayerPair);
			ReleaseSRWLockExclusive(&PlayerLock);


			ResGameRoom(SessionID, pPlayer->ClinetKey, 1, pInfo, pRoom);

			pRoom->UnLock();
			break;
		}

		InterlockedAdd64(&RoomRequest, -1);
		return;

		Sleep(1000);
	}

	ResGameRoom(SessionID, ClientKey, 0, nullptr, nullptr);
	InterlockedAdd64(&RoomRequest, -1);

}

void CMatchLanServer::ResGameRoom(ULONG64 SessionID, UINT64 ClientKey,BYTE Status, CBattleInfo *pInfo, CRoom *pRoom)
{
	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

	WORD Type = en_PACKET_MAT_MAS_RES_GAME_ROOM;
	*pBuffer << Type << ClientKey << Status;

	if(Status)
	{
		*pBuffer << pInfo->BattleServerNo;
		pBuffer->PutData((char *)pInfo->ServerIP, 32);
		*pBuffer << pInfo->Port << pRoom->RoomNo;
		pBuffer->PutData((char *)pInfo->ConnectToken, 32);
		pBuffer->PutData((char *)pRoom->EnterToken, 32);

		pBuffer->PutData((char *)pInfo->ChatServerIP, 32);
		*pBuffer << pInfo->ChatSeverPort;
	}

	SendPacket(SessionID, pBuffer);
}

void CMatchLanServer::ReqGameRoomEnterSuccess(ULONG64 SessionID, CPacketBuffer *pBuffer)
{
	UINT64 ClientKey;
	WORD BattleServerNo;
	int RoomNo;

	*pBuffer >> BattleServerNo >> RoomNo >> ClientKey;
	
	auto iter = PlayerMap.find(ClientKey);
	if (iter == PlayerMap.end())
		return;

	CPlayer *pPlayer = iter->second;

	if (pPlayer->BattleServerNo != BattleServerNo ||
		pPlayer->RoomNo != RoomNo)
		return;

	AcquireSRWLockExclusive(&PlayerLock);
	PlayerMap.erase(ClientKey);
	PlayerPool.Free(pPlayer);
	ReleaseSRWLockExclusive(&PlayerLock);
}
void CMatchLanServer::ReqGameRoomEnterFail(ULONG64 SessionID, CPacketBuffer *pBuffer)
{
	UINT64 ClientKey;

	*pBuffer >> ClientKey;

	auto iter = PlayerMap.find(ClientKey);
	if (iter == PlayerMap.end())
		return;

	CPlayer *pPlayer = iter->second;
	WORD BattleServerNo = pPlayer->BattleServerNo;
	int RoomNo = pPlayer->RoomNo;

	CBattleInfo *pBattleServer = BattleServerArr[BattleServerNo];
	CRoom *pGameRoom = &pBattleServer->Rooms[RoomNo];

	pGameRoom->MaxUser++;

	AcquireSRWLockExclusive(&PlayerLock);
	PlayerMap.erase(ClientKey);
	PlayerPool.Free(pPlayer);
	ReleaseSRWLockExclusive(&PlayerLock);
}


UINT WINAPI CMatchLanServer::hPlayerCheck(LPVOID arg)
{
	CMatchLanServer *pServer = (CMatchLanServer *)arg;

	return pServer->PlayerCheck();
}

UINT CMatchLanServer::PlayerCheck()
{
	ULONG64 SpendTick;

	while (m_bServerOpen)
	{
		Sleep(2000);

		AcquireSRWLockExclusive(&PlayerLock);
		auto iter = PlayerMap.begin();
		ReleaseSRWLockExclusive(&PlayerLock);

		while (1)
		{
			AcquireSRWLockExclusive(&PlayerLock);
			if (iter == PlayerMap.end())
			{
				ReleaseSRWLockExclusive(&PlayerLock);
				break;
			}
			ReleaseSRWLockExclusive(&PlayerLock);

			SpendTick = CUpdateTime::GetTickCount() - iter->second->TickCheck;
			if (SpendTick >= 20000)
			{
				CPlayer *pPlayer = iter->second;
			
				AcquireSRWLockExclusive(&PlayerLock);
				PlayerMap.erase(pPlayer->ClinetKey);
				PlayerPool.Free(pPlayer);
				ReleaseSRWLockExclusive(&PlayerLock);

			}
			AcquireSRWLockExclusive(&PlayerLock);
			iter++;
			ReleaseSRWLockExclusive(&PlayerLock);
		}
	}

	return 1;
}

