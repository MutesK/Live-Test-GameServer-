#pragma comment(lib, "ws2_32")
#pragma comment(lib, "Winmm.lib")

#include <winsock2.h>
#include <WS2tcpip.h>
#include "LanClient.h"

CLanClient::CLanClient()
{
	m_bOpen = false;
	m_hSendEvent = CreateEvent(nullptr, false, false, nullptr);
}
CLanClient::~CLanClient()
{
}
bool CLanClient::RecvPost()
{
	ZeroMemory(&_Session.RecvOverlapped, sizeof(OVERLAPPED));

	WSABUF wsabuf[2];
	int bufcount = 1;
	wsabuf[0].buf = _Session.RecvQ.GetWriteBufferPtr();
	wsabuf[0].len = _Session.RecvQ.GetNotBrokenPutSize();

	if (wsabuf[0].len < _Session.RecvQ.GetFreeSize())
	{
		bufcount++;
		wsabuf[1].buf = _Session.RecvQ.GetBufferPtr();
		wsabuf[1].len = _Session.RecvQ.GetFreeSize() - wsabuf[0].len;
	}

	InterlockedIncrement(&_Session.ReleaseFlag.IOCount);

	DWORD RecvSize = 0;
	DWORD Flag = 0;

	int ret = WSARecv(_Session.socket, wsabuf, bufcount, &RecvSize, &Flag, &_Session.RecvOverlapped, nullptr);

	if (ret == SOCKET_ERROR)
		return ErrorCheck(WSAGetLastError(), en_TYPERECV);

	return false;
}
bool CLanClient::SendPost()
{
	while (1)
	{
		if (InterlockedCompareExchange(&_Session.SendFlag, true, false) != false)
			return false;

		if (_Session.SendQ.GetUseSize() <= 0)
		{
			InterlockedExchange(&_Session.SendFlag, false);

			if (_Session.SendQ.GetUseSize() <= 0)
				return false;

			continue;
		}

		PRO_BEGIN(L"WSASend");
		ZeroMemory(&_Session.SendOverlapped, sizeof(OVERLAPPED));

		///////////////////////////////////////////////////////////////////////////////
		WSABUF wsabuf[en_MAX_SENDBUF];
		int bufcount = 0;

		CPacketBuffer *pBuffer;

		while (_Session.PeekQ.Dequeue(&pBuffer))
			_Session.SendQ.Enqueue(&pBuffer);

		while (_Session.SendQ.Dequeue(&pBuffer))
		{
			_Session.PeekQ.Enqueue(&pBuffer);

			wsabuf[bufcount].buf = pBuffer->GetHeaderPtr();
			wsabuf[bufcount].len = pBuffer->GetHeaderDataUseSize();
			bufcount++;

			if (bufcount >= en_MAX_SENDBUF)
				break;
		}

		_Session.m_SendCount = bufcount;


		InterlockedIncrement(&_Session.ReleaseFlag.IOCount);

		DWORD SendSize = 0;
		DWORD Flag = 0;

		int ret = WSASend(_Session.socket, wsabuf, bufcount, &SendSize, Flag, &_Session.SendOverlapped, nullptr);

		PRO_END(L"WSASend");

		if (SendSize == 0)
		{
			wprintf(L"Error Send\n");
		}

		if (ret == SOCKET_ERROR)
			return ErrorCheck(WSAGetLastError(), en_TYPESEND);

		return true;
	}
}
bool CLanClient::Connect(WCHAR *szOpenIP, int port, bool nagleOption)
{

	if (m_bOpen)
	{
		m_iErrorCode = en_PORT_OPEN;
		OnError(m_iErrorCode, L"Already Client is Open");
		return false;
	}

	m_iErrorCode = 0;
	m_WorkerCount = 1;
	m_Port = port;
	m_bNagleOption = nagleOption;
	lstrcpyW(_szIP, szOpenIP);


	m_RecvPacketTPS = m_SendPacketTPS = 0;


	// 클라이언트 네트워크 설정한다.

	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	ZeroMemory(&m_ServerAddr, sizeof(m_ServerAddr));
	m_ServerAddr.sin_family = AF_INET;
	InetPton(AF_INET, szOpenIP, &m_ServerAddr.sin_addr);
	m_ServerAddr.sin_port = htons(m_Port);

	SOCKET _socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (_socket == INVALID_SOCKET)
	{
		m_iErrorCode = WSAGetLastError();
		OnError(m_iErrorCode, L"Socket Error");
		return false;
	}

	if (!m_bNagleOption)
	{
		int opt = 1;
		setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(int));
	}

	int err = connect(_socket, (SOCKADDR *)&m_ServerAddr, sizeof(SOCKADDR_IN));
	if (err == SOCKET_ERROR)
	{
		ErrorCheck(WSAGetLastError(), en_TYPECONN);
		return false;
	}
	m_bOpen = true;
	m_IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);

	m_SendThread = (HANDLE)_beginthreadex(nullptr, 0, SendThread, this, 0, nullptr);

	m_hWorkerThread = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, this, 0, nullptr);
	CloseHandle(m_hWorkerThread);

	//////////////////////////////////////////////////////////////////
	// IOCP에 등록한다.
	//////////////////////////////////////////////////////////////////
	CreateIoCompletionPort((HANDLE)_socket, m_IOCP, (ULONG64)this, 0);
	_Session.socket = _socket;
	_Session.ReleaseFlag.Flag = 0;
	_Session.SendFlag = 0;
	_Session.m_SendCount = 0;
	_Session.m_bSendShutdown = false;

	//m_AcceptTPS++;
	InterlockedCompareExchange(&_Session.ReleaseFlag.UseFlag, 1, 0);
	InterlockedIncrement(&_Session.ReleaseFlag.IOCount);
	OnConnected();
	RecvPost();
	if (InterlockedDecrement(&_Session.ReleaseFlag.IOCount) == 0)
	{
		Disconnect();
		return false;
	}


	return true;
}
void CLanClient::Stop()
{
	////////// 미완성
	m_bOpen = false;

	Disconnect();

	SetEvent(m_hSendEvent);
	PostQueuedCompletionStatus(m_IOCP, 0, 0, 0);

	WaitForSingleObject(m_SendThread, INFINITE);

	SYSLOG(L"DEBUG", LOG_DEBUG, L"Client Stop");

	OnDisconnect();
}
bool CLanClient::Disconnect()
{
	if (InterlockedCompareExchange64(&_Session.ReleaseFlag.Flag, 0, dfRELEASEFLAG) == dfRELEASEFLAG)
	{
		shutdown(_Session.socket, SD_BOTH);
		closesocket(_Session.socket);
		_Session.socket = INVALID_SOCKET;

		_Session.RecvQ.ClearBuffer();
		CPacketBuffer *pBuffer;
		while (_Session.PeekQ.Dequeue(&pBuffer))
			_Session.SendQ.Enqueue(&pBuffer);

		while (_Session.SendQ.Dequeue(&pBuffer))
			pBuffer->Free();

		if(m_bOpen)
			Stop();

		return true;
	}

	return false;
}
int CLanClient::SendPacket(CPacketBuffer *pInBuffer)
{

	if (SessionAcquireLock() != true)
		return 0;

	st_PACKET_HEADER Header;
	////////////////////////////////////////////
	PRO_BEGIN(L"SendPacket");
	pInBuffer->AddRefCount();

	pInBuffer->ShortSizeHeaderAlloc();

	if (pInBuffer->GetLastError() != 0)
		return false;

	if (pInBuffer->GetUseSize() == 0)
		return false;

	_Session.SendQ.Enqueue(&pInBuffer);
	SetEvent(m_hSendEvent);

	pInBuffer->Free();
	PRO_END(L"SendPacket");

	////////////////////////////////////////////
	SessionAcquireFree();

	return true;
}

