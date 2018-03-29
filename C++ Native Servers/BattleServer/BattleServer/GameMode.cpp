#include "BattleServer.h"
#include "Engine/MMOServer.h"
#include "Player.h"
#include "MasterLanClient.h"
#include "ChatLanServer.h"
#include "Engine/Parser.h"
#include "Engine/HttpPost.h"

// GAME ��� ������Ʈ �̺�Ʈ ����ó����
// ���� ���� ���� ó���Ѵ�.

void	CBattleServer::OnGame_Update(void)
{
	Game_PlayerCheck();

	Game_RoomCheck();
}

void	CBattleServer::Game_PlayerCheck()
{
	for (int i = 0; i < MaxSession; i++)
	{
		CPlayer *pPlayer = &Players[i];

		if (pPlayer->GetMode() != MODE_GAME)
			continue;

		switch (pPlayer->ProcessType)
		{
		case en_PACKET_CS_GAME_RES_CREATE_MY_CHARACTER:
			pPlayer->ProcessType = 0;
			Game_CreateCharacter(pPlayer);
			break;
		case en_PACKET_CS_GAME_REQ_MOVE_PLAYER:
			pPlayer->ProcessType = 0;
			Game_MoveCharacter(pPlayer);
			break;
		case en_PACKET_CS_GAME_REQ_HIT_POINT:
			pPlayer->ProcessType = 0;
			Game_HitPoint(pPlayer);
			break;
		case en_PACKET_CS_GAME_REQ_FIRE1:
			pPlayer->ProcessType = 0;
			Game_Fire1(pPlayer);
			break;
		case en_PACKET_CS_GAME_REQ_RELOAD:
			pPlayer->ProcessType = 0;
			Game_Reload(pPlayer);
			break;
		case en_PACKET_CS_GAME_REQ_HIT_DAMAGE:
			pPlayer->ProcessType = 0;
			Game_HitDamage(pPlayer);
			break;
		case en_PACKET_CS_GAME_REQ_MEDKIT_GET:
			pPlayer->ProcessType = 0;
			Game_MedkitGet(pPlayer);
			break;
		}
	}
}

void	CBattleServer::Game_RoomCheck()
{
	for (int index = 0; index < MAX_BATTLEROOM; index++)
	{
		CRoom& Room = Rooms[index];

		if (Room.RoomStatus == eGAME_MODE)
		{

			// �����ڰ� 1���̶��,
			if (Room.SurvivalUser <= 1 && Room.PlayerMap.size() > 0)
			{
				// �����ڿ��� Winner, 
				// ������ �����ڿ��Դ� GameOver ��Ŷ
				Game_Result(Room);
			}

			if (Room.RedZoneTick == 0)
			{
				Room.RedZoneTick = CUpdateTime::GetTickCount();
				continue;
			}

			// ������ üũ �� �ߵ�
			if (!Room.RedZoneAlertFlag &&
				CUpdateTime::GetTickCount() - Room.RedZoneTick >= 20000)
			{
				Game_RedZoneAlert(Room);
			}
			else if (Room.RedZoneAlertFlag &&
				CUpdateTime::GetTickCount() - Room.RedZoneTick >= 40000) // ���ο� ������
			{
				Game_RedZone(Room);
				Room.RedZoneAlertFlag = false;
				Room.RedZoneTick = 0;
				Room.RedZoneDemageTick = CUpdateTime::GetTickCount();
			}

			if (CUpdateTime::GetTickCount() - Room.RedZoneDemageTick >= 1000)
			{
				Game_RedZonePlayerCheck(Room);
				Room.RedZoneDemageTick = CUpdateTime::GetTickCount();
			}
		}

		else if (Room.RoomStatus == eFINISH_GAMEMODE &&
			CUpdateTime::GetTickCount() - Room.ReadySec >= 5000)
		{
			Game_DestoryRoom(Room);
			Room.RoomStatus = eNOT_USE;
			InterlockedAdd64(&GameRoom, -1);
		}
	}
}

