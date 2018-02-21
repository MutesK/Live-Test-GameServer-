#pragma once

#include "Engine/LanServer.h"
#include <map>
#include "MasterServer.h"

/*
	마스터 서버가 배틀서버 관리용
*/

class BattleLanServer : public CLanServer
{
public:
	BattleLanServer();
	virtual ~BattleLanServer();

	virtual bool Start(WCHAR* szServerIP, int Port, int WorkerThread, int MaxSession, bool NagleOption, char *pMasterToken);
	virtual void Stop();

	void Monitoring();

	void SyncBattleInfo(CBattleInfo *pArrInfo[]);
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

	//	배틀서버가 마스터 서버에게 서버 켜짐 알림
	void ReqServerOn(ULONG64 SessionID, CPacketBuffer *pBuffer);
	int ServerOn(ULONG64 SessionID, CPacketBuffer *pBuffer);
	void ResServerOn(ULONG64 SessionID, int BattleIndex);
	//	배틀서버의 연결토큰 재발행 알림.
	void ReqConnectToken(ULONG64 SessionID, CPacketBuffer *pBuffer);
	int ConnectToken(ULONG64 SessionID, char *pToken);
	void ResConnectToken(ULONG64 SessionId, UINT ReqSeqence);
	//	배틀 서버의 신규 대기 방 생성 알림
	void ReqCreatedRoom(ULONG64 SessionID, CPacketBuffer *pBuffer);
	int CreateRoom(ULONG64 SessionID, int ServerNo, int RoomNo, int MaxUser, char *pToken);
	void ResCreateRoom(ULONG64 SessionID, int RoomNo, UINT ReqSeqence);
	//	방 닫힘 알림
	void ReqClosedRoom(ULONG64 SessionID, CPacketBuffer *pBuffer);
	int CloseRoom(ULONG64 SessionID, int RoomNo);
	void ResCloseRoom(ULONG64 SessionID, int RoomNo, UINT ReqSeqence);
	//	방에서 유저가 나갔음
	void ReqLeftONEUser(ULONG64 SessionID, CPacketBuffer *pBuffer);
	int LeftOneUser(ULONG64 SessionID, int RoomNo);
	void ResLeftOneUser(ULONG64 SessionID, int RoomNo, UINT ReqSequence);
private:
	char MasterToken[32];

	LONG64 PacketError;
	LONG BattleServerIndex;
	LONG Disconnected;

	CBattleInfo BattleServerArr[MAX_BATTLESERVER];

	LONG64 BattleServerNum;
	LONG64 ActivateGameRoomTotal;
};

