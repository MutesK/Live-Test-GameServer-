#pragma comment(lib, "ws2_32")
#pragma comment(lib, "Winmm.lib")

#include <winsock2.h>
#include <WS2tcpip.h>
#include "LanServer.h"

CLanServer::CLanServer()
{
	m_bServerOpen = false;
	m_SendEvent = CreateEvent(nullptr, false, false, nullptr);
}
CLanServer::~CLanServer()
{
}
bool CLanServer::RecvPost(CSession *pSession)
{
	ZeroMemory(&pSession->RecvOverlapped, sizeof(OVERLAPPED));

	WSABUF wsabuf[2];
	int bufcount = 1;
	wsabuf[0].buf = pSession->RecvQ.GetWriteBufferPtr();
	wsabuf[0].len = pSession->RecvQ.GetNotBrokenPutSize();

	if (wsabuf[0].len < pSession->RecvQ.GetFreeSize())
	{
		bufcount++;
		wsabuf[1].buf = pSession->RecvQ.GetBufferPtr();
		wsabuf[1].len = pSession->RecvQ.GetFreeSize() - wsabuf[0].len;
	}


	DWORD RecvSize = 0;
	DWORD Flag = 0;

	InterlockedIncrement(&pSession->ReleaseFlag.IOCount);
	int ret = WSARecv(pSession->socket, wsabuf, bufcount, &RecvSize, &Flag, &pSession->RecvOverlapped, nullptr);

	if (ret == SOCKET_ERROR)
		return ErrorCheck(pSession, WSAGetLastError(), en_TYPERECV);

	return false;
}
bool CLanServer::SendPost(CSession *pSession)
{
	while (1)
	{
		if (pSession->ReleaseFlag.UseFlag == 0)
			return false;

		if (InterlockedCompareExchange(&pSession->SendFlag, true, false) != false)
			return false;

		if (pSession->SendQ.GetUseSize() <= 0)
		{
			InterlockedExchange(&pSession->SendFlag, false);

			if (pSession->SendQ.GetUseSize() <= 0)
				return false;

			continue;
		}

		if (pSession->ReleaseFlag.UseFlag == 0)
		{
			InterlockedExchange(&pSession->SendFlag, false);
			return false;
		}

		ZeroMemory(&pSession->SendOverlapped, sizeof(OVERLAPPED));

		WSABUF wsabuf[en_MAX_SENDBUF];
		int bufcount = 0;
		CPacketBuffer *pBuffer;

		while (pSession->SendQ.Dequeue(&pBuffer))
		{
			pSession->PeekQ.Enqueue(&pBuffer);

			wsabuf[bufcount].buf = pBuffer->GetHeaderPtr();
			wsabuf[bufcount].len = pBuffer->GetHeaderDataUseSize();
			bufcount++;

			if (bufcount >= en_MAX_SENDBUF)
				break;
		}

		pSession->m_SendCount = bufcount;

		DWORD SendSize = 0;
		DWORD Flag = 0;

		InterlockedIncrement(&pSession->ReleaseFlag.IOCount);
		int ret = WSASend(pSession->socket, wsabuf, bufcount, &SendSize, Flag, &pSession->SendOverlapped, nullptr);

		if (ret == SOCKET_ERROR)
			return ErrorCheck(pSession, WSAGetLastError(), en_TYPESEND);

		return true;

	}
}
bool CLanServer::Start(WCHAR *szOpenIP, int port, int workerCount, bool nagleOption, int MaxSession)
{

	if (m_bServerOpen)
	{
		m_iErrorCode = en_PORT_OPEN;
		OnError(m_iErrorCode, L"Already Server is Open");
		return false;
	}


	m_iErrorCode = 0;

	m_Port = port;
	m_WorkerCount = min(workerCount, en_MAX_WORKERTHREAD);
	m_MaxSession = min(MaxSession, en_MAX_USER);
	m_bNagleOption = nagleOption;
	m_UserSession = 1;
	m_NowSession = 0;


	m_AcceptTPS = m_RecvPacketTPS = m_SendPacketTPS = 0;
	m_AcceptTotal = 0;

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		m_iErrorCode = WSAGetLastError();
		OnError(m_iErrorCode, L"WSA Startup Error");
		return false;
	}

	m_ListenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (m_ListenSocket == INVALID_SOCKET)
	{
		m_iErrorCode = WSAGetLastError();
		OnError(m_iErrorCode, L"Listen Socket Error");
		return false;
	}

	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	InetPton(AF_INET, szOpenIP, &serveraddr.sin_addr);
	serveraddr.sin_port = htons(port);

	int ret = bind(m_ListenSocket, (SOCKADDR *)&serveraddr, sizeof(serveraddr));
	if (ret == SOCKET_ERROR)
	{
		m_iErrorCode = WSAGetLastError();
		OnError(m_iErrorCode, L"Bind Error");
		return false;
	}
	ret = listen(m_ListenSocket, SOMAXCONN);
	if (ret == SOCKET_ERROR)
	{
		m_iErrorCode = WSAGetLastError();
		OnError(m_iErrorCode, L"Listen Error");
		return false;
	}


	m_pSessionArray = new CSession[m_MaxSession];
	for (int i = 0; i < m_MaxSession; i++)
	{
		m_pSessionArray[i].ReleaseFlag.Flag = 0;
		m_pSessionArray[i].SendFlag = 0;
		m_pSessionArray[i].socket = INVALID_SOCKET;
		m_pSessionArray[i].SessionID = 0;
	}

	m_UserSession = 1;

	for (int i = 0; i < m_MaxSession; i++)
		SessionIndex.Push(i);

	m_bServerOpen = true;
	m_pWorkerThreadArray = new HANDLE[m_WorkerCount];
	m_IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);

	m_SendThread = (HANDLE)_beginthreadex(nullptr, 0, SendThread, this, 0, nullptr);

	// WorkerThread 
	for (int i = 0; i < m_WorkerCount; i++)
	{
		m_pWorkerThreadArray[i] = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, this, 0, nullptr);
		CloseHandle(m_pWorkerThreadArray[i]);
	}

	m_AcceptThread = (HANDLE)_beginthreadex(nullptr, 0, AcceptThread, this, 0, nullptr);

	SYSLOG(L"DEBUG", LOG_DEBUG, L"Server Start[WorkerThread : %d, MaxSession : %d]", m_WorkerCount, m_MaxSession);


	return true;
}
void CLanServer::Stop()
{
	////////// 미완성
	m_bServerOpen = false;

	shutdown(m_ListenSocket, SD_BOTH);
	closesocket(m_ListenSocket);

	// 접속한 모든 클라이언트에 대해 강제종료
	while (m_NowSession > 0)
	{
		for (int i = 0; i < m_MaxSession; i++)
		{
			shutdown(m_pSessionArray[i].socket, SD_BOTH);
			Disconnect(&m_pSessionArray[i]);
		}
	}
	// Accept Thread 종료체크
	WaitForSingleObject(m_AcceptThread, INFINITE);
	WaitForSingleObject(m_SendThread, INFINITE);

	// WorkerThread 종료 체크
	PostQueuedCompletionStatus(m_IOCP, 0, 0, 0);
	WaitForMultipleObjects(m_WorkerCount, m_pWorkerThreadArray, true, INFINITE);

	delete[] m_pSessionArray;
	delete[] m_pWorkerThreadArray;

	SYSLOG(L"DEBUG", LOG_DEBUG, L"Server Stop");

}
bool CLanServer::Disconnect(ULONG64& SessionID)
{
	CSession *pSession = FindSession(SessionID);

	if (pSession == nullptr)
		return false;

	return Disconnect(pSession);
}
bool CLanServer::Disconnect(CSession *pSession)
{
	if (InterlockedCompareExchange64(&pSession->ReleaseFlag.Flag, 0, dfRELEASEFLAG) == dfRELEASEFLAG)
	{
		shutdown(pSession->socket, SD_BOTH);

		ULONG64 SessionID = pSession->SessionID;
		pSession->SessionID = 0;
		OnClientLeave(SessionID);

		InterlockedDecrement(&m_NowSession);
		int ArraySessionID = GET_SESSIONARRAY(SessionID);

		closesocket(pSession->socket);
		BufferClear(pSession);

		SessionIndex.Push(ArraySessionID);
		return true;
	}

	return false;
}
int CLanServer::SendPacket(ULONG64 SessionID, CPacketBuffer *pInBuffer)
{
	if (!SessionAcquireLock(SessionID))
		return 0;

	CSession *pSession = FindSession(SessionID);

	if (pSession == nullptr)
		return 0;

	if (pSession->SessionID == SessionID)
	{
		st_PACKET_HEADER Header;

		pInBuffer->ShortSizeHeaderAlloc();

		if (pInBuffer->GetLastError() != 0)
		{
			SessionAcquireFree(pSession);
			return false;
		}

		if (pInBuffer->GetUseSize() == 0)
		{
			SessionAcquireFree(pSession);
			return false;
		}

		pInBuffer->AddRefCount();
		pSession->SendQ.Enqueue(&pInBuffer);

		SendThreadQueue.Enqueue(&pSession->SessionID);
		SetEvent(m_SendEvent);

		pInBuffer->Free();

	}
	else
		return false;

	SessionAcquireFree(pSession);

	return true;
}

