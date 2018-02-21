#pragma once

#include "CommonProtocol.h"
#include "Engine/NetServer.h"
#include <map>
using namespace std;

class CLanMatchServer;
class CDBConnectorTLS;

struct Player
{
	UINT64 ClientUniqueKey; // �ߺ� �Ұ���

	__int64 AccountNo;  // �ߺ� ����
	ULONG64 SessionID; // �ߺ� �Ұ���

	WCHAR ConnectIP[16];
	int Port;

	char SessionKey[64];
	bool ReqRoomInfo; // �� ���� true�ε� disconnect�� �Ǿ��ٸ� ���и� �˷��ش�.


	int RoomNo;
	WORD BattleServerNo;

	WCHAR BattleServerIP[16];
	WORD BattleServerPort;

	WCHAR ChatServerIP[16];
	WORD ChatServerPort;

	char ConnectToken[32];
	char EnterToken[32];

	ULONGLONG LastRecv;
};

class CMatchServer : public CNetServer
{
public:
	CMatchServer();
	virtual ~CMatchServer();

	virtual bool Start(WCHAR *szServerConfig, bool NagleOption);
	virtual void Stop();

	void Monitoring();
	void MasterServerConnect();
	void MasterServerDisconnect();
private:
	void FirstDBInsert(WCHAR *IP, int Port);
	void ConnectUserDBUpdate(int ConnectUser);

	virtual void OnClientJoin(ULONG64 OutSessionID, WCHAR *pClientIP, int Port);
	virtual void OnClientLeave(ULONG64 SessionID);

	virtual bool OnConnectionRequest(WCHAR *szIP, int Port);


	virtual void OnRecv(ULONG64 OutSessionID, CPacketBuffer *pOutBuffer);
	virtual void OnSend(ULONG64 OutSessionID, int SendSize);

	virtual void OnError(int errorcode, WCHAR *szMessage);
	
	virtual void OnWorkerThreadBegin();
	virtual void OnWorkerThreadEnd();
	
	void DisconnectPlayer(ULONG64 SessionID);
	void HTTPURLParse();
/////////////////////////////////////////////////////////////////
// Client <-> MatchServer -> MasterServer
/////////////////////////////////////////////////////////////////
	void ReqLogin(ULONG64 OutSessionID, CPacketBuffer *pBuffer);
	void ReqGameRoom(ULONG64 OutSessionID);
	void ReqGameRoomEnter(ULONG64 OutSessionID, CPacketBuffer *pBuffer);

	bool ResLogin(ULONG64 OutSessionID, BYTE ResultStatus);
	void ResGameRoom(Player *pPlayer, BYTE Status = 0, CPacketBuffer *BattleServerInfo = nullptr);
	void ResGameRoomEnter(Player *pPlayer);

	void ReqGameRoomEnterSuccess(Player *pPlayer, WORD BattleServerNo, int RoomNo);
	void ReqGameRoomEnterFail(Player *pPlayer);
/////////////////////////////////////////////////////////////
// MasterServer -> MatchServer -> Client
/////////////////////////////////////////////////////////////
public:
	void MasterServerResGameRoom(CPacketBuffer *pBuffer);
private:
	////////////////////////////////////////////////////////////
	// ���� ���� �νİ� ��Ʈ��Ʈ�� ���� DB ����
	////////////////////////////////////////////////////////////
	CDBConnectorTLS *pConnect;
	CLanMatchServer *pClient;

	WORD ServerNo;
	char MasterToken[32];


	LONG64 ReqGameRoomTPS;
	LONG64 RoomEnterSuccessTPS;
	LONG64 RoomEnterFailTPS;

	LONG64 UniqueIndex;

////////////////////////////////////////////////////////////
// HTTP ���� API ���ۿ�
	WCHAR shAPIURL[100];

	int httpPort;
	WCHAR httpURL[100];
	WCHAR httpPath[20000];

	char PostDATA[100];
	char RecvDATA[1400];
////////////////////////////////////////////////////////////
	bool ServerOpen;

////////////////////////////////////////////////////////////
// �÷��̾� ������
	CMemoryPoolTLS<Player> PlayerPool;
	map<ULONG64, Player *> PlayerMap;
	map<ULONG64, Player *> PlayerUniqueMap;


	SRWLOCK PlayerLock;
	SRWLOCK PlayerUniqueLock;

	HANDLE hTimeOut;
	static UINT WINAPI hTimedOutThread(LPVOID arg);
	UINT TimeOutCheck();
////////////////////////////////////////////////////////////

	LONG64 LoginRequest;
	LONG64 RoomRequest;
	UINT   Ver_Code;
};