void	CBattleServer::Game_DestoryRoom(CRoom &Room)
{
	map<INT64, CPlayer *> &PlayerMap = Room.PlayerMap;

	Room.Lock();
	for (auto iter = PlayerMap.begin(); iter != PlayerMap.end(); ++iter)
	{
		CPlayer *pPlayer = iter->second;

		CPacketBuffer *pBuffer = CPacketBuffer::Alloc();
		WORD Type = en_PACKET_CS_GAME_RES_RECORD;

		*pBuffer << Type << pPlayer->Record_PlayCount << pPlayer->PlayTime <<
			pPlayer->Record_Kill << pPlayer->Record_Die << pPlayer->Record_Win;

		if (!pPlayer->SendPacket(pBuffer))
			pBuffer->Free();
	}
	Room.Unlock();

	Room.Lock();
	for (auto iter = PlayerMap.begin(); iter != PlayerMap.end(); ++iter)
	{
		CPlayer *pPlayer = iter->second;
		pPlayer->Disconnect(1);
	}
	Room.Unlock();
}

void	CBattleServer::Game_RedZoneAlert(CRoom& Room)
{
	bool bLeft = (Room.RedZonePosition & 0x80) >> 7;
	bool bTop = (Room.RedZonePosition & 0x40) >> 6;
	bool bRight = (Room.RedZonePosition & 0x20) >> 5;
	bool bBottom = (Room.RedZonePosition & 0x10) >> 4;


	// �� �� �� ���� ������ �κ���?

	// �������� ������ �κ��߿� �����Ѵ�.
	vector<BYTE> RandomSelect;

	if (!bLeft)
	{
		BYTE Left = LEFT;
		RandomSelect.push_back(Left);
	}

	if (!bTop)
	{
		BYTE Top = TOP;
		RandomSelect.push_back(Top);
	}

	if (!bRight)
	{
		BYTE Right = RIGHT;
		RandomSelect.push_back(Right);
	}

	if(!bBottom)
	{ 
	BYTE Bottom = BOTTOM;
	RandomSelect.push_back(Bottom);
	}

	int index = RandomSelect.size() - 1;
	if (index == 0)
		return;

	srand(time(NULL));
	int RandomIndex = rand() % index;

	// Alert ��Ŷ
	Room.RedZoneAlertFlag = true;

	// ���õ� ��ġ�� ���� ��Ŷ �����Ѵ�.
	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();
	WORD Type;
	switch (RandomSelect[RandomIndex])
	{
	case LEFT:
		Type = en_PACKET_CS_GAME_RES_REDZONE_ALERT_LEFT;
		break;
	case TOP:
		Type = en_PACKET_CS_GAME_RES_REDZONE_ALERT_TOP;
		break;
	case RIGHT:
		Type = en_PACKET_CS_GAME_RES_REDZONE_ALERT_RIGHT;
		break;
	case BOTTOM:
		Type = en_PACKET_CS_GAME_RES_REDZONE_ALERT_BOTTOM;
		break;
	}
	BYTE AlertTimeSec = 20;
	*pBuffer << Type << AlertTimeSec;

	SendPacketAllInRoom(pBuffer, &Room);
	
	Room.RedZoneAlertPosition = RandomSelect[RandomIndex];
}

