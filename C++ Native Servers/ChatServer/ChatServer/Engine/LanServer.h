#pragma once

//////////////////////////////////////////////////////////////////////////////////
//	광범위용 서버 클래스
// NetServer와 완전비슷하지만, CPacketBuffer에서 패킷 암호화 및 복호화 기능이 포함되어 있다.
//////////////////////////////////////////////////////////////////////////////////

// GQCS는 순서를 보장하지않는다.

#include "CommonInclude.h"

using namespace std;

// 소켓 에러를 제외한 나머지 에러코드
#define NO_SESSION_SPACE 10

#define INSERT_SESSION(Out_SESSIONID, InAINDEX, InUSERSESSION)   \
	(Out_SESSIONID) = 0;									     \
	(Out_SESSIONID) = ((Out_SESSIONID) | (InAINDEX)) << 48;		 \
	(Out_SESSIONID) |= (InUSERSESSION)						     \

// 세션ID 가져오는 매크로 함수
#define GET_SESSIONID(InSessionID)		 ((InSessionID) & 0x0000FFFFFFFFFFFF)
//#define GET_SESSIONARRAY(InSessionID, OutIndex)			(OutIndex) = ((InSessionID) >> 48)
#define GET_SESSIONARRAY(InSessionID)			((InSessionID) >> 48)
#define dfMAXSESSION 1152921504606846976
#define dfRELEASEFLAG 4294967296


class CLanServer
{
private:
	class CSession
	{
	public:
		struct st_Release
		{
			union
			{
				LONG64	Flag;
				struct
				{
					DWORD		IOCount;				// IO Reference Count
					DWORD		UseFlag;
				};
			};
		};

		CLockFreeQueue<CPacketBuffer *>SendQ;
		CLockFreeQueue<CPacketBuffer *>PeekQ;
		CStreamBuffer RecvQ;

		OVERLAPPED	  SendOverlapped;
		OVERLAPPED	  RecvOverlapped;

		ULONG64		  SessionID;			// 해당 값은 고윳값이여야 한다.
		SOCKET		  socket;

		LONG		SendFlag;				// 이중 Send 방지
		UINT		m_SendCount;

		st_Release	ReleaseFlag;


		int			_SendErrorCode;
		int			_RecvErrorCode;
		bool		m_bSendShutdown;
	};
	enum
	{
		en_MAX_WORKERTHREAD = 10,
		en_MAX_USER = 15000,

		en_TYPESEND = 300,
		en_TYPERECV,

		en_MAX_SENDBUF = 100,  // 한 세션당 100회 최대 전송버퍼를 가진다.
		en_MAX_RECVBYTES = 1300
	};



	bool			  m_bNagleOption;		// 네이글 옵션
	UINT			  m_Port;				// 포트번호
	UINT			  m_WorkerCount;		// 워커스레드 갯수

	HANDLE			  *m_pWorkerThreadArray;  // 워커스레드 배열
	HANDLE			  m_AcceptThread;		// Accept 파트 핸들

	HANDLE			  m_SendThread;			// Send Thread
	HANDLE			  m_SendEvent;
	SOCKET			  m_ListenSocket;

	HANDLE			  m_IOCP;

	/////////////////////////////////

	ULONG64			  m_UserSession;			// 고유값
	CSession		 *m_pSessionArray;			// 접속자 처리할 배열

	CLockFreeQueue<ULONG64> SendThreadQueue;
	CLockFreeStack <WORD> SessionIndex;

	BYTE m_PacketCode;
	BYTE m_PacketKey1;
	BYTE m_PacketKey2;
	BYTE m_iHeaderSize;

protected:
	enum ErrorCode
	{
		en_PORT_OPEN = 20000,
		en_BUFFER_ERROR,
		en_DECODE_PACKET_ERROR,
	};
	//////////////////////////////////////////////////////////////////
	//	모니터링 정보 출력용
	//////////////////////////////////////////////////////////////////
	ULONG64			m_AcceptTotal;
	long			m_AcceptTPS;
	long			m_RecvPacketTPS, m_SendPacketTPS;

	LONG			m_NowSession;			// 현재 접속자
	LONG			m_MaxSession;			// 최대 접속자

											// 열려있는지 닫혀있는지 판단 -> 이거로 쓰레드도 닫을꺼다.
	bool			  m_bServerOpen;
	//////////////////////////////////////////////////////////////////
	//	에러코드 저장용
	//////////////////////////////////////////////////////////////////
	int m_iErrorCode;

public:
	CLanServer();
	virtual ~CLanServer();

