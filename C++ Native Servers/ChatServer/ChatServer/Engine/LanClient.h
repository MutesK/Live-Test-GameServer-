#pragma once

//////////////////////////////////////////////////////////////////////////////////
//	광범위용 서버 클래스
// LanServer와 완전비슷하지만, CPacketBuffer에서 패킷 암호화 및 복호화 기능이 포함되어 있다.
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


class CLanClient
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
		CQueue<CPacketBuffer *> PeekQ;
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
		en_MAX_WORKERTHREAD = 1,

		en_TYPESEND = 300,
		en_TYPERECV,
		en_TYPECONN,

		en_MAX_SENDBUF = 500,  // 한 세션당 100회 최대 전송버퍼를 가진다.
		en_MAX_RECVBYTES = 1300
	};


	WCHAR			  _szIP[16];			// Host IP
	bool			  m_bNagleOption;		// 네이글 옵션
	UINT			  m_Port;				// 포트번호
	UINT			  m_WorkerCount;		// 워커스레드 갯수

	HANDLE			  m_hWorkerThread;
	HANDLE			  m_hSendEvent;

	HANDLE			  m_IOCP;
	SOCKADDR_IN		  m_ServerAddr;

	HANDLE			  m_SendThread;			// Send Thread
	int				  m_iErrorCode;
	CSession		  _Session;
protected:
	enum ErrorCode
	{
		en_PORT_OPEN = 20000,
		en_BUFFER_ERROR,
		en_DECODE_PACKET_ERROR,
		en_SESSIONID_NOT_MATCH,
	};
	//////////////////////////////////////////////////////////////////
	//	모니터링 정보 출력용
	//////////////////////////////////////////////////////////////////
	long			m_RecvPacketTPS, m_SendPacketTPS;
	bool		    m_bOpen;


public:
	CLanClient();
	virtual ~CLanClient();


	virtual bool Connect(WCHAR *szOpenIP, int port, bool nagleOption);

	///////////////////////////////////////////////////////////////////////
	// 워커스레드, AccpetThread가 끝이 났는지 확인해야된다.
	// 끝이 났다면 모든소켓을 Shutdown 처리한다.
	///////////////////////////////////////////////////////////////////////
	virtual void Stop();


	///////////////////////////////////////////////////////////////////////
	// 버퍼 만큼 전송
	///////////////////////////////////////////////////////////////////////
	int SendPacket(CPacketBuffer *pInBuffer);
	int SendPacketAndShutdown(CPacketBuffer *pInBuffer);

	int ClientGetLastError();
private:
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

	bool Disconnect();

	///////////////////////////////////////////////////////////////////////
	// Recv, Send Post 함수
	// 리턴  TRUE : 해당 세션이 Disconnect 상태가 됨.
	// 리턴	 FALSE : Disconnect 상태가 되지 않음.
	// 참고. 리턴은 성공여부가 아니다.
	///////////////////////////////////////////////////////////////////////
	bool			RecvPost();
	bool			SendPost();

	virtual void OnConnected() = 0;
	virtual void OnDisconnect() = 0;

	virtual void OnRecv(CPacketBuffer *pOutBuffer) = 0;
	virtual void OnSend(int SendSize) = 0;


	virtual void OnError(int errorcode, WCHAR *) = 0;

private:
	bool RecvProc(DWORD cbTransffered);
	bool SendProc(DWORD cbTransffered);

	///////////////////////////////////////////////////////////////////////
	// 외부 게임 스레드에 의해 동기화가 깨지는 것을 방지하기 위해 만들어지는 함수
	// 함수 이름은 락이지만 실제로는 락을 쓰지 않음.
	// 실제로는 UseFlag과 IOCount로 릴리즈를 막는다.
	///////////////////////////////////////////////////////////////////////
	bool		SessionAcquireLock();
	void        SessionAcquireFree();
protected:
	bool SessionShutdown();
private:
	bool ErrorCheck(int errorcode, int TransType);
};