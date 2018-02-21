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
	// �÷��̰� �غ�� ���� �ִٸ� �÷��� ���
	Auth_ReadyToPlayRoom();

	Auth_CheckLeaveRoom();
	// �÷��� ������� ���� �ִٸ� �÷��� ����
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
			// �� �����Ѵ�.
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
			// ���� �غ��ϰԲ���.
			//GameRoom--;
			InterlockedAdd64(&GameRoom, -1);

			pRoom->RoomStatus = eNOT_USE;
			//	��Ʋ ������ �� ������ ä�ü������� �˸� 
			ChatServerDestoryRoom(pRoom);
		}
	}
}

void	CBattleServer::ReqLogin(CPlayer *pPlayer)
{
	//	��Ʋ������ �α��� �� ������� SessionKey Ȯ�ΰ� ConnectToken �� Ȯ�� �Ѵ�.
	//	SessionKey �� ��Ʋ������ shDB �� ���� Ȯ���ϴ� ������ �δ㽺���� �� ������ ����� ������ �ʿ��ϹǷ� ��¿ �� ����
	//
	//	SessionKey �� ��Ī���� -> �����ͼ��� -> ��Ʋ���� ��η� �����Ͽ�  ��Ʋ������ shDB ��ȸ�ϴ� �κ��� ���� �� �� �ְ�����
	//	�̰� ���� �� �δ�Ǹ�, ��Ʋ������ ������ �г��� �� �������� Ȯ���� ���ؼ� shDB �� ��ȸ �ؾ� ��.
	//
	//	��Ʋ������ �α��ο�û ���Ž� shDB ������ ����Ű Ȯ�� / �������� ȹ�� / ConnectToken Ȯ�� (AUTH ������)
	//	������ �� ���� �� �����÷��� ��ȯ �� ������ ��� AUTH �����忡 �ӹ����� �Ѵ�.

	// ����Ű Ȯ��
	if (Login_SessionKeyCheck(pPlayer) != true)
	{
		ResLogin(pPlayer, 3);
		return;
	}
	// ConnectToken Ȯ��
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
		// ����
		ResLogin(pPlayer, 1);
	}

}

void	CBattleServer::ReqEnterRoom(CPlayer *pPlayer)
{
	//	��Ʋ������ Ư�� �濡 ������ ��û�Ѵ�.
	//	������� AccountNo �� ���� ���� �� �׽�Ʈ�� ���ؼ� ���� ������ ��.
	//
	//	��Ʋ���������� � �濡 � ������ �������� ���� ���ϴ� �����̹Ƿ�
	//	EnterToken �� ��ġ �Ѵٸ� ������ ��� �Ѵ�.

	BYTE Result = 1;

	// �ش���� ���� üũ
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

	// �̹� �����ѻ���ΰ�?
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
		// �ش� ���� �ο� ���� ó���Ѵ�.
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
	// ���� ��Ŷ ����
	ResEnterRoom(pPlayer, Result);

	if (Result == 1)
	{
		// ��Ʋ ���� �濡 ������ �߰��� ��Ŷ�� �̹� ������ �÷��̾�� ������.

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
	// shDB���� Ȯ���Ѵ�.
	WCHAR PathURL[200];
	BYTE Result;
	StringCchPrintf(PathURL, 200, L"%s%s", httpPath, L"select_account.php");

	// AccountNo�� JSON���� ��� ���ߵȴ�.
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
		// ���� �α� & ��Ÿ ����
		Result = 3;

		SYSLOG(L"LOGIN", LOG_ERROR, L"Request Login, HTTP POST ERROR. Result Code : %d", ret);
		//ResLogin(OutSessionID, Result);
		CCrashDump::Crash();
		return Result;
	}

	// RecvDATA�� RapidJSON�� �̿��ؼ� ���� ���´�.
	Document document;
	document.Parse(RecvDATA);

	ResultCode = document["result"].GetInt();

	if (ResultCode == -10)
	{
		// AccountNo ����
		Result = 3;
		//ResLogin(OutSessionID, Result);
		return Result;
	}

	if (ResultCode != 1)
	{
		// ��Ÿ ����
		Result = 4;
		//ResLogin(OutSessionID, Result);
		SYSLOG(L"LOGIN", LOG_ERROR, L"SHDB LogDB ETC Error");
		SYSLOG(L"LOGIN", LOG_ERROR, L"%s", RecvDATA);

		return Result;
	}

	if (memcmp(pPlayer->SessionKey, document["sessionkey"].GetString(), 64) != 0)
	{
		// ����Ű ����
		Result = 2;
		//ResLogin(OutSessionID, Result);
		return Result;
	}

	// �г��� �����Ѵ�.
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

		// �ش� ���� �ο��� ������, Open_Mode��� Close Mode�� ��ȯ�Ѵ�.
		if (pRoom->RoomStatus == eOPEN_MODE && pRoom->MaxUser == 0)
		{
			InterlockedAdd64(&OpenRoom, -1);
			InterlockedAdd64(&ReadyRoom, 1);

			//OpenRoom--;
			//ReadyRoom++;

			pRoom->RoomStatus = eCLOSE_MODE;
			// �ش� ���� �ο����� ���� ��Ŷ ����
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
			// �ش� ���� �ο����� ���� ��Ŷ ���۰� �÷��� ���� ��ȯ
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