	///////////////////////////////////////////////////////////////////////
	// 서버를 시작한다.
	// 파라미터 : IP, 포트번호, 워커스레드갯수, Nagle옵션, 최대접속자
	// 리턴 :	서버 성공 : true, 서버 실패 : false
	///////////////////////////////////////////////////////////////////////
	virtual bool Start(WCHAR *szOpenIP, int port, int workerCount, bool nagleOption, int MaxSession);

	///////////////////////////////////////////////////////////////////////
	// 서버를 종료한다.
	// 워커스레드, AccpetThread가 끝이 났는지 확인해야된다.
	// 끝이 났다면 모든소켓을 Shutdown 처리한다.
	///////////////////////////////////////////////////////////////////////
	virtual void Stop();


	///////////////////////////////////////////////////////////////////////
	// 현재 접속중인 클라이언트의 수를 가져온다.
	///////////////////////////////////////////////////////////////////////
	UINT	GetSessionCount()
	{
		return m_NowSession;
	}

	///////////////////////////////////////////////////////////////////////
	// 버퍼 만큼 전송
	///////////////////////////////////////////////////////////////////////
	int SendPacket(ULONG64 SessionID, CPacketBuffer *pInBuffer);
	int SendPacketAndShutdown(ULONG64 SessionID, CPacketBuffer *pInBuffer);

	///////////////////////////////////////////////////////////////////////
	// 외부에서 선언하는 Disconnect -> closesocket 함수 호출한다.
	///////////////////////////////////////////////////////////////////////
	bool Disconnect(ULONG64& SessionID);
	int ServerGetLastError();

private:
	///////////////////////////////////////////////////////////////////////
	// 접속 수락 쓰레드
	///////////////////////////////////////////////////////////////////////
	static UINT WINAPI AcceptThread(LPVOID arg);
	UINT AcceptWork();


	///////////////////////////////////////////////////////////////////////
	// 워커 스레드
	///////////////////////////////////////////////////////////////////////
	static UINT WINAPI WorkerThread(LPVOID arg);
	UINT WorkerWork();


	///////////////////////////////////////////////////////////////////////
	// 전송 스레드
	///////////////////////////////////////////////////////////////////////
	static UINT WINAPI SendThread(LPVOID arg);
	UINT SendWork();


	///////////////////////////////////////////////////////////////////////
	// 종료할 클라이언트 번호를 통해 종료 시킨다. Shutdown함수 호출
	// TRUE 성공, FALSE 실패
	///////////////////////////////////////////////////////////////////////
	bool Disconnect(CSession *pSession);

	///////////////////////////////////////////////////////////////////////
	// Recv, Send Post 함수
	// 리턴  TRUE : 해당 세션이 Disconnect 상태가 됨.
	// 리턴	 FALSE : Disconnect 상태가 되지 않음.
	// 참고. 리턴은 성공여부가 아니다.
	///////////////////////////////////////////////////////////////////////
	bool			RecvPost(CSession *pSession);
	bool			SendPost(CSession *pSession);

	// 완료 통지 가상함수
	virtual void OnClientJoin(ULONG64 OutSessionID, WCHAR *pClientIP, int Port) = 0; // IP, PORT 추가
	virtual void OnClientLeave(ULONG64 SessionID) = 0;
	// Accept 직후 클라이언트 수용할것인지 안할것인지 검토
	// 리턴 : false 거부 true 수용
	virtual bool OnConnectionRequest(WCHAR *szIP, int Port) = 0;


	virtual void OnRecv(ULONG64 OutSessionID, CPacketBuffer *pOutBuffer) = 0;
	virtual void OnSend(ULONG64 OutSessionID, int SendSize) = 0;


	virtual void OnError(int errorcode, WCHAR *) = 0;

private:
	bool RecvProc(CSession *pSession, DWORD cbTransffered);
	bool SendProc(CSession *pSession, DWORD cbTransffered);

	void BufferClear(CSession *pSession);
	///////////////////////////////////////////////////////////////////////
	// 외부 게임 스레드에 의해 동기화가 깨지는 것을 방지하기 위해 만들어지는 함수
	// 함수 이름은 락이지만 실제로는 락을 쓰지 않음.
	// 실제로는 UseFlag과 IOCount로 릴리즈를 막는다.
	///////////////////////////////////////////////////////////////////////
	bool		SessionAcquireLock(ULONG64 SessionID);
	void        SessionAcquireFree(CSession *pSession);
private:
	///////////////////////////////////////////////////////////////////////
	// Session Array에 관한 함수
	///////////////////////////////////////////////////////////////////////
	bool AddSession(SOCKET socket, CSession **pOutSession);
	CSession* FindSession(ULONG64 SessionID);

	bool ErrorCheck(CSession *pSession, int errorcode, int TransType);

};

