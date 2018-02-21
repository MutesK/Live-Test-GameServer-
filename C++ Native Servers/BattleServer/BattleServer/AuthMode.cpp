#include "BattleServer.h"
#include "Engine/MMOServer.h"
#include "Player.h"
#include "MasterLanClient.h"
#include "ChatLanServer.h"
#include "Engine/Parser.h"
#include "Engine/HttpPost.h"
#include "Engine/rapidjson\document.h"
#include "Engine/rapidjson\writer.h"
#include "Engine/rapidjson\stringbuffer.h"
#include "Engine/HttpPost.h"
using namespace rapidjson;

void	CBattleServer::OnAuth_Update(void)
{
	Auth_MakeRoom();

	Auth_CheckLeaveRoom();
	// 플레이가 준비된 방이 있다면 플레이 대기
	Auth_ReadyToPlayRoom();

	Auth_CheckLeaveRoom();
	// 플레이 대기중인 방이 있다면 플레이 시작
	Auth_PlayRoom();

}

void CBattleServer::Auth_MakeRoom()
{
	bool isMaked = false;

	static ULONGLONG TickOpen = CUpdateTime::GetTickCount();

	if (CUpdateTime::GetTickCount() - TickOpen < 50)
		return;

	TickOpen = CUpdateTime::GetTickCount();

	if (_ChatServer.isLogin && _MasterServer.isLogin)
	{
		if (OpenRoom < MAX_BATTLEROOM)
		{
			// 방 생성한다.
			int RoomIndex = 0;

			for (; RoomIndex < MAX_BATTLEROOM; RoomIndex++)
			{
				if (!isMaked && Rooms[RoomIndex].RoomStatus == eNOT_USE)
				{
					Rooms[RoomIndex].RoomStatus = eOPEN_MODE;
					RoomEnterToken_RamdomGenerator(&Rooms[RoomIndex]);
					Rooms[RoomIndex].MaxUser = MAX_USER;
					Rooms[RoomIndex].RoomNo = RoomIndex;
					Rooms[RoomIndex].GameIndex = 0;

					ChatServerCreateRoom(&Rooms[RoomIndex]);
					MasterServerCreateRoom(&Rooms[RoomIndex]);

					RoomIndex = (RoomIndex++) % MAX_BATTLEROOM;

					InterlockedAdd64(&OpenRoom, 1);
					//OpenRoom++;
					isMaked = true;
					break;
				}
			}

		}

	}
}

void CBattleServer::Auth_CheckLeaveRoom()
{
	for (int RoomIndex = 0; RoomIndex < MAX_BATTLEROOM; RoomIndex++)
	{
		CRoom *pRoom = &Rooms[RoomIndex];

		if (pRoom->RoomStatus != eGAME_MODE)
			continue;

		if (pRoom->PlayerMap.size() == 0)
		{
			// 생성 준비하게끔함.
			//GameRoom--;
			InterlockedAdd64(&GameRoom, -1);

			pRoom->RoomStatus = eNOT_USE;
			//	배틀 서버의 방 삭제를 채팅서버에게 알림 
			ChatServerDestoryRoom(pRoom);
		}
	}
}

void	CBattleServer::ReqLogin(CPlayer *pPlayer)
{
	//	배틀서버로 로그인 시 사용자의 SessionKey 확인과 ConnectToken 을 확인 한다.
	//	SessionKey 를 배틀서버가 shDB 를 통해 확인하는 과정이 부담스러울 수 있으나 사용자 인증이 필요하므로 어쩔 수 업음
	//
	//	SessionKey 를 매칭서버 -> 마스터서버 -> 배틀서버 경로로 전달하여  배틀서버가 shDB 조회하는 부분을 줄일 수 는 있겠으나
	//	이것 또한 더 부담되며, 배틀서버는 어차피 닉네임 등 계정정보 확인을 위해서 shDB 를 조회 해아 함.
	//
	//	배틀서버는 로그인요청 수신시 shDB 를통해 세션키 확인 / 계정정보 획득 / ConnectToken 확인 (AUTH 스레드)
	//	유저가 방 입장 후 게임플레이 전환 전 까지는 계속 AUTH 스레드에 머물도록 한다.

	// 세션키 확인
	if (Login_SessionKeyCheck(pPlayer) != true)
	{
		ResLogin(pPlayer, 3);
		return;
	}
	// ConnectToken 확인
	else if (memcmp(ConnectToken, pPlayer->ConnectToken, 32) != 0)
	{
		ResLogin(pPlayer, 2);
		return;
	}
	else if (pPlayer->VerCode != Ver_Code)
	{
		ResLogin(pPlayer, 5);
		return;
	}
	else
	{
		// 성공
		ResLogin(pPlayer, 1);
	}

}