int CLanServer::SendPacketAndShutdown(ULONG64 SessionID, CPacketBuffer *pInBuffer)
{
	int ret = SendPacket(SessionID, pInBuffer);

	if (ret != true)
		return ret;

	CSession *pSession = FindSession(SessionID);

	if (pSession == nullptr)
		return false;

	pSession->m_bSendShutdown = true;

	return true;
}
UINT WINAPI CLanServer::AcceptThread(LPVOID arg)
{
	CLanServer *pServer = reinterpret_cast<CLanServer *>(arg);

	return pServer->AcceptWork();
}
UINT CLanServer::AcceptWork()
{
	SOCKET socket;
	SOCKADDR_IN clientAddr;
	int addrlen = sizeof(clientAddr);
	WCHAR szIP[50];
	int Port;

	while (m_bServerOpen)
	{
		if (m_MaxSession <= m_NowSession)
			continue;

		socket = INVALID_SOCKET;
		socket = accept(m_ListenSocket, reinterpret_cast<SOCKADDR *>(&clientAddr), &addrlen);

		if (socket == INVALID_SOCKET)
		{
			m_iErrorCode = WSAGetLastError();
			OnError(m_iErrorCode, L"Accept Socket Error");
			break;
		}

		InetNtop(AF_INET, &clientAddr, szIP, 50);
		Port = ntohs(clientAddr.sin_port);

		if (!OnConnectionRequest(szIP, Port))
		{
			closesocket(socket);
			continue;
		}

		if (!m_bNagleOption)
		{
			int opt = 1;
			setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(int));
		}


		CSession *pSession = nullptr;

		InterlockedIncrement64((LONG64 *)&m_UserSession);//m_UserSession++;  // 고윳값 증가

		if (!AddSession(socket, &pSession))
		{
			OnError(NO_SESSION_SPACE, L"Session is Full");
			closesocket(socket);
			continue;
		}

		CreateIoCompletionPort((HANDLE)socket, m_IOCP, (ULONG64)pSession, 0);

		/////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// 소켓으로부터 IP, PORT 가져온다.
		SOCKADDR_IN AddrSock;
		WCHAR IP[200];
		int len = sizeof(AddrSock);
		int Port;
		getpeername(socket, (SOCKADDR *)&AddrSock, &len);
		wsprintf(IP, L"%u.%u.%u.%u", AddrSock.sin_addr.S_un.S_un_b.s_b1,
			AddrSock.sin_addr.S_un.S_un_b.s_b2, AddrSock.sin_addr.S_un.S_un_b.s_b3, AddrSock.sin_addr.S_un.S_un_b.s_b4);
		Port = AddrSock.sin_port;
		/////////////////////////////////////////////////////////////////////////////////////////////////////////////

		InterlockedIncrement(&m_NowSession);

		BufferClear(pSession);
		/////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// IOCP 등록 
		/////////////////////////////////////////////////////////////////////////////////////////////////////////////
		InterlockedAdd(reinterpret_cast<LONG *>(&m_AcceptTotal), 1);
		InterlockedAdd(&m_AcceptTPS, 1);
		InterlockedIncrement(&pSession->ReleaseFlag.IOCount);

		OnClientJoin(pSession->SessionID, IP, Port);
		RecvPost(pSession);

		if (InterlockedDecrement(&pSession->ReleaseFlag.IOCount) == 0)
			Disconnect(pSession);
	}

	return 1;
}
UINT WINAPI CLanServer::WorkerThread(LPVOID arg)
{
	CLanServer *pServer = reinterpret_cast<CLanServer *>(arg);

	return pServer->WorkerWork();
}
UINT CLanServer::WorkerWork()
{
	DWORD cbTransffered;
	LPOVERLAPPED pOverlapped;
	CSession *pSession;

	while (1)
	{
		cbTransffered = 0;
		pOverlapped = nullptr;
		pSession = nullptr;

		int retval = GetQueuedCompletionStatus(m_IOCP, &cbTransffered, (PULONG_PTR)&pSession, &pOverlapped, INFINITE);


		// 워커스레드 정상종료 체크
		if (pOverlapped == nullptr && pSession == nullptr && cbTransffered == 0)
		{
			PostQueuedCompletionStatus(m_IOCP, 0, 0, 0);
			break;
		}

		if (pSession == nullptr)
			continue;

		// 소켓 종료
		if (cbTransffered == 0)
		{
			shutdown(pSession->socket, SD_BOTH);
		}
		else
			// 완료통지
		{
			if (pSession->ReleaseFlag.UseFlag == 0)
				continue;

			if (pOverlapped == &pSession->RecvOverlapped)
				RecvProc(pSession, cbTransffered);
			
			else if (pOverlapped == &pSession->SendOverlapped)
				SendProc(pSession, cbTransffered);
		}

		if (InterlockedDecrement(&pSession->ReleaseFlag.IOCount) == 0)
			Disconnect(pSession);
	}

	return 1;
}

