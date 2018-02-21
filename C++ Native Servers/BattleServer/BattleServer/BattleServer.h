#pragma once

#include "CommonProtocol.h"
#include "Engine/MMOServer.h"
#include "ContentsData.h"

/*
	1. ä�ü��� ����, ������ ���� ����
		ä�ü��� 1:1 ����, ������ ������ 1:N ����
		
	2. MMOServer ��ü ó��
	������ �÷��̾� ��Ŷ ó���� CGameServer Ŭ����
*/


class CBattleServer : public CMMOServer
{
public:
	CBattleServer(int MaxSession);
	virtual ~CBattleServer();

	virtual bool Start(WCHAR *szServerConfig, bool bNagle);
	void Stop(void);

	void Monitoring();
	void RoomStatusMonitoring();
	void HTTPURLParse();
private:
	CPlayer* Players;
public:
	/////////////////////////////////////////////////////////////////////////////
	// AUTH ��� ������Ʈ �̺�Ʈ ����ó����
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

	void    ResAuthRemoveUser(CPlayer *pPlayer);
	/////////////////////////////////////////////////////////////////////////////
	// GAME ��� ������Ʈ �̺�Ʈ ����ó����
	void	OnGame_Update(void);

	void	Game_CreateCharacter(CPlayer *pPlayer);
	void	Game_MoveCharacter(CPlayer *pPlayer);
	void	Game_HitPoint(CPlayer *pPlayer);
	void	Game_Fire1(CPlayer *pPlayer);
	void	Game_Reload(CPlayer *pPlayer);
	void	Game_HitDamage(CPlayer *pPlayer);

	void	ResCreateMyCharacter(CPlayer *pPlayer);
	void	ResCreateOtherCharacter(CPlayer *pPlayer);

	// ��Ʋ���� �濡�� ������ ����
	void    ResGameRemoveUser(CPlayer *pPlayer);
	/////////////////////////////////////////////////////////////////////////////
	// ���� ó����
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
	HANDLE hLoginThread[10];
	//////////////////////////////////////////////////////////////////////////////
public:
	void ConnectToken_RamdomGenerator();
	void RoomEnterToken_RamdomGenerator(CRoom *pRoom);
	//////////////////////////////////////////////////////////////
	// Player Ŭ���� ȣ��
	// ���� ���� ��Ʈ
	//////////////////////////////////////////////////////////////
	void DisconnectedPlayer(CPlayer *pPlayer);
private:
	virtual void	OnError(int iErrorCode, WCHAR *szError);
public:
///////////////////////////////////////////////////////////////////////////////////////
// ���� ������ ��� �Լ�
///////////////////////////////////////////////////////////////////////////////////////
	//	ä�ü����� ��Ʋ�������� ���� ���� �˸�
	int ChatServerReqServerOn(ULONG64 ChatSessionID, WCHAR *pServerIp, WORD ChatPort);

	int ChatServerReqConnectToken();
	int ChatServerResConnectToken(UINT ReqSequence);

	// �� ������ �ش� ���� �����͸� �ѱ��.
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

	// �� ������ �ش� ���� �����͸� �ѱ��.
	int MasterServerCreateRoom(CRoom *pRoom);
	int MasterServerResCreateRoom(int RoomNo, UINT ReqSequence);

	// �� ������ �ش� ���� �����͸� �ѱ��.
	int MasterServerClosedRoom(CRoom *pRoom);
	int MasterServerResClosedRoom(int RoomNo, UINT ReqSequence);

	// ���� �Ѹ��� �濡�� �������� �ش� ���� �����͸� �ѱ��.
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
	// HTTP ���� API ���ۿ�
	WCHAR shAPIURL[100];

	int httpPort;
	WCHAR httpURL[100];
	WCHAR httpPath[20000];
	////////////////////////////////////////////////////////////

	int MaxSession;
	ULONG64 TickCount;

	////////////////////////////////////////////////////////////
	// ����͸�
	LONG64 OpenRoom;
	LONG64 ReadyRoom;
	LONG64 GameRoom;
	////////////////////////////////////////////////////////////

	int MAX_USER;
	int MAX_BATTLEROOM;
	UINT Ver_Code;
};

