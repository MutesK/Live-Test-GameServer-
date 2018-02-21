#include "MasterServer.h"
#include "MatchLanServer.h"
#include "BattleLanServer.h"
#include "Engine/Parser.h"
#include "Engine/HttpPost.h"
/*
CMasterServer::CMasterServer()
{
	BattleServerIndex = 0;
	MatchServerNum = 0;
	BattleServerNum = 0;
	ActivateGameRoomTotal = 0;
	 RoomRequest = 0;
	InitializeSRWLock(&PlayerLock);
}

CMasterServer::~CMasterServer()
{
}

bool CMasterServer::Start(WCHAR *szServerConfig, bool bNagleOption)
{
	// 매치서버와 배틀서버를 실행, 마스터 토큰을 만든다.
	CParser *pParser = new CParser(szServerConfig);

	WCHAR MatchIP[16];
	int MatchPort;
	int MatchThread;
	int MatchMaxSession;

	WCHAR BattleIP[16];
	int BattlePort;
	int BattleThread;
	int BattleMaxSession;

	pParser->GetValue(L"MATCH_IP", MatchIP, L"NETWORK");
	pParser->GetValue(L"MATCH_PORT", &MatchPort, L"NETWORK");
	pParser->GetValue(L"MATCH_WORKER_THREAD", &MatchThread, L"NETWORK");
	pParser->GetValue(L"MATCH_MAX", &MatchMaxSession, L"NETWORK");

	pParser->GetValue(L"BATTLE_IP", BattleIP, L"NETWORK");
	pParser->GetValue(L"BATTLE_PORT", &BattlePort, L"NETWORK");
	pParser->GetValue(L"BATTLE_WORKER_THREAD", &BattleThread, L"NETWORK");
	pParser->GetValue(L"BATTLE_MAX", &BattleMaxSession, L"NETWORK");

	WCHAR _MasterToken[50];
	pParser->GetValue(L"TOKEN", _MasterToken, L"SYSTEM");
	UTF16toUTF8(MasterToken, _MasterToken, 32);


	pMatchServer = new CMatchLanServer(this);
	pBattleServer = new BattleLanServer(this);

	if (!pMatchServer->Start(MatchIP, MatchPort, MatchThread, MatchMaxSession, bNagleOption))
		return false;

	if (!pBattleServer->Start(BattleIP, BattlePort, BattleThread, BattleMaxSession, bNagleOption))
		return false;

	m_bServerOpen = true;
	hPlayerCheckThread = (HANDLE)_beginthreadex(nullptr, 0, hPlayerCheck, this, 0, nullptr);
}

void CMasterServer::Stop()
{
	m_bServerOpen = false;

	pMatchServer->Stop();
	pBattleServer->Stop();
}

void CMasterServer::Monitoring()
{
	wprintf(L"=======================================================================\n");
	wprintf(L"Battle Server On : %I64d\n", BattleServerNum);
	wprintf(L"Working on GameRoom Total : %I64d\n", ActivateGameRoomTotal);
	wcout << L"Packet Pool : " << CPacketBuffer::PacketPool.GetChunkSize() << L"\n";
	wcout << L"Packet Pool Use: " << CPacketBuffer::PacketPool.GetAllocCount() << L"\n\n";
	pMatchServer->Monitoring();
	pBattleServer->Monitoring();
	wprintf(L"=======================================================================\n");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 매치메이킹 서버에서 호출하는 함수
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CMasterServer::MatchServerServerOn(ULONG64 MatchSessionID, int ServerNo, char *pMasterToken)
{
	// 매치메이킹 서버가 마스터 서버에게 서버 켜짐 알림(로그인)

	if (memcmp(pMasterToken, MasterToken, 32) != 0)
		return -1;

	if (MatchServerInfo[ServerNo].Used)
		return -2;

	MatchServerInfo[ServerNo].Used = true;
	MatchServerInfo[ServerNo].ServerNo = ServerNo;
	MatchServerInfo[ServerNo].SessionID = MatchSessionID;

	MatchServerNum = _InterlockedAdd64(&MatchServerNum, 1) % MAX_BATTLESERVER;

	return true;
}

int CMasterServer::MatchServerGameRoom(ULONG64 MatchSessionID, UINT64 ClientKey, CPacketBuffer *pBuffer)
{
	// 최적의 게임방을 찾는다.
	InterlockedAdd64(&RoomRequest, 1);
	BYTE Status = 1;
	CBattleInfo *pInfo = nullptr;

	WORD Type = en_PACKET_MAT_MAS_RES_GAME_ROOM;
	*pBuffer << Type << ClientKey;
	
	for (int BattleIndex = 0; BattleIndex < MAX_BATTLESERVER; BattleIndex++)
	{
		if (!BattleServerArr[BattleIndex].isUse)
			continue;

		pInfo = &BattleServerArr[BattleIndex];
		break;
	}

	if (pInfo == nullptr)
	{
		InterlockedAdd64(&RoomRequest, -1);
		*pBuffer << Status;
		return false;
	}
	

	for (int RoomIndex = 0; RoomIndex < MAX_BATTLEROOM; RoomIndex++)
	{
		CRoom *pRoom = &pInfo->Rooms[RoomIndex];

		if (!pRoom->isUse)
			continue;

		if (pRoom->MaxUser != 0)
		{
			pRoom->MaxUser--;
			Status = 1;

			CPlayer *pPlayer = PlayerPool.Alloc();
			pPlayer->ClinetKey = ClientKey;
			pPlayer->RoomNo = pRoom->RoomNo;
			pPlayer->BattleServerNo = pInfo->BattleServerNo;
			pPlayer->TickCheck = CUpdateTime::GetTickCount();

			pair <UINT64, CPlayer *> PlayerPair = { ClientKey, pPlayer };

			AcquireSRWLockExclusive(&PlayerLock);
			PlayerMap.insert(PlayerPair);
			ReleaseSRWLockExclusive(&PlayerLock);

			*pBuffer << Status;
			*pBuffer << pInfo->BattleServerNo;;
			pBuffer->PutData((char *)pInfo->ServerIP, 32);
			*pBuffer << pInfo->Port << pRoom->RoomNo;
			pBuffer->PutData((char *)pInfo->ConnectToken, 32);
			pBuffer->PutData((char *)pRoom->EnterToken, 32);

			pBuffer->PutData((char *)pInfo->ChatServerIP, 32);
			*pBuffer << pInfo->ChatSeverPort;

			InterlockedAdd64(&RoomRequest, -1);
			return true;
		}
	}
	
	*pBuffer << Status;
	InterlockedAdd64(&RoomRequest, -1);

	return true;
}

int CMasterServer::MatchServerGameRoomEnterSuccess(ULONG64 SessionID, WORD BattleServerNo, int RoomNo, UINT64 ClientKey)
{
	auto iter = PlayerMap.find(ClientKey);
	if (iter == PlayerMap.end())
		return -1;

	CPlayer *pPlayer = iter->second;

	if (pPlayer->BattleServerNo != BattleServerNo ||
		pPlayer->RoomNo != RoomNo)
		return -1;

	CBattleInfo *pBattleServer = &BattleServerArr[BattleServerNo];
	CRoom *pGameRoom = &pBattleServer->Rooms[RoomNo];


	AcquireSRWLockExclusive(&PlayerLock);
	PlayerMap.erase(ClientKey);
	PlayerPool.Free(pPlayer);
	ReleaseSRWLockExclusive(&PlayerLock);
}

int CMasterServer::MatchServerGameRoomEnterFail(ULONG64 SessionID, UINT64 ClientKey)
{
	auto iter = PlayerMap.find(ClientKey);
	if (iter == PlayerMap.end())
		return -1;

	CPlayer *pPlayer = iter->second;
	WORD BattleServerNo = pPlayer->BattleServerNo;
	int RoomNo = pPlayer->RoomNo;

	CBattleInfo *pBattleServer = &BattleServerArr[BattleServerNo];
	CRoom *pGameRoom = &pBattleServer->Rooms[RoomNo];

	pGameRoom->MaxUser++;

	AcquireSRWLockExclusive(&PlayerLock);
	PlayerMap.erase(ClientKey);
	PlayerPool.Free(pPlayer);
	ReleaseSRWLockExclusive(&PlayerLock);
}

int CMasterServer::MatchServerDisconnect(ULONG64 MatchSessionID)
{
	for (int i = 0; i < MAX_MATCHSERVER; i++)
	{
		if (MatchServerInfo[i].SessionID == MatchSessionID)
		{
			MatchServerInfo[i].Used = false;
			_InterlockedAdd64(&MatchServerNum, -1);
			return true;
		}
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 배틀 서버에서 호출하는 함수
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CMasterServer::BattleServerServerOn(ULONG64 BattleSessionID, CPacketBuffer *pOutBuffer)
{
	//	배틀서버가 마스터 서버에게 서버 켜짐 알림
	// 락이 필요하다.
	char _MasterToken[32];
	InterlockedAdd(&BattleServerIndex, 1);

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

	for (int i = 0; i < MAX_BATTLEROOM; i++)
	{
		CRoom* pRoom = &pServer->Rooms[i];
		
		if (pRoom->isUse)
			ActivateGameRoomTotal--;

		pRoom->isUse = false;

	}

	return pServer->BattleServerNo;
}

int CMasterServer::BattleServerConnectToken(ULONG64 BattleSessionID, char *pToken)
{
	//	배틀서버의 연결토큰 재발행 알림.
	for (int i = 0; i < MAX_BATTLESERVER; i++)
	{
		if (!BattleServerArr[i].isUse)
			continue;

		if (BattleServerArr[i].OutSessionID == BattleSessionID)
		{
			memcpy(BattleServerArr[i].ConnectToken, pToken, 32);
			return true;
		}
	}

	return -1;
}

int CMasterServer::BattleServerCreateRoom(ULONG64 BattleSessionID, int BattleServerNo, int RoomNo, int MaxUser, char *pToken)
{


		// 이왕 있는김에 둘다 넣자~
	if (BattleServerArr[BattleServerNo].OutSessionID == BattleSessionID)
	{
		CRoom *pRoom = &BattleServerArr[BattleServerNo].Rooms[RoomNo];
		pRoom->isUse = false;

		_InterlockedAdd64(&ActivateGameRoomTotal, 1);

		pRoom->BattleServerNo = BattleServerNo;
		pRoom->MaxUser = MaxUser;
		pRoom->RoomNo = RoomNo;
		memcpy(pRoom->EnterToken, pToken, 32);
		pRoom->isUse = true;

		return true;
	}
	

	return -1;
}

int CMasterServer::BattleServerCloseRoom(ULONG64 BattleSessionID, int RoomNo)
{
	for (int i = 0; i < MAX_BATTLESERVER; i++)
	{
		if (!BattleServerArr[i].isUse)
			continue;

		if (BattleServerArr[i].OutSessionID == BattleSessionID)
		{
			CRoom *pRoom = &BattleServerArr[i].Rooms[RoomNo];
			_InterlockedAdd64(&ActivateGameRoomTotal, -1);

			pRoom->isUse = false;
			return true;
		}
	}

	return -1;
}

int CMasterServer::BattleServerLeftOneUser(ULONG64 BattleSessionID, int RoomNo)
{
	for (int i = 0; i < MAX_BATTLESERVER; i++)
	{
		if (!BattleServerArr[i].isUse)
			continue;

		if (BattleServerArr[i].OutSessionID == BattleSessionID)
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

int CMasterServer::BattleServerDisconnect(ULONG64 BattleSessionID)
{
	for (int i = 0; i < MAX_BATTLESERVER; i++)
	{
		if (BattleServerArr[i].OutSessionID == BattleSessionID)
		{
			if (BattleServerArr[i].isUse)
			{
				BattleServerArr[i].isUse = false;
				_InterlockedAdd64(&BattleServerNum, -1);

				for (int j = 0; j < MAX_BATTLEROOM; j++)
				{
					CRoom *pRoom = &BattleServerArr[i].Rooms[j];

					if (pRoom->isUse)
						ActivateGameRoomTotal--;

					pRoom->isUse = false;
				}
			}
			return true;
		}
	}

	return false;
}

*/