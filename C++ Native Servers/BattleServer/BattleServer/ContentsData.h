#pragma once


#include <Windows.h>
#include <map>
#include "Engine/MemoryPool.h"
using namespace std;
/////////////////////////////////////////////////////////////////////////////////////
// 채팅 서버, 마스터 서버 통신용
/////////////////////////////////////////////////////////////////////////////////////

#define READY_SEC 15000

enum eROOM_STATUS
{
	eGAME_MODE = 1,
	eCLOSE_MODE,
	eOPEN_MODE,
	eFINISH_GAMEMODE,
	eNOT_USE
};

enum GAME_CONTENTS_VARIABLE
{
	enDAMAGE = 5,

	LEFT = 0x80,
	TOP = 0x40,
	RIGHT = 0x20,
	BOTTOM = 0x10
};

class CPlayer;
class CChatLanServer;
class CMasterLanClient;

struct ITEMS
{
	UINT MedkitID;

	float PosX;
	float PosY;
};

class Server
{
public:
	WCHAR ServerIP[16];
	int  ServerPort;

	ULONG64 SessionID;
	bool  isLogin;
	UINT  ReqSequence;
};

class ChatServer : public Server
{
public:
	CChatLanServer *pChatServer;
};

class MasterServer : public Server
{
public:
	CMasterLanClient *pMasterServer;

	bool isServerOnSended;
};

class CRoom
{
public:
	int MaxUser;
	int RoomNo;
	int SurvivalUser;

	UINT MedkitID;
	int GameIndex;
	int RoomStatus;
	ULONGLONG ReadySec;

	char EnterToken[32];

	ULONGLONG RedZoneTick;
	ULONGLONG RedZoneDemageTick;

	bool	RedZoneAlertFlag;
	BYTE	RedZoneAlertPosition;
	BYTE	RedZonePosition;  // left, top, right, bottom

	SRWLOCK	PlayerListLock;
	//list <CPlayer *> PlayerList;
	map<INT64, CPlayer *> PlayerMap;


	CMemoryPool<ITEMS> ItemPool;
	map<INT64, ITEMS *> Itemmap;
	CRoom()
	{
		InitializeSRWLock(&PlayerListLock);
	}

	void Lock()
	{
		AcquireSRWLockExclusive(&PlayerListLock);
	}

	void Unlock()
	{
		ReleaseSRWLockExclusive(&PlayerListLock);
	}

	void ReadLock()
	{
		AcquireSRWLockShared(&PlayerListLock);
	}

	void ReadUnLock()
	{
		ReleaseSRWLockShared(&PlayerListLock);
	}
};

extern float	g_Data_Position[10][2];
extern int		g_Data_HP;

extern int		g_Data_HitDamage;