void	CBattleServer::Game_RedZone(CRoom& Room)
{
	// ���ӽ��� �� 40�ʸ��� �������� 1���� Ȱ��ȭ�ȴ�.
	// Ȱ��ȭ �Ǳ� 20������ ��� ���ش�. -> RedZone ��� ��Ŷ ����
	// Ȱ��ȭ�Ǳ� �����ϸ� (Ȱ��ȭ ��Ŷ), 1�ʸ��� 1�������� ���δ�.

	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

	WORD Type;
	switch (Room.RedZoneAlertPosition)
	{
	case TOP:
		Type = en_PACKET_CS_GAME_RES_REDZONE_ACTIVE_TOP;
		Room.RedZonePosition |= 0x40;
		break;
	case LEFT:
		Type = en_PACKET_CS_GAME_RES_REDZONE_ACTIVE_LEFT;
		Room.RedZonePosition |= 0x80;
		break;
	case RIGHT:
		Type = en_PACKET_CS_GAME_RES_REDZONE_ACTIVE_RIGHT;
		Room.RedZonePosition |= 0x20;
		break;
	case BOTTOM:
		Type = en_PACKET_CS_GAME_RES_REDZONE_ACTIVE_BOTTOM;
		Room.RedZonePosition |= 0x10;
		break;
	}


	*pBuffer << Type;
	SendPacketAllInRoom(pBuffer, &Room);

	

	Room.RedZoneTick = CUpdateTime::GetTickCount();
}

void	CBattleServer::Game_RedZonePlayerCheck(CRoom& Room)
{
	bool Left = (Room.RedZonePosition & 0x80) >> 7;
	bool Top = (Room.RedZonePosition & 0x40) >> 6;
	bool Right = (Room.RedZonePosition & 0x20) >> 5;
	bool Bottom = (Room.RedZonePosition & 0x10) >> 4;

	if (!Left && !Top && !Right && !Bottom)
		return;

	map<INT64, CPlayer *> &PlayerMap = Room.PlayerMap;

	Room.Lock();
	for (auto iter = PlayerMap.begin(); iter != PlayerMap.end(); ++iter)
	{
		CPlayer *pPlayer = iter->second;

		if (pPlayer->Die)
			continue;

		if (Left)
		{
			if (pPlayer->MoveTargetX >= 0 && pPlayer->MoveTargetX <= 153 &&
				pPlayer->MoveTargetZ >= 0 && pPlayer->MoveTargetZ <= 50)
			{
				pPlayer->HP--;

				Room.Unlock();
				if (pPlayer->HP <= 0)
					Game_PlayerDie(pPlayer);
				else
					Game_PlayerRedZoneDamage(pPlayer);
				Room.Lock();
			}
		}
		if (Top)
		{
			if (pPlayer->MoveTargetX >= 0 && pPlayer->MoveTargetX <= 44 &&
				pPlayer->MoveTargetZ >= 0 && pPlayer->MoveTargetZ <= 170)
			{
				pPlayer->HP--;

				Room.Unlock();
				if (pPlayer->HP <= 0)
					Game_PlayerDie(pPlayer);
				else
					Game_PlayerRedZoneDamage(pPlayer);
				Room.Lock();
			}
		}
		if (Right)
		{
			if (pPlayer->MoveTargetX >= 0 && pPlayer->MoveTargetX <= 153 &&
				pPlayer->MoveTargetZ >= 115 && pPlayer->MoveTargetZ <= 170)
			{
				pPlayer->HP--;

				Room.Unlock();
				if (pPlayer->HP <= 0)
					Game_PlayerDie(pPlayer);
				else
					Game_PlayerRedZoneDamage(pPlayer);
				Room.Lock();
			}
		}
		if (Bottom)
		{
			if (pPlayer->MoveTargetX >= 102 && pPlayer->MoveTargetX <= 153 &&
				pPlayer->MoveTargetZ >= 0 && pPlayer->MoveTargetZ <= 170)
			{
				pPlayer->HP--;

				Room.Unlock();
				if (pPlayer->HP <= 0)
					Game_PlayerDie(pPlayer);
				else
					Game_PlayerRedZoneDamage(pPlayer);

			}
		}
	}
	Room.Unlock();

	Room.RedZoneDemageTick = CUpdateTime::GetTickCount();
}
void	CBattleServer::Game_Result(CRoom& Room)
{
	CPacketBuffer *pWinner = CPacketBuffer::Alloc();
	WORD WinnerType = en_PACKET_CS_GAME_RES_WINNER;

	*pWinner << WinnerType;

	CPacketBuffer *pGameOver = CPacketBuffer::Alloc();
	WORD OverType = en_PACKET_CS_GAME_RES_GAMEOVER;

	*pGameOver << OverType;
	
	map<INT64, CPlayer *> &PlayerMap = Room.PlayerMap;

	Room.Lock();
	for (auto iter = PlayerMap.begin(); iter != PlayerMap.end(); ++iter)
	{
		CPlayer *pPlayer = iter->second;

		pPlayer->Record_PlayCount++;
		pPlayer->PlayTime = CUpdateTime::GetTickCount() - pPlayer->PlayTime / 1000;
		pPlayer->Record_PlayTime += pPlayer->PlayTime;
		
		if (pPlayer->Die)
		{
			pGameOver->AddRefCount();
			if (!pPlayer->SendPacket(pGameOver))
				pGameOver->Free();

			pPlayer->Record_Die++;
		}
		else
		{
			if (!pPlayer->SendPacket(pWinner))
				pGameOver->Free();

			pPlayer->Record_Win++;
		}
	}
	Room.Unlock();

	//////////////////////////////////////////////////////////////////////////////////////////////////////

	pGameOver->Free();

	// ���� �ı���Ų��.
	Room.RoomStatus = eFINISH_GAMEMODE;
	Room.ReadySec = CUpdateTime::GetTickCount();
}

