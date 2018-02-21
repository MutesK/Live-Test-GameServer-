#pragma once

#include "CommonProtocol.h"
#include "Engine/LanServer.h"
#include "MasterServer.h"

/*
	마스터서버가 메치메이킹 서버 관리용
*/



class BattleLanServer;
class CMatchLanServer : public CLanServer
{
public:
	CMatchLanServer(BattleLanServer *pBattleServer);
	virtual ~CMatchLanServer();

	virtual bool Start(WCHAR* szServerIP, int Port, int WorkerThread,
		int MaxSession, bool NagleOption, char *pMasterToken);
	virtual void Stop();

	void SendMatchPacket(ULONG64 SessionID, CPacketBuffer *pBuffer);
	void Monitoring();
private:
	virtual void OnClientJoin(ULONG64 OutSessionID, WCHAR *pClientIP, int Port);
	virtual void OnClientLeave(ULONG64 SessionID);

	virtual bool OnConnectionRequest(WCHAR *szIP, int Port);


	virtual void OnRecv(ULONG64 OutSessionID, CPacketBuffer *pOutBuffer);
	virtual void OnSend(ULONG64 OutSessionID, int SendSize);

	virtual void OnError(int errorcode, WCHAR *szMessage);

	virtual void OnWorkerThreadBegin();
	virtual void OnWorkerThreadEnd();

	bool ServerDisconnect(ULONG64 SessionID);
 
	void ReqMatchServerOn(ULONG64 SessionID, CPacketBuffer *pBuffer);
	void ResMatchServerOn(ULONG64 SessionID, WORD ServerNo);

	void ReqGameRoom(ULONG64 SessionID, CPacketBuffer *pBuffer);
	void ResGameRoom(ULONG64 SessionID, UINT64 ClientKey, BYTE Status, CBattleInfo *pInfo, CRoom *pRoom);

	void ReqGameRoomEnterSuccess(ULONG64 SessionID, CPacketBuffer *pBuffer);
	void ReqGameRoomEnterFail(ULONG64 SessionID, CPacketBuffer *pBuffer);

	static UINT WINAPI hPlayerCheck(LPVOID arg);
	UINT PlayerCheck();
private:
	LONG64 PacketError;
	LONG64 RoomRequest;
	LONG64 MatchServerNum;
	LONG64 Disconnected;

	char MasterToken[32];

	CMemoryPoolTLS<CPlayer> PlayerPool;
	map<UINT64, CPlayer *> PlayerMap;

	CMatchInfo MatchServerInfo[MAX_MATCHSERVER];
	CBattleInfo* BattleServerArr[MAX_BATTLESERVER];

	SRWLOCK	PlayerLock;
	HANDLE hPlayerCheckThread;

	BattleLanServer *pBattleServer;
};



struct cmp {
	bool operator()(CRoom *pLeft, CRoom *pRight) {
		return pLeft->MaxUser >= pRight->MaxUser;
	}
};
