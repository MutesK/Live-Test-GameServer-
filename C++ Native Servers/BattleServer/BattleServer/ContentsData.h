#pragma once


#include <Windows.h>
#include <map>
using namespace std;
/////////////////////////////////////////////////////////////////////////////////////
// 채팅 서버, 마스터 서버 통신용
/////////////////////////////////////////////////////////////////////////////////////

#define READY_SEC 10000

enum eROOM_STATUS
{
	eGAME_MODE = 1,
	eCLOSE_MODE,
	eOPEN_MODE,
	eNOT_USE
};

enum GAME_CONTENTS_VARIABLE
{
	enDAMAGE = 5
};

class CPlayer;
class CChatLanServer;
class CMasterLanClient;

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

	int GameIndex;
	int RoomStatus;
	ULONGLONG ReadySec;

	char EnterToken[32];

	SRWLOCK	PlayerListLock;
	//list <CPlayer *> PlayerList;
	map<INT64, CPlayer *> PlayerMap;
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