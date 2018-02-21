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
	enSESSION_STATE _Mode;		// 세션의 상태모드

	int				_iArrayIndex;		//  세션 배열의 자기 인덱스
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
	// WSASend 동작상태 플래그.  WSASend 는 동시 불가능.
	//--------------------------------------------------------------
	LONG		_ISendIO;

	//--------------------------------------------------------------
	// IOCP IO 등록 Count.  0 이 되면 세션은 종료모드로 들어간다.
	//--------------------------------------------------------------
	LONG64		_IOCount;

	//--------------------------------------------------------------
	// 일정시간 이상 통신이 없는 세션은 끊어짐. 
	//--------------------------------------------------------------
	ULONG64					_LastRecvPacketTime;

	//--------------------------------------------------------------
	// IO Count 가 0 이 되는 순간 LogoutFlag 를 true 로 셋팅함.
	// 이에대한 종료 처리는 UpdateThread, AuthThread 내부에서 각각 진행.
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
	// 세션 배열의 인덱스 지정.  배열구조를 사용하며, 각 세션은 자신의 배열 인덱스를
	// 맴버로 가지고 있음. 이는 절대 변경해선 안됨.
	//////////////////////////////////////////////////////////////////////////
	void SetSessionArrayIndex(int iIndex);

	//////////////////////////////////////////////////////////////////////////
	// 게임 모드 변경
	//////////////////////////////////////////////////////////////////////////
	void		SetMode_GAME(void);

	//////////////////////////////////////////////////////////////////////////
	// 소켓 끊기.  셧다운 or 강제 끊기.
	//////////////////////////////////////////////////////////////////////////
	void		Disconnect(bool bForce = false);

	//////////////////////////////////////////////////////////////////////////
	// 해당 세션에 패킷 보내기.  내부에서 암호하 진행함
	//////////////////////////////////////////////////////////////////////////
	bool		SendPacket(CPacketBuffer *pPacket);

	//////////////////////////////////////////////////////////////////////////
	// 모든 Send 완료시 접속종료 설정.
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
	// 입출력 완료시 처리 함수.
	//////////////////////////////////////////////////////////////////////////
	void		CompleteRecv(DWORD dwTransferred);
	void		CompleteSend(DWORD dwTransferred);

	//////////////////////////////////////////////////////////////////////////
	// 이벤트 순수 가상 함수
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

	virtual void	OnError(int iErrorCode, WCHAR *szError) = 0;	// 에러 

	///////////////////////////////////////////////////////////////////
	// Auth,Game 스레드의 처리함수.
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



	// AUTH 모드 업데이트 이벤트 로직처리부
	virtual	void	OnAuth_Update(void) = 0;

	// GAME 모드 업데이트 이벤트 로직처리부
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
	CLockFreeQueue <st_CLIENT_CONNECT *> _AcceptSocketQueue;  // 신규접속 처리 큐

	//----------------------------------------------------------------------------
	// Auth 
	//----------------------------------------------------------------------------
	HANDLE	_hAuthThread;

	CLockFreeStack<int> _BlankSessionIndexStack;		// 빈 세션들어있는 스택


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
	// 모니터링 요소
	//----------------------------------------------------------------------------
	long		_Monitor_AcceptSocket;  // Accept에서 소켓을 받아드린 수치
	long		_Monitor_SessionAllMode; // 세션(게임모드이거나 인증모드) 모든 수치
	long		_Monitor_SessionAuthMode; // 인증 모드에 들어온 세션의 수치
	long		_Monitor_SessionGameMode; // 게임 모드로 들어온 세션의 수치

	long		_Monitor_Counter_AuthUpdate; // Auth Update 호출 카운터
	long		_Monitor_Counter_GameUpdate; // Game Update 호출 카운터
	long		_Monitor_Counter_Accept;  // Accept TPS 카운터
	long		_Monitor_Counter_PacketProc; // Packet Proc TPS 카운터
	long		_Monitor_Counter_PacketSend; // Packet Send TPS 카운터
};

