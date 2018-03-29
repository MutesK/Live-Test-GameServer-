#include "Player.h"
#include "BattleServer.h"


CPlayer::CPlayer()
{
}


CPlayer::~CPlayer()
{
}

void CPlayer::SetBattleServer(CBattleServer *pServer)
{
	pBattleServer = pServer;
}

bool CPlayer::OnAuth_SessionJoin(void)
{
	ResetPlayer();

	return true;
}

void CPlayer::ResetPlayer()
{
	memset(EnterToken, 0, 32);
	memset(ConnectToken, 0, 32);
	memset(SessionKey, 0, 64);
	memset(NickName, 0, 40);

	Die = false;
	VerCode = RoomNo = AccountNo = ProcessType = 0;

	Record_Die = Record_PlayCount = Record_PlayTime
		= Record_Kill = Record_Win = 0;
}
bool CPlayer::OnAuth_SessionLeave(bool bToGame)
{
	if (!bToGame)
	{
		pBattleServer->DisconnectedPlayer(this);

		ResetPlayer();
	}
	return true;
}
bool CPlayer::OnAuth_Packet(CPacketBuffer *pPacket)
{
	WORD Type;

	*pPacket >> Type;

	switch (Type)
	{
	case en_PACKET_CS_GAME_REQ_LOGIN:
		// 배틀서버로 클라이언트 로그인 요청
		ReqLogin(pPacket);
	break;
	case en_PACKET_CS_GAME_REQ_ENTER_ROOM:
		// 배틀서버의 방에 입장 요청
		ReqEnterRoom(pPacket);
		break;
	case en_PACKET_CS_GAME_REQ_HEARTBEAT:
		break;
	}

	pPacket->Free();

	_LastRecvPacketTime = CUpdateTime::GetTickCount();

	return true;
}
void CPlayer::OnAuth_Timeout(void)
{
	SYSLOG(L"TIMEOUT", LOG_SYSTEM, L"Timeout Session On AuthMode: AccountNo : %I64d", AccountNo);
}

bool CPlayer::OnGame_SessionJoin(void)
{
	return true;
}

bool CPlayer::OnGame_SessionLeave(void)
{
	pBattleServer->DisconnectedPlayer(this);

	return true;
}

bool CPlayer::OnGame_Packet(CPacketBuffer *pPacket)
{
	WORD Type;

	*pPacket >> Type;

	switch (Type)
	{
	case en_PACKET_CS_GAME_REQ_HEARTBEAT:
		break;
	case en_PACKET_CS_GAME_REQ_MOVE_PLAYER:
		ReqMovePlayer(pPacket);
		break;
	case en_PACKET_CS_GAME_REQ_HIT_POINT:
		ReqHitPoint(pPacket);
		break;
	case en_PACKET_CS_GAME_REQ_FIRE1:
		ReqFire1(pPacket);
		break;
	case en_PACKET_CS_GAME_REQ_RELOAD:
		//ReqReload(pPacket);
		break;
	case en_PACKET_CS_GAME_REQ_HIT_DAMAGE:
		ReqHitDamage(pPacket);
		break;
	case en_PACKET_CS_GAME_REQ_MEDKIT_GET:
		ReqMedkitGet(pPacket);
		break;
	}
	if (!Die)
		ProcessType = Type;
	
	pPacket->Free();
	_LastRecvPacketTime = CUpdateTime::GetTickCount();

	return true;
}

void CPlayer::OnGame_Timeout(void)
{
	// SYSLOG(L"TIMEOUT", LOG_SYSTEM, L"Timeout Session On GameMode: AccountNo : %I64d", AccountNo);
	// pBattleServer->DisconnectedPlayer(this);
}

bool CPlayer::OnGame_SessionRelease(void)
{
	return true;
}

void CPlayer::OnError(int ErrorCode, WCHAR *szStr)
{
	SYSLOG(L"ERROR", LOG_ERROR, szStr);
}

int	CPlayer::GetMode()
{
	return this->_Mode;
}

void CPlayer::ReqLogin(CPacketBuffer *pPacket)
{
	*pPacket >> AccountNo;
	pPacket->GetData(SessionKey, 64);
	pPacket->GetData(ConnectToken, 32);
	*pPacket >> VerCode;

	pBattleServer->LoginProcessReq(this);
}

void CPlayer::ReqEnterRoom(CPacketBuffer *pPacket)
{
	INT64 ReqAccountNo;
	*pPacket >> ReqAccountNo;

	if (ReqAccountNo != AccountNo)
	{
		SYSLOG(L"SYSTEM", LOG_SYSTEM, L"REQ_ENTER_ROOM PacketError: Packet AccountNo Mismatched");
		Disconnect(true);
	}

	*pPacket >> RoomNo;
	pPacket->GetData(EnterToken, 32);

	pBattleServer->ReqEnterRoom(this);
}


void CPlayer::ReqMovePlayer(CPacketBuffer *pBuffer)
{
	if (Die)
		return;

	INT64 AccountNo; // Debug Variable;

	float TargetX, TargetY, TargetZ;
	bool OverX, OverY, OverZ;

	*pBuffer >> MoveTargetX >> MoveTargetY >> MoveTargetZ >>
		HitPointX >> HitPointY >> HitPointZ;
}

void CPlayer::ReqHitPoint(CPacketBuffer *pBuffer)
{
	//	캐릭터가 바라보는 시점으로 일정시간마다 반복적으로 보냄
	//	서버는 이를 다른 유저에게 보내준다.
	//
	//	실시간으로 보낼 필요는 없으나 시야 동기화를 위해서 보냄
	//  자기 자신에게는 보내지말자
	if (Die)
		return;

	*pBuffer >> HitPointX >> HitPointY >> HitPointZ;
}

void CPlayer::ReqFire1(CPacketBuffer *pBuffer)
{
	//	총 발사. 발사 Target 지점을 보냄.
	//
	//	서버는 다른 유저에게 RES 패킷을 뿌림
	//	총에 맞은 오브젝트는 별도의 패킷으로 전송

	// 자기 자신에게는 보내지 않는다.
	if (Die)
		return;

	*pBuffer >> HitPointX >> HitPointY >> HitPointZ;
}

void CPlayer::ReqHitDamage(CPacketBuffer *pBuffer)
{
	if (Die)
		return;
	*pBuffer >> TargetAccountNo;
}

void CPlayer::ReqMedkitGet(CPacketBuffer *pBuffer)
{
	if (Die)
		return;

	*pBuffer >> GetMedKitID;
}