void	CBattleServer::ReqEnterRoom(CPlayer *pPlayer)
{
	//	배틀서버의 특정 방에 입장을 요청한다.
	//	사용자의 AccountNo 는 버그 감지 및 테스트를 위해서 넣은 데이터 임.
	//
	//	배틀서버에서는 어떤 방에 어떤 유저가 들어올지는 알지 못하는 상태이므로
	//	EnterToken 만 일치 한다면 입장을 허용 한다.

	BYTE Result = 1;

	// 해당방의 상태 체크
	if (pPlayer->RoomNo < 0 || pPlayer->RoomNo >= MAX_BATTLEROOM)
		Result = 4;
	else if (Rooms[pPlayer->RoomNo].RoomStatus == eGAME_MODE ||
		Rooms[pPlayer->RoomNo].RoomStatus == eCLOSE_MODE ||
		Rooms[pPlayer->RoomNo].RoomStatus == eNOT_USE)
		Result = 3;
	else if (Rooms[pPlayer->RoomNo].MaxUser == 0)
		Result = 5;
	else if (memcmp(Rooms[pPlayer->RoomNo].EnterToken, pPlayer->EnterToken, 32) != 0)
		Result = 2;

	// 이미 접속한사람인가?
	for (int RoomIndex = 0; RoomIndex < MAX_BATTLEROOM; RoomIndex++)
	{
		CRoom &Room = Rooms[RoomIndex];

		Room.ReadLock();
		auto finditer = Room.PlayerMap.find(pPlayer->AccountNo);
		if (finditer != Room.PlayerMap.end())
		{
			Room.ReadUnLock();
			Result = 2;
			SYSLOG(L"SYSTEM", LOG_SYSTEM, L"Duplicate Connection: AccountNo : %I64d, RoomNo : %d, Connector IP : %s",
				pPlayer->AccountNo, pPlayer->RoomNo, pPlayer->_ClientInfo._IP);

			break;
		}
		Room.ReadUnLock();

	}

	if (Result == 1)
	{
		// 해당 방의 인원 입장 처리한다.
		Rooms[pPlayer->RoomNo].Lock();
		pair<INT64, CPlayer *> pairs = { pPlayer->AccountNo, pPlayer };
		auto result = Rooms[pPlayer->RoomNo].PlayerMap.insert(pairs);
		if (!result.second)
			SYSLOG(L"ROOM", LOG_ERROR, L"AccountNo : %d is Already exist in Rooms[%d]", pPlayer->AccountNo, pPlayer->RoomNo);
		//Rooms[pPlayer->RoomNo].PlayerList.push_back(pPlayer);
		Rooms[pPlayer->RoomNo].MaxUser--;
		Rooms[pPlayer->RoomNo].Unlock();
	}
	else
	{
		SYSLOG(L"ROOM", LOG_ERROR, L"Enter ROOM Error / Result : %d, Rooms Status : %d, Rooms MaxUser : %d, Rooms EnterToken : %s, Player EnterToken : %s",
			Result, Rooms[pPlayer->RoomNo].RoomStatus, Rooms[pPlayer->RoomNo].MaxUser, Rooms[pPlayer->RoomNo].EnterToken, pPlayer->EnterToken);
	}
	// 응답 패킷 전송
	ResEnterRoom(pPlayer, Result);

	if (Result == 1)
	{
		// 배틀 서버 방에 유저가 추가됨 패킷을 이미 입장한 플레이어에게 보낸다.

		ResAddUser(pPlayer);
		ResAddOtherUser(pPlayer);
	}
}

void CBattleServer::ResAddUser(CPlayer *pPlayer)
{
	std::map<INT64, CPlayer *> *pPlayerMap = &Rooms[pPlayer->RoomNo].PlayerMap;

	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();
	
	WORD Type = en_PACKET_CS_GAME_RES_ADD_USER;

	*pBuffer << Type << pPlayer->RoomNo << pPlayer->AccountNo;
	pBuffer->PutData(reinterpret_cast<char *>(pPlayer->NickName), 40);

	SendPacketAllInRoom(pBuffer, pPlayer);
}

