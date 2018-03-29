#pragma once

#include "Engine/CommonInclude.h"
#include <map>

#define MAX_BATTLESERVER 100
#define MAX_MATCHSERVER 100
#define MAX_BATTLEROOM 250

class CMatchLanServer;
class BattleLanServer;

class CPlayer
{
public:
	UINT64 ClinetKey;
	ULONGLONG TickCheck;

	WORD BattleServerNo;
	int RoomNo;
};

class CRoom
{
public:
	WORD BattleServerNo;
	int RoomNo;

	LONG MaxUser;
	LONG ReadyUser;

	bool isUse;

	char EnterToken[32];

	SRWLOCK RoomLock;

	CRoom()
	{
		InitializeSRWLock(&RoomLock);
	}

	void Lock()
	{
		AcquireSRWLockExclusive(&RoomLock);
	}

	void UnLock()
	{
		ReleaseSRWLockExclusive(&RoomLock);
	}
};

class CBattleInfo
{
public:
	ULONG64 OutSessionID;

	char ConnectToken[32];
	WORD BattleServerNo;
	bool isUse;

	WCHAR ServerIP[16];
	WORD Port;

	WCHAR ChatServerIP[16];
	WORD ChatSeverPort;

	CRoom Rooms[MAX_BATTLEROOM];
};

class CMatchInfo
{
public:
	int ServerNo;

	bool Used;
	ULONG64 SessionID;
};
/*
class CMasterServer
{
public:
	CMasterServer();
	~CMasterServer();

	bool Start(WCHAR *szServerConfig, bool bNagleOption);
	void Stop();

	void Monitoring();
///////////////////////////////////////////////////////////////
// 매치메이킹 서버에서 호출하는 함수
///////////////////////////////////////////////////////////////
	int MatchServerServerOn(ULONG64 MatchSessionID, int ServerNo, char *pMasterToken);
	int MatchServerGameRoom(ULONG64 MatchSessionID, UINT64 ClientKey, CPacketBuffer *pOutBuffer);
	int MatchServerGameRoomEnterSuccess(ULONG64 SessionID, WORD BattleServerNo, int RoomNo, UINT64 ClientKey);
	int MatchServerGameRoomEnterFail(ULONG64 SessionID, UINT64 ClientKey);

	int MatchServerDisconnect(ULONG64 MatchSessionID);
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 배틀 서버에서 호출하는 함수
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	int BattleServerServerOn(ULONG64 BattleSessionID, CPacketBuffer *pOutBuffer);
	int BattleServerConnectToken(ULONG64 BattleSessionID, char *pToken);
	int BattleServerCreateRoom(ULONG64 BattleSessionID, int BattleServerNo, int RoomNo, int MaxUser, char *pToken);
	int BattleServerCloseRoom(ULONG64 BattleSessionID, int RoomNo);
	int BattleServerLeftOneUser(ULONG64 BattleSessionID, int RoomNo);

	int BattleServerDisconnect(ULONG64 BattleSessionID);


	static UINT WINAPI hPlayerCheck(LPVOID arg);
	UINT PlayerCheck();
private:
	CMatchLanServer *pMatchServer;


	char MasterToken[32];
	

	CMemoryPoolTLS<CPlayer> PlayerPool;
	map<UINT64, CPlayer *> PlayerMap;

	SRWLOCK	PlayerLock;
	HANDLE hPlayerCheckThread;
	bool m_bServerOpen;


	LONG64 MatchServerNum;
	LONG64 BattleServerNum;
	LONG64 ActivateGameRoomTotal;

	LONG64 RoomRequest;
	BattleLanServer *pBattleServer;
};

*/