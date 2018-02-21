#include "BattleServer.h"
#include "Engine/MMOServer.h"
#include "Player.h"
#include "MasterLanClient.h"
#include "ChatLanServer.h"
#include "Engine/Parser.h"
#include "Engine/HttpPost.h"

// GAME 모드 업데이트 이벤트 로직처리부
// 실제 게임 로직 처리한다.
void	CBattleServer::OnGame_Update(void)
{
	/*
	Game_CreateCharacter();

	Game_MoveCharacter();

	Game_Reload();

	Game_HitPoint();

	Game_Fire1();

	Game_HitDamage();

	// Game_PlayerTimeCheck();
	*/

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
		}
	}

}

void	CBattleServer::Game_CreateCharacter(CPlayer *pPlayer)
{
	// 생성 위치 인덱스
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
	// 공격 피해자
	INT64  VictimAccountNo = pPlayer->TargetAccountNo;
	pPlayer->TargetAccountNo = 0;

	// 해당 플레이어를 가져온다.
	CRoom *pRoom = &Rooms[pPlayer->RoomNo];

	pRoom->Lock();
	CPlayer *pVictimPlayer = pRoom->PlayerMap.find(VictimAccountNo)->second;
	pRoom->Unlock();

	if (pVictimPlayer == nullptr)
	{
		SYSLOG(L"HITDAMAGE", LOG_ERROR, L"Victim Player isn't Detected, Victim AccountNo : %I64d, Attacker AccountNo : %I64d", VictimAccountNo, pPlayer->AccountNo);
		return;
	}

	// HP 차감시킨다.
	pVictimPlayer->HP -= enDAMAGE;

	if (pVictimPlayer->HP < 0)
		pVictimPlayer->HP = 0;

	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

	WORD Type = en_PACKET_CS_GAME_RES_HIT_DAMAGE;

	*pBuffer << Type << VictimAccountNo << pVictimPlayer->HP;

	SendPacketAllInRoom(pBuffer, pPlayer, true);

}