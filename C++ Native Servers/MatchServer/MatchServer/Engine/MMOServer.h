#pragma once

#include "CommonInclude.h"


#define NO_SESSION_SPACE 10

#define INSERT_SESSION(Out_SESSIONID, InAINDEX, InUSERSESSION)   \
	(Out_SESSIONID) = 0;									     \
	(Out_SESSIONID) = ((Out_SESSIONID) | (InAINDEX)) << 48;		 \
	(Out_SESSIONID) |= (InUSERSESSION)						     \


#define GET_SESSIONID(InSessionID)		 ((InSessionID) & 0x0000FFFFFFFFFFFF)
#define GET_SESSIONARRAY(InSessionID)			((InSessionID) >> 48)

enum enSESSION_STATE
{
	MODE_NONE,
	
	MODE_AUTH,
	MODE_AUTH_TO_GAME,
	MODE_GAME,

	MODE_LOGOUT_IN_AUTH,
	MODE_LOGOUT_IN_GAME,

	MODE_WAIT_LOGOUT
};

enum enErrorCode
{
	ACCEPT_INVALID_SOCKET = 10000,
	ACCEPT_SOCKET_ARRAY_ISFULL,

	NET_IO_NOBUFS,
	NET_IO_FAULT,
};

enum
{
	eWORKER_THREAD_MAX = 5,

	en_MAX_SENDBUF = 100,
	en_SESSION_MAX = 10000,
	eTIMEOUT = 9999999,

	eERROR_MAX_LEN = 1000
};

struct st_CLIENT_CONNECT
{
	WCHAR	_IP[16];
	USHORT	_Port;

	SOCKET	socket;
};

class CNetSession
{
private:
	enum TransType
	{
		eTypeSEND,
		eTypeRECV
	};
protected:
	enSESSION_STATE _Mode;		// ������ ���¸��

	int				_iArrayIndex;		//  ���� �迭�� �ڱ� �ε���
	st_CLIENT_CONNECT	_ClientInfo;

	CStreamBuffer					_RecvQ;
	CLockFreeQueue<CPacketBuffer *> _SendQ;
	CQueue	<CPacketBuffer *>		_PeekQ;
	CLockFreeQueue<CPacketBuffer *> _CompleteRecvQ;

	OVERLAPPED				_RecvOverlapped;
	OVERLAPPED				_SendOverlapped;

	int			_iSendPacketCnt;
	DWORD		_SendPacketSize;

	int			_ErrorCode;

	//--------------------------------------------------------------
	// WSASend ���ۻ��� �÷���.  WSASend �� ���� �Ұ���.
	//--------------------------------------------------------------
	LONG		_ISendIO;

	//--------------------------------------------------------------
	// IOCP IO ��� Count.  0 �� �Ǹ� ������ ������� ����.
	//--------------------------------------------------------------
	LONG64		_IOCount;

	//--------------------------------------------------------------
	// �����ð� �̻� ����� ���� ������ ������. 
	//--------------------------------------------------------------
	ULONG64					_LastRecvPacketTime;

	//--------------------------------------------------------------
	// IO Count �� 0 �� �Ǵ� ���� LogoutFlag �� true �� ������.
	// �̿����� ���� ó���� UpdateThread, AuthThread ���ο��� ���� ����.
	//--------------------------------------------------------------
	bool		_bLogoutFlag;
protected:
	bool		_bAuthToGameFlag;
private:
	friend class CMMOServer;
public:
	CNetSession();
	virtual ~CNetSession();

	//////////////////////////////////////////////////////////////////////////
	// ���� �迭�� �ε��� ����.  �迭������ ����ϸ�, �� ������ �ڽ��� �迭 �ε�����
	// �ɹ��� ������ ����. �̴� ���� �����ؼ� �ȵ�.
	//////////////////////////////////////////////////////////////////////////
	void SetSessionArrayIndex(int iIndex);

	//////////////////////////////////////////////////////////////////////////
	// ���� ��� ����
	//////////////////////////////////////////////////////////////////////////
	void		SetMode_GAME(void);

	//////////////////////////////////////////////////////////////////////////
	// ���� ����.  �˴ٿ� or ���� ����.
	//////////////////////////////////////////////////////////////////////////
	void		Disconnect(bool bForce = false);

	//////////////////////////////////////////////////////////////////////////
	// �ش� ���ǿ� ��Ŷ ������.  ���ο��� ��ȣ�� ������
	//////////////////////////////////////////////////////////////////////////
	bool		SendPacket(CPacketBuffer *pPacket);

	//////////////////////////////////////////////////////////////////////////
	// ��� Send �Ϸ�� �������� ����.
	//////////////////////////////////////////////////////////////////////////
	void		SendCompleteDisconnect(void);

private:
	void ErrorCheck(int errorCode, TransType Type);
	//////////////////////////////////////////////////////////////////////////
	// IO Post
	//////////////////////////////////////////////////////////////////////////
	bool		RecvPost(void);
	bool		SendPost(void);

	//////////////////////////////////////////////////////////////////////////
	// ����� �Ϸ�� ó�� �Լ�.
	//////////////////////////////////////////////////////////////////////////
	void		CompleteRecv(DWORD dwTransferred);
	void		CompleteSend(DWORD dwTransferred);

