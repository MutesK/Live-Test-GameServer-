#pragma once

//////////////////////////////////////////////////////////////////////////////////
//	�������� ���� Ŭ����
// NetServer�� �������������, CPacketBuffer���� ��Ŷ ��ȣȭ �� ��ȣȭ ����� ���ԵǾ� �ִ�.
//////////////////////////////////////////////////////////////////////////////////

// GQCS�� ������ ���������ʴ´�.

#include "CommonInclude.h"

using namespace std;

// ���� ������ ������ ������ �����ڵ�
#define NO_SESSION_SPACE 10

#define INSERT_SESSION(Out_SESSIONID, InAINDEX, InUSERSESSION)   \
	(Out_SESSIONID) = 0;									     \
	(Out_SESSIONID) = ((Out_SESSIONID) | (InAINDEX)) << 48;		 \
	(Out_SESSIONID) |= (InUSERSESSION)						     \

// ����ID �������� ��ũ�� �Լ�
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

		ULONG64		  SessionID;			// �ش� ���� �������̿��� �Ѵ�.
		SOCKET		  socket;

		LONG		SendFlag;				// ���� Send ����
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

		en_MAX_SENDBUF = 100,  // �� ���Ǵ� 100ȸ �ִ� ���۹��۸� ������.
		en_MAX_RECVBYTES = 1300
	};



	bool			  m_bNagleOption;		// ���̱� �ɼ�
	UINT			  m_Port;				// ��Ʈ��ȣ
	UINT			  m_WorkerCount;		// ��Ŀ������ ����

	HANDLE			  *m_pWorkerThreadArray;  // ��Ŀ������ �迭
	HANDLE			  m_AcceptThread;		// Accept ��Ʈ �ڵ�

	HANDLE			  m_SendThread;			// Send Thread
	HANDLE			  m_SendEvent;
	SOCKET			  m_ListenSocket;

	HANDLE			  m_IOCP;

	/////////////////////////////////

	ULONG64			  m_UserSession;			// ������
	CSession		 *m_pSessionArray;			// ������ ó���� �迭

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
	//	����͸� ���� ��¿�
	//////////////////////////////////////////////////////////////////
	ULONG64			m_AcceptTotal;
	long			m_AcceptTPS;
	long			m_RecvPacketTPS, m_SendPacketTPS;

	LONG			m_NowSession;			// ���� ������
	LONG			m_MaxSession;			// �ִ� ������

											// �����ִ��� �����ִ��� �Ǵ� -> �̰ŷ� �����嵵 ��������.
	bool			  m_bServerOpen;
	//////////////////////////////////////////////////////////////////
	//	�����ڵ� �����
	//////////////////////////////////////////////////////////////////
	int m_iErrorCode;

public:
	CLanServer();
	virtual ~CLanServer();

	///////////////////////////////////////////////////////////////////////
	// ������ �����Ѵ�.
	// �Ķ���� : IP, ��Ʈ��ȣ, ��Ŀ�����尹��, Nagle�ɼ�, �ִ�������
	// ���� :	���� ���� : true, ���� ���� : false
	///////////////////////////////////////////////////////////////////////
	virtual bool Start(WCHAR *szOpenIP, int port, int workerCount, bool nagleOption, int MaxSession);

	///////////////////////////////////////////////////////////////////////
	// ������ �����Ѵ�.
	// ��Ŀ������, AccpetThread�� ���� ������ Ȯ���ؾߵȴ�.
	// ���� ���ٸ� �������� Shutdown ó���Ѵ�.
	///////////////////////////////////////////////////////////////////////
	virtual void Stop();


	///////////////////////////////////////////////////////////////////////
	// ���� �������� Ŭ���̾�Ʈ�� ���� �����´�.
	///////////////////////////////////////////////////////////////////////
	UINT	GetSessionCount()
	{
		return m_NowSession;
	}

	///////////////////////////////////////////////////////////////////////
	// ���� ��ŭ ����
	///////////////////////////////////////////////////////////////////////
	int SendPacket(ULONG64 SessionID, CPacketBuffer *pInBuffer);
	int SendPacketAndShutdown(ULONG64 SessionID, CPacketBuffer *pInBuffer);

	///////////////////////////////////////////////////////////////////////
	// �ܺο��� �����ϴ� Disconnect -> closesocket �Լ� ȣ���Ѵ�.
	///////////////////////////////////////////////////////////////////////
	bool Disconnect(ULONG64& SessionID);
	int ServerGetLastError();

private:
	///////////////////////////////////////////////////////////////////////
	// ���� ���� ������
	///////////////////////////////////////////////////////////////////////
	static UINT WINAPI AcceptThread(LPVOID arg);
	UINT AcceptWork();


	///////////////////////////////////////////////////////////////////////
	// ��Ŀ ������
	///////////////////////////////////////////////////////////////////////
	static UINT WINAPI WorkerThread(LPVOID arg);
	UINT WorkerWork();


	///////////////////////////////////////////////////////////////////////
	// ���� ������
	///////////////////////////////////////////////////////////////////////
	static UINT WINAPI SendThread(LPVOID arg);
	UINT SendWork();


	///////////////////////////////////////////////////////////////////////
	// ������ Ŭ���̾�Ʈ ��ȣ�� ���� ���� ��Ų��. Shutdown�Լ� ȣ��
	// TRUE ����, FALSE ����
	///////////////////////////////////////////////////////////////////////
	bool Disconnect(CSession *pSession);

	///////////////////////////////////////////////////////////////////////
	// Recv, Send Post �Լ�
	// ����  TRUE : �ش� ������ Disconnect ���°� ��.
	// ����	 FALSE : Disconnect ���°� ���� ����.
	// ����. ������ �������ΰ� �ƴϴ�.
	///////////////////////////////////////////////////////////////////////
	bool			RecvPost(CSession *pSession);
	bool			SendPost(CSession *pSession);

	// �Ϸ� ���� �����Լ�
	virtual void OnClientJoin(ULONG64 OutSessionID, WCHAR *pClientIP, int Port) = 0; // IP, PORT �߰�
	virtual void OnClientLeave(ULONG64 SessionID) = 0;
	// Accept ���� Ŭ���̾�Ʈ �����Ұ����� ���Ұ����� ����
	// ���� : false �ź� true ����
	virtual bool OnConnectionRequest(WCHAR *szIP, int Port) = 0;


	virtual void OnRecv(ULONG64 OutSessionID, CPacketBuffer *pOutBuffer) = 0;
	virtual void OnSend(ULONG64 OutSessionID, int SendSize) = 0;


	virtual void OnError(int errorcode, WCHAR *) = 0;

private:
	bool RecvProc(CSession *pSession, DWORD cbTransffered);
	bool SendProc(CSession *pSession, DWORD cbTransffered);

	void BufferClear(CSession *pSession);
	///////////////////////////////////////////////////////////////////////
	// �ܺ� ���� �����忡 ���� ����ȭ�� ������ ���� �����ϱ� ���� ��������� �Լ�
	// �Լ� �̸��� �������� �����δ� ���� ���� ����.
	// �����δ� UseFlag�� IOCount�� ����� ���´�.
	///////////////////////////////////////////////////////////////////////
	bool		SessionAcquireLock(ULONG64 SessionID);
	void        SessionAcquireFree(CSession *pSession);
private:
	///////////////////////////////////////////////////////////////////////
	// Session Array�� ���� �Լ�
	///////////////////////////////////////////////////////////////////////
	bool AddSession(SOCKET socket, CSession **pOutSession);
	CSession* FindSession(ULONG64 SessionID);

	bool ErrorCheck(CSession *pSession, int errorcode, int TransType);

};