void	CBattleServer::Game_CreateCharacter(CPlayer *pPlayer)
{
	// ���� ��ġ �ε���
	int index = Rooms[pPlayer->RoomNo].GameIndex;
	Rooms[pPlayer->RoomNo].GameIndex++;

	pPlayer->MoveTargetX = g_Data_Position[index][0];
	pPlayer->MoveTargetY = g_Data_Position[index][1];
	pPlayer->HP = g_Data_HP;

	ResCreateMyCharacter(pPlayer);
	ResCreateOtherCharacter(pPlayer);
}


void	CBattleServer::ResCreateMyCharacter(CPlayer *pPlayer)
{
	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

	WORD Type = en_PACKET_CS_GAME_RES_CREATE_MY_CHARACTER;

	*pBuffer << Type << pPlayer->MoveTargetX << pPlayer->MoveTargetY << pPlayer->HP;

	if (!pPlayer->SendPacket(pBuffer))
		pBuffer->Free();
}


void CBattleServer::ResCreateOtherCharacter(CPlayer *pPlayer)
{
	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

	WORD Type = en_PACKET_CS_GAME_RES_CREATE_OTHER_CHARACTER;

	*pBuffer << Type << pPlayer->AccountNo;
	pBuffer->PutData(reinterpret_cast<char *>(pPlayer->NickName), 40);
	*pBuffer << pPlayer->MoveTargetX << pPlayer->MoveTargetY << pPlayer->HP;

	SendPacketAllInRoom(pBuffer, pPlayer, true);

}


void    CBattleServer::ResGameRemoveUser(CPlayer *pPlayer)
{
	if (!pPlayer->Die)
	{
		CRoom &Room = Rooms[pPlayer->RoomNo];
		Room.SurvivalUser--;
	}

	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

	WORD Type = en_PACKET_CS_GAME_RES_LEAVE_USER;
	*pBuffer << Type << pPlayer->RoomNo << pPlayer->AccountNo;

	SendPacketAllInRoom(pBuffer, &Rooms[pPlayer->RoomNo]);
}

void	CBattleServer::Game_MoveCharacter(CPlayer *pPlayer)
{
	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

	WORD Type = en_PACKET_CS_GAME_RES_MOVE_PLAYER;

	*pBuffer << Type << pPlayer->AccountNo;
	*pBuffer << pPlayer->MoveTargetX << pPlayer->MoveTargetY << pPlayer->MoveTargetZ <<
		pPlayer->HitPointX << pPlayer->HitPointY << pPlayer->HitPointZ;


	SendPacketAllInRoom(pBuffer, pPlayer, true);
}