int CLanClient::SendPacketAndShutdown(CPacketBuffer *pInBuffer)
{
	int ret = SendPacket(pInBuffer);

	if (ret != true)
		return ret;

	_Session.m_bSendShutdown = true;

	return true;
}

UINT WINAPI CLanClient::WorkerThread(LPVOID arg)
{
	CLanClient *pClient = reinterpret_cast<CLanClient *>(arg);

	return pClient->WorkerWork();
}
UINT CLanClient::WorkerWork()
{
	DWORD cbTransffered;
	LPOVERLAPPED pOverlapped;
	CSession *pSession;

	while (1)
	{
		cbTransffered = 0;
		pOverlapped = nullptr;
		pSession = nullptr;

		PRO_END(L"WorkerThread");
		//	OnWorkerThreadEnd();
		int retval = GetQueuedCompletionStatus(m_IOCP, &cbTransffered, (PULONG_PTR)&pSession, &pOverlapped, INFINITE);
		//	OnWorkerThreadBegin();
		PRO_BEGIN(L"WorkerThread");

		// 워커스레드 정상종료 체크
		if (pOverlapped == nullptr && pSession == nullptr && cbTransffered == 0)
		{
			PostQueuedCompletionStatus(m_IOCP, 0, 0, 0);
			//	OnWorkerThreadEnd();
			break;
		}

		if (pSession == nullptr)
			continue;

		// 소켓 종료
		if (cbTransffered == 0)
		{
			shutdown(_Session.socket, SD_BOTH);
		}
		else
			// 완료통지
		{
			if (!_Session.ReleaseFlag.UseFlag)
				continue;

			if (pOverlapped == &_Session.RecvOverlapped)
				RecvProc(cbTransffered);
			
			else if (pOverlapped == &_Session.SendOverlapped)
				SendProc(cbTransffered);
		}

		if (InterlockedDecrement(&_Session.ReleaseFlag.IOCount) == 0)
			Disconnect();


		Sleep(0);
	}

	return 1;
}


