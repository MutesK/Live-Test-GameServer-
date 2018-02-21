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


	VerCode = RoomNo = AccountNo = ProcessType = 0;

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
		// ��Ʋ������ Ŭ���̾�Ʈ �α��� ��û
		ReqLogin(pPacket);
	break;
	case en_PACKET_CS_GAME_REQ_ENTER_ROOM:
		// ��Ʋ������ �濡 ���� ��û
		ReqEnterRoom(pPacket);
		break;
	case en_PACKET_CS_GAME_REQ_HEARTBEAT:
		break;
	}

	pPacket->Free();

	LastTick = CUpdateTime::GetTickCount();

	return true;
}
void CPlayer::OnAuth_Timeout(void)
{
	pBattleServer->DisconnectedPlayer(this);
	ResetPlayer();
	AccountNo = -1;
}

bool CPlayer::OnGame_SessionJoin(void)
{
	return true;
}

bool CPlayer::OnGame_SessionLeave(void)
{
	pBattleServer->DisconnectedPlayer(this);
	ResetPlayer();
	AccountNo = -1;

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
	}
	ProcessType = Type;
	pPacket->Free();

	LastTick = CUpdateTime::GetTickCount();

	return true;
}

void CPlayer::OnGame_Timeout(void)
{
	pBattleServer->DisconnectedPlayer(this);
	ResetPlayer();
	AccountNo = -1;
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
	INT64 AccountNo; // Debug Variable;

	float TargetX, TargetY, TargetZ;
	bool OverX, OverY, OverZ;

	*pBuffer >> MoveTargetX >> MoveTargetY >> MoveTargetZ >>
		HitPointX >> HitPointY >> HitPointZ;
}

void CPlayer::ReqHitPoint(CPacketBuffer *pBuffer)
{
	//	ĳ���Ͱ� �ٶ󺸴� �������� �����ð����� �ݺ������� ����
	//	������ �̸� �ٸ� �������� �����ش�.
	//
	//	�ǽð����� ���� �ʿ�� ������ �þ� ����ȭ�� ���ؼ� ����
	//  �ڱ� �ڽſ��Դ� ����������

	*pBuffer >> HitPointX >> HitPointY >> HitPointZ;
}

void CPlayer::ReqFire1(CPacketBuffer *pBuffer)
{
	//	�� �߻�. �߻� Target ������ ����.
	//
	//	������ �ٸ� �������� RES ��Ŷ�� �Ѹ�
	//	�ѿ� ���� ������Ʈ�� ������ ��Ŷ���� ����

	// �ڱ� �ڽſ��Դ� ������ �ʴ´�.

	*pBuffer >> HitPointX >> HitPointY >> HitPointZ;
}

void CPlayer::ReqHitDamage(CPacketBuffer *pBuffer)
{
	*pBuffer >> TargetAccountNo;
}