void	CBattleServer::ResAddOtherUser(CPlayer *pResPlayer)
{
	std::map<INT64, CPlayer *> *pPlayerMap = &Rooms[pResPlayer->RoomNo].PlayerMap;

	Rooms[pResPlayer->RoomNo].Lock();
	for (pair<INT64, CPlayer *> iter : *pPlayerMap)
	{
		CPlayer *pPlayer = iter.second;

		if (pPlayer == pResPlayer)
			continue;

		CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

		WORD Type = en_PACKET_CS_GAME_RES_ADD_USER;
		*pBuffer << Type << pPlayer->RoomNo << pPlayer->AccountNo;
		pBuffer->PutData(reinterpret_cast<char *>(pPlayer->NickName), 40);

		if (!pResPlayer->SendPacket(pBuffer))
			pBuffer->Free();
	}
	Rooms[pResPlayer->RoomNo].Unlock();
}

BYTE CBattleServer::Login_SessionKeyCheck(CPlayer *pPlayer)
{
	// shDB에서 확인한다.
	WCHAR PathURL[200];
	BYTE Result;
	StringCchPrintf(PathURL, 200, L"%s%s", httpPath, L"select_account.php");

	// AccountNo를 JSON으로 묶어서 쏴야된다.
	char PostDATA[100];
	char RecvDATA[1400] = "";

	sprintf_s(PostDATA, 100, "{\"accountno\":%I64d}", pPlayer->AccountNo);

	int retry = 10000;
	int ret = -1;
	int ResultCode = 0;
	PRO_BEGIN(L"HTTP POST");
	while (1)
	{
		ret = HTTP_POST(httpURL, PathURL, PostDATA, RecvDATA);

		if (ret == 200 || retry == 0)
			break;

		retry--;
	}
	PRO_END(L"HTTP POST");

	if (retry == 0 || ret == -1)
	{
		// 에러 로그 & 기타 오류
		Result = 3;

		SYSLOG(L"LOGIN", LOG_ERROR, L"Request Login, HTTP POST ERROR. Result Code : %d", ret);
		//ResLogin(OutSessionID, Result);
		CCrashDump::Crash();
		return Result;
	}

	// RecvDATA를 RapidJSON을 이용해서 값을 얻어온다.
	Document document;
	document.Parse(RecvDATA);

	ResultCode = document["result"].GetInt();

	if (ResultCode == -10)
	{
		// AccountNo 없음
		Result = 3;
		//ResLogin(OutSessionID, Result);
		return Result;
	}

	if (ResultCode != 1)
	{
		// 기타 오류
		Result = 4;
		//ResLogin(OutSessionID, Result);
		SYSLOG(L"LOGIN", LOG_ERROR, L"SHDB LogDB ETC Error");
		SYSLOG(L"LOGIN", LOG_ERROR, L"%s", RecvDATA);

		return Result;
	}

	if (memcmp(pPlayer->SessionKey, document["sessionkey"].GetString(), 64) != 0)
	{
		// 세션키 오류
		Result = 2;
		//ResLogin(OutSessionID, Result);
		return Result;
	}

	// 닉네임 저장한다.
	char nickname[20];
	strcpy_s(nickname, 20, document["nick"].GetString());
	UTF8toUTF16(nickname, pPlayer->NickName, 20);

	return true;
}

void	CBattleServer::Auth_ReadyToPlayRoom()
{
	for (int i = 0; i < MAX_BATTLEROOM; i++)
	{
		CRoom *pRoom = &Rooms[i];

		// 해당 방의 인원이 꽉차고, Open_Mode라면 Close Mode로 전환한다.
		if (pRoom->RoomStatus == eOPEN_MODE && pRoom->MaxUser == 0)
		{
			InterlockedAdd64(&OpenRoom, -1);
			InterlockedAdd64(&ReadyRoom, 1);

			//OpenRoom--;
			//ReadyRoom++;

			pRoom->RoomStatus = eCLOSE_MODE;
			// 해당 방의 인원에게 전부 패킷 전송
			ResReadyToPlayRoom(i);

			MasterServerClosedRoom(pRoom);
			pRoom->ReadySec = CUpdateTime::GetTickCount();
		}
	}
}