	//////////////////////////////////////////////////////////////////////////
	// �̺�Ʈ ���� ���� �Լ�
	//////////////////////////////////////////////////////////////////////////
	virtual	bool		OnAuth_SessionJoin(void) = 0;
	virtual	bool		OnAuth_SessionLeave(bool bToGame = false) = 0;
	virtual	bool		OnAuth_Packet(CPacketBuffer *pPacket) = 0;
	virtual	void		OnAuth_Timeout(void) = 0;

	virtual	bool		OnGame_SessionJoin(void) = 0;
	virtual	bool		OnGame_SessionLeave(void) = 0;
	virtual	bool		OnGame_Packet(CPacketBuffer *pPacket) = 0;
	virtual	void		OnGame_Timeout(void) = 0;

	virtual bool		OnGame_SessionRelease(void) = 0;
	virtual void		OnError(int ErrorCode, WCHAR *szStr) = 0;
};

//////////////////////////////////////////////////////////////////////////////////////////////

class CMMOServer
{
public:
	CMMOServer(int iMaxSession);
	virtual ~CMMOServer();

	virtual bool Start(WCHAR *szListenIP, int iPort, int iWorkerThread, bool bNagle, BYTE PacketCode, BYTE PacketKey1, BYTE PacketKey2);

	bool Stop(void);


	void SetSessionArray(int iArrayIndex, CNetSession* pSession);

	void SendPacket_GameAll(CPacketBuffer *pPacket, ULONG64 ClientID = 0);

	void SendPacket(CPacketBuffer *pPacket, ULONG64 ClientID);

private:

	void Error(int ErrorCode, WCHAR *szFormatStr, ...);

	virtual void	OnError(int iErrorCode, WCHAR *szError) = 0;	// ���� 

	///////////////////////////////////////////////////////////////////
	// Auth,Game �������� ó���Լ�.
	///////////////////////////////////////////////////////////////////
	void ProcAuth_Accept(void);
	void ProcAuth_Packet(void);
	void ProcAuth_Logout(void);
	void ProcAuth_TimeoutCheck(void);

	void ProcGame_AuthToGame(void);
	void ProcGame_Packet(void);
	void ProcGame_Logout(void);
	void ProcGame_TimeoutCheck(void);

	void ProcGame_Release(void);



	// AUTH ��� ������Ʈ �̺�Ʈ ����ó����
	virtual	void	OnAuth_Update(void) = 0;

	// GAME ��� ������Ʈ �̺�Ʈ ����ó����
	virtual	void	OnGame_Update(void) = 0;


	//----------------------------------------------------------------------------
	// NetWork 
	//----------------------------------------------------------------------------
protected:
	bool		_bServerOpen;

	int		_iNowSession;
	int		_iMaxSession;
protected:
	SOCKET		_ListenSocket;

	bool		_bEnableNagle;
	int		_iWorkerThread;

	WCHAR		_szListenIP[16];
	int		_iListenPort;

	//----------------------------------------------------------------------------
	// Accept 
	//----------------------------------------------------------------------------
	HANDLE	_hAcceptThread;

	CMemoryPool<st_CLIENT_CONNECT>		 _ClientConnectPool;
	CLockFreeQueue <st_CLIENT_CONNECT *> _AcceptSocketQueue;  // �ű����� ó�� ť

	//----------------------------------------------------------------------------
	// Auth 
	//----------------------------------------------------------------------------
	HANDLE	_hAuthThread;

	CLockFreeStack<int> _BlankSessionIndexStack;		// �� ���ǵ���ִ� ����


	//---------------------------------------------------------------------------
	// GameUpdate 
	//---------------------------------------------------------------------------
	HANDLE _hGameUpdateThread;


	//---------------------------------------------------------------------------
	// IOCP 
	//---------------------------------------------------------------------------
	HANDLE _hIOCPWorkerThread[eWORKER_THREAD_MAX];
	HANDLE _hIOCP;

	//----------------------------------------------------------------------------
	// Send 
	//----------------------------------------------------------------------------
	HANDLE		_hSendThread;

	CNetSession	**_pSessionArray;

	WCHAR		_szErrorStr[dfLOGMSGLEN_MAX];

private:
	static UINT WINAPI	AcceptThread(LPVOID arg);
	bool				AcceptThread_Work();

	static UINT WINAPI	AuthThread(LPVOID arg);
	bool				AuthThread_Work();

	static UINT	WINAPI GameUpdateThread(LPVOID arg);
	bool			   GameUpdateThread_Work();

	static UINT WINAPI IOCPWorkerThread(LPVOID arg);
	bool			   IOCPWorker_Work();

	static UINT WINAPI SendThread(LPVOID arg);
	bool			   SendThread_Work();

public:
	//----------------------------------------------------------------------------
	// ����͸� ���
	//----------------------------------------------------------------------------
	long		_Monitor_AcceptSocket;  // Accept���� ������ �޾Ƶ帰 ��ġ
	long		_Monitor_SessionAllMode; // ����(���Ӹ���̰ų� �������) ��� ��ġ
	long		_Monitor_SessionAuthMode; // ���� ��忡 ���� ������ ��ġ
	long		_Monitor_SessionGameMode; // ���� ���� ���� ������ ��ġ

	long		_Monitor_Counter_AuthUpdate; // Auth Update ȣ�� ī����
	long		_Monitor_Counter_GameUpdate; // Game Update ȣ�� ī����
	long		_Monitor_Counter_Accept;  // Accept TPS ī����
	long		_Monitor_Counter_PacketProc; // Packet Proc TPS ī����
	long		_Monitor_Counter_PacketSend; // Packet Send TPS ī����
};