void	CBattleServer::Game_HitPoint(CPlayer *pPlayer)
{
	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

	WORD Type = en_PACKET_CS_GAME_RES_HIT_POINT;

	*pBuffer << Type << pPlayer->AccountNo << pPlayer->HitPointX << pPlayer->HitPointY
		<< pPlayer->HitPointZ;

	SendPacketAllInRoom(pBuffer, pPlayer, true);
}


void	CBattleServer::Game_Fire1(CPlayer *pPlayer)
{
	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

	WORD Type = en_PACKET_CS_GAME_RES_FIRE1;

	*pBuffer << Type << pPlayer->AccountNo << pPlayer->HitPointX << pPlayer->HitPointY << pPlayer->HitPointZ;

	SendPacketAllInRoom(pBuffer, pPlayer, true);
}

void	CBattleServer::Game_Reload(CPlayer *pPlayer)
{
	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

	WORD Type = en_PACKET_CS_GAME_RES_RELOAD;

	*pBuffer << Type << pPlayer->AccountNo;

	SendPacketAllInRoom(pBuffer, pPlayer, true);
}

void CBattleServer::Game_HitDamage(CPlayer *pPlayer)
{
	//1. pPlayer�� ������, TargetAccountNo�� ���� ������ �÷��̾ �������.
	//2. �� �÷��̾ �Ÿ��� ���� �������� �����Ѵ�.
	//3. ���� ������ �������� ���ٸ� ��ŵ
	//4. �������� �ִٸ� ���̰� ���� �׾��ٸ� �׾��ٴ� ��Ŷ
	//5. �����ʾҴٸ� �ǰݰ����Ŷ

	INT64 TargetAccountNo = pPlayer->TargetAccountNo;
	CRoom *pRoom = &Rooms[pPlayer->RoomNo];

	pRoom->Lock();
	auto iter = pRoom->PlayerMap.find(TargetAccountNo);

	if (iter == pRoom->PlayerMap.end())
	{
		SYSLOG(L"SYSTEM", LOG_SYSTEM, L"Hit Damage ] TargetAccountNo : %I64d, Attacker AccountNo : %I64d",
			TargetAccountNo, pPlayer->AccountNo);

		pRoom->Unlock();
		return;
	}
	pRoom->Unlock();

	CPlayer *pVictim = iter->second;
	if (pVictim->Die)
		return;

	// 2. �� �÷��̾ �Ÿ��� ���� �������� �����Ѵ�.
	float distance = sqrt(
		pow(abs(pPlayer->MoveTargetX - pVictim->MoveTargetX), 2) +
		pow(abs(pPlayer->MoveTargetZ - pVictim->MoveTargetZ), 2));

	// �Ÿ��� 2���϶�� 100�� ����, 17�� �ʰ��ϸ� 0�� ����.
	// f(x) = -6x + 112
	if (distance <= 2)
		// 100 ��
		pVictim->HP -= g_Data_HitDamage;
	
	else if (distance > 17)
		// 0��
		return; //3. ���� ������ �������� ���ٸ� ��ŵ
	
	else
	{
		int Percent = -6 * distance + 112;
		int damage = g_Data_HitDamage * Percent / 100;
		pVictim->HP -= damage;
	}

	//4. �������� �ִٸ� ���̰� ���� �׾��ٸ� �׾��ٴ� ��Ŷ
	if (pVictim->HP <= 0 && !pVictim->Die)  // �̹� �׾��ִٸ� ����!
	{
		pPlayer->KillCount++;
		Game_PlayerDie(pVictim);

		// �ش���ġ�� �޵�Ŷ ����
		Game_MakeMedikit(pRoom, pVictim->MoveTargetX, pVictim->MoveTargetZ);
	}
	else
	{
		// HitDamage ��Ŷ
		CPacketBuffer *pBuffer = CPacketBuffer::Alloc();
		WORD Type = en_PACKET_CS_GAME_RES_HIT_DAMAGE;

		*pBuffer << Type << pPlayer->AccountNo << TargetAccountNo << pVictim->HP;

		SendPacketAllInRoom(pBuffer, pRoom);
	}
}

