#pragma once

#include "Engine/MMOServer.h"

class CBattleServer;

class CPlayer : public CNetSession
{
public:
	////////////////////////////////////////////////////////////
	// 인증
	////////////////////////////////////////////////////////////
	char EnterToken[32];

	char SessionKey[64];
	char ConnectToken[32];

	UINT VerCode;
	/////////////////////////////////////////////////////////////
	// 통신용
	LONG ProcessType;

	ULONGLONG LastTick;
	/////////////////////////////////////////////////////////////
	// 게임 컨텐츠용 변수
	bool Die;
	int RoomNo;
	INT64 AccountNo;

	WCHAR NickName[20];

	int HP;
	//	MoveTarget 캐릭터가 이동 목적지
	float	MoveTargetX;
	float	MoveTargetY;
	float	MoveTargetZ;

	// HitPoint 캐릭터가 바라보는 위치(시야 및 총발사 타겟)
	float	HitPointX;
	float	HitPointY;
	float	HitPointZ;

	UINT	GetMedKitID;
	/////////////////////////////////////////////////////////////

	int		Record_PlayCount;
	int		Record_PlayTime;
	int		Record_Kill;
	int		Record_Die;
	int		Record_Win;

	int		KillCount;
	ULONGLONG	PlayTime;


	// 만약 이 플레이어가 다른 플레이어를 타격했을때 대비 저장
	INT64 TargetAccountNo;
public:
	CPlayer();
	virtual ~CPlayer();

	int	GetMode();

	void SetBattleServer(CBattleServer *pServer);
private:
	virtual bool		OnAuth_SessionJoin(void);
	virtual bool		OnAuth_SessionLeave(bool bToGame = false);
	virtual bool		OnAuth_Packet(CPacketBuffer *pPacket);
	virtual void		OnAuth_Timeout(void);

	virtual bool		OnGame_SessionJoin(void);
	virtual bool		OnGame_SessionLeave(void);
	virtual bool		OnGame_Packet(CPacketBuffer *pPacket);
	virtual void		OnGame_Timeout(void);

	virtual bool		OnGame_SessionRelease(void);

	virtual void		OnError(int ErrorCode, WCHAR *szStr);

	// 패킷 처리 함수
	void ReqLogin(CPacketBuffer *pBuffer);
	void ReqEnterRoom(CPacketBuffer *pBuffer);
	//void ReqHeartBeat(CPacketBuffer *pBuffer);

	void ReqMovePlayer(CPacketBuffer *pBuffer);
	void ReqHitPoint(CPacketBuffer *pBuffer);
	void ReqFire1(CPacketBuffer *pBuffer);
	//void ReqReload(CPacketBuffer *pBuffer);
	void ReqHitDamage(CPacketBuffer *pBuffer);
	void ReqMedkitGet(CPacketBuffer *pBuffer);
	void ResetPlayer();
private:
	CBattleServer *pBattleServer;

	friend class CBattleServer;
};