UINT WINAPI CLanClient::SendThread(LPVOID arg)
{
	CLanClient *pClient = reinterpret_cast<CLanClient *>(arg);

	return pClient->SendWork();
}
UINT CLanClient::SendWork()
{
	CSession *pSession = nullptr;
	ULONG64 SessionID;
	while (m_bOpen)
	{
		WaitForSingleObject(m_hSendEvent, INFINITE);
		if (_Session.ReleaseFlag.UseFlag == 0 || _Session.SendQ.GetUseSize() == 0)
			continue;

		SendPost();
	}

	return 1;
}
bool CLanClient::RecvProc(DWORD cbTransffered)
{
	_Session.RecvQ.MoveWritePos(cbTransffered);

	while (1)
	{
		m_RecvPacketTPS++;

		// 받은 내용을 검사해야된다. 최소한 패킷헤더 이상을 가지고있어야 처리된다.
		if (_Session.RecvQ.GetUseSize() <= sizeof(short))
			break; // 더이상 처리할 패킷없음.

		short Len;
		_Session.RecvQ.Peek(reinterpret_cast<char *>(&Len), sizeof(short));

		if (_Session.RecvQ.GetUseSize() < Len)
			break; // 더이상 처리할 패킷없음.

		_Session.RecvQ.RemoveData(2);

		if (Len > en_MAX_RECVBYTES)
		{
			SessionShutdown();
			return false;
		}


		// 패킷코드에서 통과했다면, Len만큼 PacketBuffer를 만든다.
		CPacketBuffer *pBuffer = CPacketBuffer::Alloc();
		if (_Session.RecvQ.GetData(pBuffer->GetWriteBufferPtr(), Len) != Len)
			break;

		pBuffer->MoveWritePos(Len);

		// 여기까지 오면 사실상 정상패킷처리해준다.
		pBuffer->AddRefCount();
		OnRecv(pBuffer);
		pBuffer->Free();
	}

	RecvPost();

	return true;
}
bool CLanClient::SendProc(DWORD cbTransffered)
{
	// Send 처리
	////////////////////////////////////////////
	CPacketBuffer *pTemp;

	for (int i = 0; i < _Session.m_SendCount; i++)
	{
		_Session.PeekQ.Dequeue(&pTemp);
		OnSend(pTemp->GetUseSize());

		pTemp->Free();

		m_SendPacketTPS++;
	}

	while (_Session.PeekQ.Dequeue(&pTemp))
		_Session.SendQ.Enqueue(&pTemp);

	InterlockedExchange(&_Session.SendFlag, false);


	if (_Session.m_bSendShutdown && _Session.SendQ.GetUseSize() == 0)
	{
		SessionShutdown();
		return true;
	}

	if (_Session.SendQ.GetUseSize() > 0)
		SetEvent(m_hSendEvent);

	return true;
}

bool CLanClient::ErrorCheck(int errorcode, int TransType)
{
	m_iErrorCode = errorcode;

	if (TransType == en_TYPESEND)
		_Session._SendErrorCode = errorcode;
	else
		_Session._RecvErrorCode = errorcode;


	bool ret = false;

	if (errorcode != WSA_IO_PENDING && errorcode != 997)
	{
		if (InterlockedDecrement(&_Session.ReleaseFlag.IOCount) == 0)
		{
			Disconnect();
			return true;
		}


		WCHAR szMessage[250];

		switch (errorcode)
		{
		case WSAENOBUFS:
			StringCchPrintf(szMessage, 250, L"Socket : %llu, %s : WSAENOBUF Error", _Session.socket, TransType == en_TYPESEND ? L"Send" : L"Recv");
			OnError(errorcode, szMessage);
			shutdown(_Session.socket, SD_BOTH);
			break;
		case WSAEFAULT:
			StringCchPrintf(szMessage, 250, L"Socket : %llu, %s : WSAEFAULT(Buffer) Error", _Session.socket, TransType == en_TYPESEND ? L"Send" : L"Recv");
			OnError(errorcode, szMessage);
			shutdown(_Session.socket, SD_BOTH);
			break;
		}

	}

	return ret;
}

bool CLanClient::SessionAcquireLock()
{

	InterlockedIncrement(&_Session.ReleaseFlag.IOCount);

	if (_Session.ReleaseFlag.IOCount == 1)
	{
		if (InterlockedDecrement(&_Session.ReleaseFlag.IOCount) == 0)
			Disconnect();

		return false;
	}

	if (_Session.ReleaseFlag.UseFlag == 0)
	{
		if (InterlockedDecrement(&_Session.ReleaseFlag.IOCount) == 0)
			Disconnect();

		return false;
	}

	return true;
}

void  CLanClient::SessionAcquireFree()
{
	if (InterlockedDecrement(&_Session.ReleaseFlag.IOCount) == 0)
		Disconnect();
}

int CLanClient::ClientGetLastError()
{
	return m_iErrorCode;
}

bool CLanClient::SessionShutdown()
{
	CancelIoEx(reinterpret_cast<HANDLE>(_Session.socket), &_Session.RecvOverlapped);
	CancelIoEx(reinterpret_cast<HANDLE>(_Session.socket), &_Session.SendOverlapped);

	shutdown(_Session.socket, SD_BOTH);

	return true;
}