void	CBattleServer::Game_PlayerDie(CPlayer *pPlayer)
{
	CRoom *pRoom = &Rooms[pPlayer->RoomNo];

	pPlayer->Die = true;
	pRoom->SurvivalUser--;

	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();
	WORD Type = en_PACKET_CS_GAME_RES_DIE;
	*pBuffer << Type << pPlayer->AccountNo;

	SendPacketAllInRoom(pBuffer, pRoom);
}


void	CBattleServer::Game_PlayerRedZoneDamage(CPlayer *pPlayer)
{
	CRoom *pRoom = &Rooms[pPlayer->RoomNo];

	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();
	WORD Type = en_PACKET_CS_GAME_RES_REDZONE_DAMAGE;

	*pBuffer << Type << pPlayer->AccountNo << pPlayer->HP;

	SendPacketAllInRoom(pBuffer, pRoom);
}

void	CBattleServer::Game_MakeMedikit(CRoom *pRoom, float Xpos, float Ypos)
{
	ITEMS *pItems = pRoom->ItemPool.Alloc();
	pItems->MedkitID = pRoom->MedkitID++;
	pItems->PosX = Xpos;
	pItems->PosY = Ypos;
	
	pRoom->Itemmap.insert({ pItems->MedkitID, pItems });

	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

	WORD Type = en_PACKET_CS_GAME_RES_MEDKIT_CREATE;
	*pBuffer << Type << pItems->MedkitID << Xpos << Ypos;

	SendPacketAllInRoom(pBuffer, pRoom);
}

void	CBattleServer::Game_MedkitGet(CPlayer* pPlayer)
{
	UINT GettedMedkitID = pPlayer->GetMedKitID;
	pPlayer->GetMedKitID = 0;

	//	������ �ش� �޵�Ŷ ��ǥ��, �ش� �÷��̾��� ��ǥ�� Ȯ���Ͽ� X,Y �� ���� +2 ~ -2 �������� �����
	//	ȹ������ Ȯ���Ͽ�, �޵�Ŷ ���� / HP ���� (g_DataHP / 2 ��ŭ + ��Ŵ) �� ��� ��Ŷ�� ��ü�� �Ѹ�. 

	// 1. �ش� �޵�Ŷ�� ��ǥ ��������
	CRoom &Room = Rooms[pPlayer->RoomNo];

	auto iter = Room.Itemmap.find(GettedMedkitID);
	if (iter == Room.Itemmap.end())
		return;

	ITEMS *pItem = iter->second;

	// �������� ���ΰ�?
	if (pItem->PosX - 2 <= pPlayer->MoveTargetX && pPlayer->MoveTargetX <= pItem->PosX + 2 &&
		pItem->PosY - 2 <= pPlayer->MoveTargetZ && pPlayer->MoveTargetZ <= pItem->PosY + 2)
	{
		// �޵�Ŷ ����, HP ����
		Room.Itemmap.erase(GettedMedkitID);

		pPlayer->HP = min((pPlayer->HP + (g_Data_HP / 2)), g_Data_HP);
		Room.ItemPool.Free(pItem);

		// ��� ��Ŷ

		CPacketBuffer *pBuffer = CPacketBuffer::Alloc();
		WORD Type = en_PACKET_CS_GAME_RES_MEDKIT_GET;

		*pBuffer << Type << pPlayer->AccountNo << GettedMedkitID << pPlayer->HP;
		SendPacketAllInRoom(pBuffer, &Room);
	}
	else
		return;
}