#pragma once

//////////////////////////////////////////////////////////////////////////////////
//	�������� ���� Ŭ����
// LanServer�� �������������, CPacketBuffer���� ��Ŷ ��ȣȭ �� ��ȣȭ ����� ���ԵǾ� �ִ�.
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
		en_MAX_WORKERTHREAD = 1,

		en_TYPESEND = 300,
		en_TYPERECV,
		en_TYPECONN,

		en_MAX_SENDBUF = 500,  // �� ���Ǵ� 100ȸ �ִ� ���۹��۸� ������.
		en_MAX_RECVBYTES = 1300
	};


	WCHAR			  _szIP[16];			// Host IP
	bool			  m_bNagleOption;		// ���̱� �ɼ�
	UINT			  m_Port;				// ��Ʈ��ȣ
	UINT			  m_WorkerCount;		// ��Ŀ������ ����

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
	//	����͸� ���� ��¿�
	//////////////////////////////////////////////////////////////////
	long			m_RecvPacketTPS, m_SendPacketTPS;
	bool		    m_bOpen;


public:
	CLanClient();
	virtual ~CLanClient();


	virtual bool Connect(WCHAR *szOpenIP, int port, bool nagleOption);

	///////////////////////////////////////////////////////////////////////
	// ��Ŀ������, AccpetThread�� ���� ������ Ȯ���ؾߵȴ�.
	// ���� ���ٸ� �������� Shutdown ó���Ѵ�.
	///////////////////////////////////////////////////////////////////////
	virtual void Stop();


	///////////////////////////////////////////////////////////////////////
	// ���� ��ŭ ����
	///////////////////////////////////////////////////////////////////////
	int SendPacket(CPacketBuffer *pInBuffer);
	int SendPacketAndShutdown(CPacketBuffer *pInBuffer);

	int ClientGetLastError();
private:
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

	bool Disconnect();

	///////////////////////////////////////////////////////////////////////
	// Recv, Send Post �Լ�
	// ����  TRUE : �ش� ������ Disconnect ���°� ��.
	// ����	 FALSE : Disconnect ���°� ���� ����.
	// ����. ������ �������ΰ� �ƴϴ�.
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
	// �ܺ� ���� �����忡 ���� ����ȭ�� ������ ���� �����ϱ� ���� ��������� �Լ�
	// �Լ� �̸��� �������� �����δ� ���� ���� ����.
	// �����δ� UseFlag�� IOCount�� ����� ���´�.
	///////////////////////////////////////////////////////////////////////
	bool		SessionAcquireLock();
	void        SessionAcquireFree();
protected:
	bool SessionShutdown();
private:
	bool ErrorCheck(int errorcode, int TransType);
};