void	CBattleServer::Auth_PlayRoom()
{
	ULONGLONG NowTick = CUpdateTime::GetTickCount();

	for (int i = 0; i < MAX_BATTLEROOM; i++)
	{
		CRoom *pRoom = &Rooms[i];

		if (pRoom->RoomStatus == eCLOSE_MODE && NowTick - pRoom->ReadySec >= READY_SEC)
		{
			// 해당 방의 인원에게 전부 패킷 전송과 플레이 상태 전환
			Auth_ChangeUserGameMode(i);
			InterlockedAdd64(&ReadyRoom, -1);
			InterlockedAdd64(&GameRoom, 1);
			//ReadyRoom--;
			//GameRoom++;
			pRoom->RoomStatus = eGAME_MODE;
		}
	}

}

void	CBattleServer::ResLogin(CPlayer *pPlayer, BYTE Result)
{
	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

	WORD Type = en_PACKET_CS_GAME_RES_LOGIN;
	*pBuffer << Type << pPlayer->AccountNo << Result;

	if (!pPlayer->SendPacket(pBuffer))
	{
		SYSLOG(L"SYSTEM", LOG_SYSTEM, L"ResLogin Packet Send Failed");
		pBuffer->Free();
	}
}

void	CBattleServer::ResEnterRoom(CPlayer *pPlayer, BYTE Result)
{
	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

	WORD Type = en_PACKET_CS_GAME_RES_ENTER_ROOM;

	*pBuffer << Type << pPlayer->AccountNo << pPlayer->RoomNo << Result;

	if (!pPlayer->SendPacket(pBuffer))
	{
		SYSLOG(L"SYSTEM", LOG_SYSTEM, L"ResLoginEnter Packet Send Failed");
		pBuffer->Free();
	}
}

void	CBattleServer::ResReadyToPlayRoom(int RoomIndex)
{
	CRoom *pRoom = &Rooms[RoomIndex];

	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

	WORD Type = en_PACKET_CS_GAME_RES_PLAY_READY;
	BYTE ReadySec = READY_SEC;

	*pBuffer << Type << RoomIndex << ReadySec;

	SendPacketAllInRoom(pBuffer, pRoom);
}

void CBattleServer::Auth_ChangeUserGameMode(int RoomIndex)
{
	CRoom *pRoom = &Rooms[RoomIndex];

	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

	WORD Type = en_PACKET_CS_GAME_RES_PLAY_START;

	*pBuffer << Type << RoomIndex;

	pRoom->ReadLock();
	for (auto iter = pRoom->PlayerMap.begin(); iter != pRoom->PlayerMap.end(); ++iter)
	{
		CPlayer *pPlayer = iter->second;

		if (pPlayer->AccountNo != iter->first)
			continue;

		pBuffer->AddRefCount();
		if (!pPlayer->SendPacket(pBuffer))
			pBuffer->Free();

		pPlayer->SetMode_GAME();
		pPlayer->LastTick = CUpdateTime::GetTickCount();

		pPlayer->ProcessType = en_PACKET_CS_GAME_RES_CREATE_MY_CHARACTER;
	}
	pRoom->ReadUnLock();

	pBuffer->Free();
}

void CBattleServer::DisconnectedPlayer(CPlayer *pPlayer)
{
	if (pPlayer->RoomNo >= 0 && pPlayer->RoomNo < MAX_BATTLEROOM)
	{
		CRoom *pRoom = &Rooms[pPlayer->RoomNo];

		pRoom->Lock();

		pRoom->MaxUser++;
		pRoom->PlayerMap.erase(pPlayer->AccountNo);
		pRoom->Unlock();

		if (pPlayer->GetMode() == MODE_AUTH)
			ResAuthRemoveUser(pPlayer);
		else
			ResGameRemoveUser(pPlayer);

		MasterServerLeftUser(pRoom);
	}
}


void    CBattleServer::ResAuthRemoveUser(CPlayer *pPlayer)
{
	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

	WORD Type = en_PACKET_CS_GAME_RES_REMOVE_USER;
	*pBuffer << Type << pPlayer->RoomNo << pPlayer->AccountNo;

	SendPacketAllInRoom(pBuffer, &Rooms[pPlayer->RoomNo]);
}