UINT WINAPI CLanServer::SendThread(LPVOID arg)
{
	CLanServer *pServer = reinterpret_cast<CLanServer *>(arg);

	return pServer->SendWork();
}
UINT CLanServer::SendWork()
{
	CSession *pSession = nullptr;
	ULONG64 SessionID;
	while (m_bServerOpen)
	{
		WaitForSingleObject(m_SendEvent, INFINITE);

		while (SendThreadQueue.Dequeue(&SessionID))
		{
			pSession = FindSession(SessionID);

			if (pSession == nullptr)
				continue;

			if (!SessionAcquireLock(SessionID))
				continue;

			SendPost(pSession);

			SessionAcquireFree(pSession);
		}
	}

	return 1;
}
bool CLanServer::RecvProc(CSession *pSession, DWORD cbTransffered)
{
	pSession->RecvQ.MoveWritePos(cbTransffered);

	while (1)
	{
		// 받은 내용을 검사해야된다. 최소한 패킷헤더 이상을 가지고있어야 처리된다.
		if (pSession->RecvQ.GetUseSize() <= sizeof(short))
			break; // 더이상 처리할 패킷없음.

		short Len;
		pSession->RecvQ.Peek(reinterpret_cast<char *>(&Len), sizeof(short));

		if (pSession->RecvQ.GetUseSize() < Len)
			break; // 더이상 처리할 패킷없음.

		pSession->RecvQ.RemoveData(2);

		if (Len > en_MAX_RECVBYTES)
		{
			Disconnect(pSession);
			return false;
		}

		m_RecvPacketTPS++;

		CPacketBuffer *pBuffer = CPacketBuffer::Alloc();
		pSession->RecvQ.GetData(pBuffer->GetWriteBufferPtr(), Len);
		pBuffer->MoveWritePos(Len);

		// 여기까지 오면 사실상 정상패킷처리해준다.
		pBuffer->AddRefCount();
		OnRecv(pSession->SessionID, pBuffer);
		pBuffer->Free();
	}

	if (!SessionAcquireLock(pSession->SessionID))
		return false;

	RecvPost(pSession);

	SessionAcquireFree(pSession);

	return true;
}
bool CLanServer::SendProc(CSession *pSession, DWORD cbTransffered)
{
	CPacketBuffer *pTemp;

	for (int i = 0; i < pSession->m_SendCount; i++)
	{
		if (!pSession->PeekQ.Dequeue(&pTemp))
		{
			SYSLOG(L"PACKET", LOG_SYSTEM, L"PeekQ Dequeue Error : CSession : %p, SessionID, %lld", pSession, pSession->SessionID);
		}

		OnSend(pSession->SessionID, pTemp->GetUseSize());
		pTemp->Free();

		m_SendPacketTPS++;
	}

	InterlockedExchange(&pSession->SendFlag, false);

	if (pSession->m_bSendShutdown && pSession->SendQ.GetUseSize() == 0)
	{
		Disconnect(pSession);
		return true;
	}

	if (!SessionAcquireLock(pSession->SessionID))
		return false;

	if (pSession->SendQ.GetUseSize() > 0)
	{
		SendThreadQueue.Enqueue(&pSession->SessionID);
		SetEvent(m_SendEvent);
	}

	SessionAcquireFree(pSession);

	return true;
}

