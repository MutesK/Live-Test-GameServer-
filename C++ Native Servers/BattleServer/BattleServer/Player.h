#pragma once

#include "Engine/MMOServer.h"

class CBattleServer;

class CPlayer : public CNetSession
{
public:
	////////////////////////////////////////////////////////////
	// ����
	////////////////////////////////////////////////////////////
	char EnterToken[32];

	char SessionKey[64];
	char ConnectToken[32];

	UINT VerCode;
	/////////////////////////////////////////////////////////////
	// ��ſ�
	LONG ProcessType;

	ULONGLONG LastTick;
	/////////////////////////////////////////////////////////////
	// ���� �������� ����
	bool Die;
	int RoomNo;
	INT64 AccountNo;

	WCHAR NickName[20];

	int HP;
	//	MoveTarget ĳ���Ͱ� �̵� ������
	float	MoveTargetX;
	float	MoveTargetY;
	float	MoveTargetZ;

	// HitPoint ĳ���Ͱ� �ٶ󺸴� ��ġ(�þ� �� �ѹ߻� Ÿ��)
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


	// ���� �� �÷��̾ �ٸ� �÷��̾ Ÿ�������� ��� ����
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

	// ��Ŷ ó�� �Լ�
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

