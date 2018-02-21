#pragma once

#include "Engine\NetServer.h"
#include "CommonProtocol.h"
#include <map>
#include <list>
using namespace std;

#define MAX_BATTLEROOM 300

class CLanBattle;

class CChatServer : public CNetServer
{
public:
	enum
	{
		enDISSession,
		enJoinSession,
		enMODESession,
		enMSGSession,

		enTIMEOUT = 6000000,
	};

	class CPlayer
	{
	public:
		ULONG64 SessionID;
		__int64 AccountNo;

		WCHAR NickName[20];
		WCHAR ID[20];

		int RoomNo;
		bool Logined;
		bool EnterRoom;
		ULONG64 LastRecvTick;
	};

	class CRoom
	{
	public:
		int RoomNo;
		int BattleServerNo;
		int MaxUser;
		
		LONG64 isUse;
		char EnterToken[32];

		SRWLOCK RoomPlayerLock;
		list<CPlayer *> RoomPlayer;  // ����ȭ �ʿ�
		CRoom()
		{
			InitializeSRWLock(&RoomPlayerLock);
		}
		void Lock()
		{
			AcquireSRWLockExclusive(&RoomPlayerLock);
		}
		void UnLock()
		{
			ReleaseSRWLockExclusive(&RoomPlayerLock);
		}
	};

#pragma pack(push, 1)
	struct Queue_DATA
	{
		ULONG64 SessionID;
		int Type;
		CPacketBuffer *pBuffer;
	};
#pragma pack(pop)
public:
	CChatServer();
	virtual ~CChatServer();

	// UpdateThread �Լ�
	virtual bool Start(WCHAR *szServerConfig, bool nagleOption);

	virtual void Stop();
	void Mornitoring();
	//================================================================================================
private:
	virtual void OnClientJoin(ULONG64 OutSessionID, WCHAR *pClientIP, int Port);
	virtual void OnClientLeave(ULONG64 SessionID);

	virtual bool OnConnectionRequest(WCHAR *szIP, int Port);


	virtual void OnRecv(ULONG64 OutSessionID, CPacketBuffer *pOutBuffer);
	virtual void OnSend(ULONG64 OutSessionID, int SendSize);

	virtual void OnError(int errorcode, WCHAR *szMessage);

	virtual void OnWorkerThreadBegin();
	virtual void OnWorkerThreadEnd();
	//================================================================================================

	static UINT WINAPI UpdateThread(LPVOID arg);
	bool UpdateWork();


	void JoinSession(ULONG64 SessionID);
	void DisconnectSession(ULONG64 SessionID);
	void MsgProc(ULONG64 SessionID, CPacketBuffer *pBuffer);

	void ReqLogin(ULONG64 SessionID, CPacketBuffer *pBuffer);
	void ReqEnterRoom(ULONG64 SessionID, CPacketBuffer *pBuffer);
	void ReqChatMessage(ULONG64 SessionID, CPacketBuffer *pBuffer);
	void ReqHeartBeat(ULONG64 SessionID);


	void ResLogin(CPlayer *pPlayer, BYTE bResult);
	void ResEnterRoom(CPlayer *pPlayer, BYTE bResult);
	void ResChatMessage(CPlayer *pPlayer, WORD &MessgeLen, WCHAR* pMessageStr);

private:
	map <ULONG64, CPlayer *> PlayerMap;

	CMemoryPool<Queue_DATA> UpdatePool;
	CMemoryPool<CPlayer> PlayerPool;

	CLockFreeQueue<Queue_DATA *> UpdateQueue;
	///////////////////////////////////////////////////
	HANDLE hUpdateThread;
	HANDLE hUpdateEvent;

	long m_UpdateTPS;
	long m_WorkingRoom;
	ULONG64 m_TotalUserCount;

	bool b_ExitFlag;
	SYSTEMTIME StartTime;

	// �÷��̾ ���������� �����ͼ����� ���� ��ū�� ����� �޾Ҵ��� Ȯ���ϴ� �뵵�� ��ū
	char ConnectToken[32];
///////////////////////////////////////////////////
// ��Ʋ �κ�
///////////////////////////////////////////////////
	CRoom RoomArray[MAX_BATTLEROOM];

public:
	CLanBattle *pBattleServer;

	void DisconnectBattleServer();
//////////////////////////////////////////////////////////////////
// ��Ʋ������ ���� �� ��Ŷ ó�� 
//////////////////////////////////////////////////////////////////
	int ReqConnectToken(char* pConnectToken);
	int ReqCreateRoom(int BattleServerNo, int RoomNo, int MaxUser, char *pEnterToken);
	int ReqDestoryRoom(int BattleServerNo, int RoomNo);
};
