#pragma once

#include "Engine/LanServer.h"
#include <map>
#include "MasterServer.h"

/*
	������ ������ ��Ʋ���� ������
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

	//	��Ʋ������ ������ �������� ���� ���� �˸�
	void ReqServerOn(ULONG64 SessionID, CPacketBuffer *pBuffer);
	int ServerOn(ULONG64 SessionID, CPacketBuffer *pBuffer);
	void ResServerOn(ULONG64 SessionID, int BattleIndex);
	//	��Ʋ������ ������ū ����� �˸�.
	void ReqConnectToken(ULONG64 SessionID, CPacketBuffer *pBuffer);
	int ConnectToken(ULONG64 SessionID, char *pToken);
	void ResConnectToken(ULONG64 SessionId, UINT ReqSeqence);
	//	��Ʋ ������ �ű� ��� �� ���� �˸�
	void ReqCreatedRoom(ULONG64 SessionID, CPacketBuffer *pBuffer);
	int CreateRoom(ULONG64 SessionID, int ServerNo, int RoomNo, int MaxUser, char *pToken);
	void ResCreateRoom(ULONG64 SessionID, int RoomNo, UINT ReqSeqence);
	//	�� ���� �˸�
	void ReqClosedRoom(ULONG64 SessionID, CPacketBuffer *pBuffer);
	int CloseRoom(ULONG64 SessionID, int RoomNo);
	void ResCloseRoom(ULONG64 SessionID, int RoomNo, UINT ReqSeqence);
	//	�濡�� ������ ������
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