bool CLanServer::AddSession(SOCKET socket, CSession **pOutSession)
{
	WORD index = -1;

	if (!SessionIndex.Pop(&index))
		return false;

	*pOutSession = &m_pSessionArray[index];

	CSession *pSession = *pOutSession;

	INSERT_SESSION((*pOutSession)->SessionID, index, m_UserSession);

	pSession->SendFlag = false;
	pSession->m_SendCount = 0;
	pSession->ReleaseFlag.Flag = 0;
	pSession->m_bSendShutdown = false;
	pSession->_SendErrorCode = 0;
	pSession->_RecvErrorCode = 0;

	InterlockedIncrement(&(*pOutSession)->ReleaseFlag.UseFlag);
	(*pOutSession)->socket = socket;

	BufferClear(pSession);

	return true;
}

void CLanServer::BufferClear(CSession *pSession)
{
	CPacketBuffer *pBuffer;

	while (pSession->PeekQ.Dequeue(&pBuffer))
		pBuffer->Free();

	while (pSession->SendQ.Dequeue(&pBuffer))
		pBuffer->Free();


	pSession->RecvQ.ClearBuffer();
}

CLanServer::CSession* CLanServer::FindSession(ULONG64 SessionID)
{
	WORD index = 0;

	index = GET_SESSIONARRAY(SessionID);

	if (index >= en_MAX_USER)
		return nullptr;

	if (m_pSessionArray[index].ReleaseFlag.UseFlag == 1)
		return &m_pSessionArray[index];

	return nullptr;
}
bool CLanServer::ErrorCheck(CSession *pSession, int errorcode, int TransType)
{
	m_iErrorCode = errorcode;

	if (TransType == en_TYPESEND)
		pSession->_SendErrorCode = errorcode;
	else
		pSession->_RecvErrorCode = errorcode;

	bool ret = false;

	if (errorcode != WSA_IO_PENDING && errorcode != 997)
	{

		if (InterlockedDecrement(&pSession->ReleaseFlag.IOCount) == 0)
		{
			Disconnect(pSession);
			return true;
		}

		WCHAR szMessage[250];

		switch (errorcode)
		{
		case WSAENOBUFS:
			StringCchPrintf(szMessage, 250, L"SessionID : %llu, Socket : %llu, %s : WSAENOBUF Error", pSession->SessionID, pSession->socket, TransType == en_TYPESEND ? L"Send" : L"Recv");
			OnError(errorcode, szMessage);
			shutdown(pSession->socket, SD_BOTH);
			break;
		case WSAEFAULT:
			StringCchPrintf(szMessage, 250, L"SessionID : %llu, Socket : %llu, %s : WSAEFAULT(Buffer) Error", pSession->SessionID, pSession->socket, TransType == en_TYPESEND ? L"Send" : L"Recv");
			OnError(errorcode, szMessage);
			shutdown(pSession->socket, SD_BOTH);
			break;
		}
	}

	return ret;
}

bool CLanServer::SessionAcquireLock(ULONG64 SessionID)
{
	int index = GET_SESSIONARRAY(SessionID);

	CSession *pSession = &m_pSessionArray[index];

	InterlockedIncrement(&pSession->ReleaseFlag.IOCount);

	if (pSession->ReleaseFlag.IOCount == 1)
	{
		if (InterlockedDecrement(&pSession->ReleaseFlag.IOCount) == 0)
			Disconnect(pSession);

		return false;
	}

	if (pSession->ReleaseFlag.UseFlag == 0)
	{
		if (InterlockedDecrement(&pSession->ReleaseFlag.IOCount) == 0)
			Disconnect(pSession);

		return false;
	}

	if (pSession->SessionID != SessionID || pSession->SessionID == 0)
	{
		if (InterlockedDecrement(&pSession->ReleaseFlag.IOCount) == 0)
			Disconnect(pSession);

		return false;
	}


	return true;
}

void  CLanServer::SessionAcquireFree(CSession *pSession)
{
	if (InterlockedDecrement(&pSession->ReleaseFlag.IOCount) == 0)
		Disconnect(pSession);
}

int CLanServer::ServerGetLastError()
{
	return m_iErrorCode;
}