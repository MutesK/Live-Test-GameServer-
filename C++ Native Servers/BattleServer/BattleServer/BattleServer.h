#pragma once

#include "CommonProtocol.h"
#include "Engine/MMOServer.h"
#include "ContentsData.h"
#include <memory>
/*
	1. 채팅서버 관리, 마스터 서버 연결
		채팅서버 1:1 연결, 마스터 서버는 1:N 구조
		
	2. MMOServer 자체 처리
	각각의 플레이어 패킷 처리는 CGameServer 클래스
*/

class CMornitoringAgent;
class CMornitoring;

class CBattleServer : public CMMOServer
{
public:
	CBattleServer(int MaxSession);
	virtual ~CBattleServer();

	virtual bool Start(WCHAR *szServerConfig, bool bNagle);
	void Stop(void);

	void Monitoring();
	void RoomStatusMonitoring();
	void MonitoringSender();


	void HTTPURLParse();
private:
	CPlayer* Players;
public:
	/////////////////////////////////////////////////////////////////////////////
	// AUTH 모드 업데이트 이벤트 로직처리부
	void	OnAuth_Update(void);

	void	Auth_MakeRoom();
	void	Auth_CheckLeaveRoom();
	void	Auth_ReadyToPlayRoom();
	void	Auth_PlayRoom();

	void	ResReadyToPlayRoom(int RoomIndex);
	void	ReqLogin(CPlayer *pPlayer);
	void	ReqEnterRoom(CPlayer *pPlayer);

	void	ResLogin(CPlayer *pPlayer, BYTE Result);
	void	ResEnterRoom(CPlayer *pPlayer, BYTE Result);
	void	Auth_ChangeUserGameMode(int RoomIndex);

	void    ResAddUser(CPlayer *pPlayer);
	void	ResAddOtherUser(CPlayer *pPlayer);

	void	LoginProcessReq(CPlayer *pPlayer);
	BYTE	Login_SessionKeyCheck(CPlayer *pPlayer);
	BYTE	ContentLoad(CPlayer *pPlayer);

	void    ResAuthRemoveUser(CPlayer *pPlayer);
	/////////////////////////////////////////////////////////////////////////////
	// GAME 모드 업데이트 이벤트 로직처리부
	void	OnGame_Update(void);

	void	Game_PlayerCheck();
	void	Game_RoomCheck();
	void	Game_RedZoneAlert(CRoom& pRoom);
	void	Game_RedZone(CRoom& pRoom);
	void	Game_RedZonePlayerCheck(CRoom& pRoom);

	void	Game_CreateCharacter(CPlayer *pPlayer);
	void	Game_MoveCharacter(CPlayer *pPlayer);
	void	Game_HitPoint(CPlayer *pPlayer);
	void	Game_Fire1(CPlayer *pPlayer);
	void	Game_Reload(CPlayer *pPlayer);
	void	Game_HitDamage(CPlayer *pPlayer);
	void	Game_MedkitGet(CPlayer* pPlayer);


	void	Game_Result(CRoom& Room);
	void	Game_PlayerDie(CPlayer *pPlayer);
	void	Game_PlayerRedZoneDamage(CPlayer *pPlayer);
	void	Game_DestoryRoom(CRoom &Room);

	void	Game_MakeMedikit(CRoom *pRoom, float Xpos, float Ypos);

	void	ResCreateMyCharacter(CPlayer *pPlayer);
	void	ResCreateOtherCharacter(CPlayer *pPlayer);

	// 배틀서버 방에서 유저가 나감
	void    ResGameRemoveUser(CPlayer *pPlayer);
	/////////////////////////////////////////////////////////////////////////////
	// 공통 처리부
	void	SendPacketAllInRoom(CPacketBuffer *pBuffer, CRoom *pRoom);
	void	SendPacketAllInRoom(CPacketBuffer *pBuffer, CPlayer *pExceptPlayer, bool isExcept = false);
	/////////////////////////////////////////////////////////////////////////////
public:
	////////////////////////////////////////////////////////////////////////////////
	//  Login Thread
	static UINT WINAPI LoginThread(LPVOID arg);
	UINT LoginWork();

	CLockFreeQueue <CPlayer *> LoginQueue;
private:
	HANDLE hLoginThread[5];
	//////////////////////////////////////////////////////////////////////////////
public:
	void ConnectToken_RamdomGenerator();
	void RoomEnterToken_RamdomGenerator(CRoom *pRoom);
	//////////////////////////////////////////////////////////////
	// Player 클래스 호출
	// 접속 종료 파트
	//////////////////////////////////////////////////////////////
	void DisconnectedPlayer(CPlayer *pPlayer);
private:
	virtual void	OnError(int iErrorCode, WCHAR *szError);
public:
///////////////////////////////////////////////////////////////////////////////////////
// 내부 서버간 통신 함수
///////////////////////////////////////////////////////////////////////////////////////
	//	채팅서버가 배틀서버에게 서버 켜짐 알림
	int ChatServerReqServerOn(ULONG64 ChatSessionID, WCHAR *pServerIp, WORD ChatPort);

	int ChatServerReqConnectToken();
	int ChatServerResConnectToken(UINT ReqSequence);

	// 방 생성시 해당 방의 포인터를 넘긴다.
	int ChatServerCreateRoom(CRoom *pRoom);
	int ChatServerResCreateRoom(int RoomNo, UINT ReqSequence);

	int ChatServerDestoryRoom(CRoom *pRoom);
	int ChatServerResDestoryRoom(int RoomNo, UINT ReqSeqnece);

	void ChatServerDisconnect();

/////////////////////////////////////////////////////////////////////////////////////////////
	int MasterServerReqServerON();
	int MastserServerResServerOn(int BattleServerNo);

	int MasterServerReqConnectToken();
	int MasterServerResConnectToken(UINT ReqSequence);

	// 방 생성시 해당 방의 포인터를 넘긴다.
	int MasterServerCreateRoom(CRoom *pRoom);
	int MasterServerResCreateRoom(int RoomNo, UINT ReqSequence);

	// 방 닫을시 해당 방의 포인터를 넘긴다.
	int MasterServerClosedRoom(CRoom *pRoom);
	int MasterServerResClosedRoom(int RoomNo, UINT ReqSequence);

	// 유저 한명이 방에서 나갔을때 해당 방의 포인터를 넘긴다.
	int MasterServerLeftUser(CRoom *pRoom);
	int MasterServerResLeftUser(int RoomNo, UINT ReqSequence);

	void MasterServerDisconnect();
	/////////////////////////////////////////////////////////////////////////////////////////////
private:
	int BattleServerNo;
	char ConnectToken[32];
	char MasterToken[32];
	WCHAR BattleServerIP[16];
	WORD Port;

	LONG64 ConnectTokenUpdateCount;

	ChatServer _ChatServer;
	MasterServer _MasterServer;
	CRoom* Rooms;

	////////////////////////////////////////////////////////////
	// HTTP 샤딩 API 전송용
	WCHAR shAPIURL[100];

	int httpPort;
	WCHAR httpURL[100];
	WCHAR httpPath[20000];
	////////////////////////////////////////////////////////////

	int MaxSession;
	ULONG64 TickCount;

	////////////////////////////////////////////////////////////
	// 모니터링
	LONG64 OpenRoom;
	LONG64 ReadyRoom;
	LONG64 GameRoom;

	shared_ptr<CMornitoringAgent> MonitorSenderAgent;
	shared_ptr<CMornitoring> pMonitorAgent;
	////////////////////////////////////////////////////////////

	int MAX_USER;
	int MAX_BATTLEROOM;
	UINT Ver_Code;
};

