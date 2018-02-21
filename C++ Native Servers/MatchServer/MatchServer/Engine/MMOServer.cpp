#pragma comment(lib, "ws2_32")
#pragma comment(lib, "Winmm.lib")

#include <winsock2.h>
#include <WS2tcpip.h>
#include "MMOServer.h"

BYTE		g_PacketCode;
BYTE		g_PacketKey1;
BYTE		g_PacketKey2;

CNetSession::CNetSession()
{
	_Mode = MODE_NONE;
	_iArrayIndex = -1;
	_IOCount = 0;
	_ISendIO = 0;
	_bLogoutFlag = false;
	_bAuthToGameFlag = false;
}

CNetSession::~CNetSession()
{

}

void CNetSession::SetSessionArrayIndex(int iIndex)
{
	if (_iArrayIndex == -1)
		*(const_cast<int *>(&_iArrayIndex)) = iIndex;
}

// 세션 모드 변경
void CNetSession::SetMode_GAME(void)
{
	_bAuthToGameFlag = true;
}

/////////////////////////////////////////////////////////////////////
// 연결 끊기 
// TRUE : shutdown
// FALSE : closesocket
/////////////////////////////////////////////////////////////////////
void CNetSession::Disconnect(bool bForce)
{
	if (bForce)
		shutdown(_ClientInfo.socket, SD_BOTH);
	else
		closesocket(_ClientInfo.socket);
}

/////////////////////////////////////////////////////////////////////
// 패킷 전송
// 헤더, 암호화 까지 해서 보낸다.
/////////////////////////////////////////////////////////////////////
bool CNetSession::SendPacket(CPacketBuffer *pPacket)
{
	if (_Mode == MODE_NONE || _Mode == MODE_LOGOUT_IN_AUTH || _Mode == MODE_LOGOUT_IN_GAME
		|| _Mode == MODE_WAIT_LOGOUT)
	{
		pPacket->Free();
		return false;
	}

	st_PACKET_HEADER Header;
	pPacket->CommonSizeHeaderAlloc(&Header, g_PacketCode, g_PacketKey1, g_PacketKey2);
	pPacket->Encode(&Header);

	if (pPacket->GetLastError() != 0)
	{
		pPacket->Free();
		return false;
	}

	if (pPacket->GetUseSize() == 0)
	{
		pPacket->Free();
		return false;
	}

	pPacket->AddRefCount();
	_SendQ.Enqueue(&pPacket);
	pPacket->Free();

	return true;
}


void CNetSession::SendCompleteDisconnect(void)
{
	// 모든 Send 완료시 접속종료 설정.
	_bLogoutFlag = true;
}

bool CNetSession::RecvPost(void)
{
	ZeroMemory(&_RecvOverlapped, sizeof(OVERLAPPED));

	WSABUF	wsabuf[2];
	int bufcount = 1;

	wsabuf[0].buf = _RecvQ.GetWriteBufferPtr();
	wsabuf[0].len = _RecvQ.GetNotBrokenPutSize();

	if (wsabuf[0].len < _RecvQ.GetFreeSize())
	{
		bufcount++;
		wsabuf[1].buf = _RecvQ.GetBufferPtr();
		wsabuf[1].len = _RecvQ.GetFreeSize() - wsabuf[0].len;
	}

	InterlockedIncrement64(&_IOCount);

	DWORD	RecvSize = 0;
	DWORD	Flag = 0;

	int ret = WSARecv(_ClientInfo.socket, wsabuf, bufcount, &RecvSize, &Flag, &_RecvOverlapped, nullptr);

	// 에러 처리
	if (ret == SOCKET_ERROR)
		ErrorCheck(WSAGetLastError(), eTypeRECV);
	

	return true;
}
bool CNetSession::SendPost(void)
{
	ZeroMemory(&_SendOverlapped, sizeof(OVERLAPPED));

	WSABUF wsabuf[en_MAX_SENDBUF];
	int bufcount = 0;
	CPacketBuffer *pBuffer;

	while (_SendQ.Dequeue(&pBuffer))
	{
		_PeekQ.Enqueue(&pBuffer);

		wsabuf[bufcount].buf = pBuffer->GetHeaderPtr();
		wsabuf[bufcount].len = pBuffer->GetHeaderDataUseSize();
		bufcount++;

		if (bufcount >= en_MAX_SENDBUF)
			break;
	}

	_iSendPacketCnt = bufcount;

	InterlockedIncrement64(&_IOCount);


	DWORD Flag = 0;

	int ret = WSASend(_ClientInfo.socket, wsabuf, bufcount, &_SendPacketSize, Flag, &_SendOverlapped, nullptr);

	if (ret == SOCKET_ERROR)
		ErrorCheck(WSAGetLastError(), eTypeSEND);

	return true;
}

void	CNetSession::CompleteRecv(DWORD dwTransferred)
{
	_RecvQ.MoveWritePos(dwTransferred);

	_LastRecvPacketTime = CUpdateTime::GetTickCount();

	while (1)
	{
		if (_RecvQ.GetUseSize() <= sizeof(st_PACKET_HEADER))
			break;

		st_PACKET_HEADER Header;
		_RecvQ.Peek(reinterpret_cast<char *>(&Header), sizeof(Header));

		// 패킷코드 검사 
		if (Header.byCode != g_PacketCode)
		{
			// 에러 및 로그처리
			_bLogoutFlag = true;
			return;
		}

		// 이하는 명백한 공격패킷
		if (Header.shLen + 5 > _RecvQ.GetUseSize())
			break;

		// 패킷코드에서 통과했다면, Len만큼 PacketBuffer를 만든다.
		CPacketBuffer *pBuffer = CPacketBuffer::Alloc();
		_RecvQ.GetData(pBuffer->GetBufferPtr(), Header.shLen + 5);
		pBuffer->MoveWritePos(Header.shLen);

		// 디코딩한다.
		if (!pBuffer->Decode())
		{
			// 에러 및 로그처리
			pBuffer->Free();
			_bLogoutFlag = true;
			break;
		}

		// 여기까지 오면 사실상 정상패킷처리해준다.
		pBuffer->AddRefCount();
		_CompleteRecvQ.Enqueue(&pBuffer);
		pBuffer->Free();
	}

	RecvPost();
}
void CNetSession::CompleteSend(DWORD dwTransferred)
{
	CPacketBuffer *pTemp;
	
	for (int i = 0; i < _iSendPacketCnt; i++)
	{
		_PeekQ.Dequeue(&pTemp);
		pTemp->Free();
	}

	InterlockedExchange(&_ISendIO, false);
}

void CNetSession::ErrorCheck(int errorCode, TransType Type)
{
	if (errorCode != WSA_IO_PENDING)
	{
		if (Type == eTypeSEND)
			InterlockedExchange(&_ISendIO, 0);

		if (InterlockedDecrement64(&_IOCount) == 0)
		{
			_bLogoutFlag = true;
			return;
		}

		switch (errorCode)
		{
		case WSAENOBUFS:
			shutdown(_ClientInfo.socket, SD_BOTH);
			OnError(NET_IO_NOBUFS, L"Client No Buf Error");
			break;
		case WSAEFAULT:
			shutdown(_ClientInfo.socket, SD_BOTH);
			OnError(NET_IO_FAULT, L"Client No Buf Error");
			break;
		}
	}
}

CMMOServer::CMMOServer(int iMaxSession)
{
	_bServerOpen = false;

	_iMaxSession = min(iMaxSession, en_SESSION_MAX);


	// 연결한 세션 배열 포인터
	// CNetSession *p[en_SESSION_MAX] 형태
	// 외부에서 선언된 세션을 연결시키기 위해 사용
	// CNetSession에게 상속받은 클래스가 연결시켜줄 예정
	_pSessionArray = new CNetSession*[_iMaxSession];
	for (int i = 0; i < _iMaxSession; i++)
		_pSessionArray[i] = nullptr;
}


CMMOServer::~CMMOServer()
{
}


bool CMMOServer::Start(WCHAR *szListenIP, int iPort, int iWorkerThread, bool bNagle, BYTE PacketCode, BYTE PacketKey1, BYTE PacketKey2)
{
	if (_bServerOpen)
		return false;

	lstrcpyW(_szListenIP, szListenIP);
	_iListenPort = iPort;

	g_PacketCode = PacketCode;
	g_PacketKey1 = PacketKey1;
	g_PacketKey2 = PacketKey2;

	_iNowSession = 0;
	_iWorkerThread = min(iWorkerThread, eWORKER_THREAD_MAX);
	_bEnableNagle = bNagle;

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		// 에러 처리
		return false;
	}

	_ListenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (_ListenSocket == INVALID_SOCKET)
	{
		// 에러 처리
		return false;
	}

	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(SOCKADDR_IN));
	serveraddr.sin_family = AF_INET;
	InetPton(AF_INET, _szListenIP, &serveraddr.sin_addr);
	serveraddr.sin_port = htons(_iListenPort);

	if (bind(_ListenSocket, (SOCKADDR *)&serveraddr, sizeof(serveraddr)) == SOCKET_ERROR)
	{
		// 에러 처리
		return false;
	}

	if (listen(_ListenSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		return false;
	}

	_bServerOpen = true;
	// IOCP 설정
	_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);

	// AcceptThread 설정
	_hAcceptThread = (HANDLE)_beginthreadex(nullptr, 0, AcceptThread, this, 0, nullptr);

	// Auth Thread 설정
	_hAuthThread = (HANDLE)_beginthreadex(nullptr, 0, AuthThread, this, 0, nullptr);

	// Game Update Thread 설정
	_hGameUpdateThread = (HANDLE)_beginthreadex(nullptr, 0, GameUpdateThread, this, 0, nullptr);

	for (int i = 0; i < eWORKER_THREAD_MAX; i++)
	{
		_hIOCPWorkerThread[i] = (HANDLE)_beginthreadex(nullptr, 0, IOCPWorkerThread, this, 0, nullptr);
		CloseHandle(_hIOCPWorkerThread[i]);
	}
	// Send Thread 설정
	_hSendThread = (HANDLE)_beginthreadex(nullptr, 0, SendThread, this, 0, nullptr);


	// 모니터링 변수 초기화
	_Monitor_AcceptSocket = 0;
	_Monitor_SessionAllMode = 0;
	_Monitor_SessionAuthMode = 0;
	_Monitor_SessionGameMode = 0;
}

bool CMMOServer::Stop(void)
{
	_bServerOpen = false;

	closesocket(_ListenSocket);

		// Accpet Thread 대기
	WaitForSingleObject(_hAcceptThread, INFINITE);
	WaitForSingleObject(_hAuthThread, INFINITE);
	WaitForSingleObject(_hGameUpdateThread, INFINITE);

	PostQueuedCompletionStatus(_hIOCP, 0, 0, 0);
	WaitForMultipleObjects(_iWorkerThread, _hIOCPWorkerThread, true, INFINITE);

	_BlankSessionIndexStack.ClearStack();
}

void CMMOServer::SetSessionArray(int iArrayIndex, CNetSession* pSession)
{
	if (iArrayIndex >= _iMaxSession)
		return;

	if (_pSessionArray[iArrayIndex] != nullptr)
		return;

	_pSessionArray[iArrayIndex] = pSession;
	pSession->SetSessionArrayIndex(iArrayIndex);
	_BlankSessionIndexStack.Push(iArrayIndex);
}

void CMMOServer::SendPacket_GameAll(CPacketBuffer *pPacket, ULONG64 ClientID)
{
	for (int iArrayIndex = 0; iArrayIndex < en_SESSION_MAX; iArrayIndex++)
	{
		if (_pSessionArray[iArrayIndex] == nullptr)
			continue;

		if (_pSessionArray[iArrayIndex]->_Mode == MODE_GAME)
		{
			pPacket->AddRefCount();
			_pSessionArray[iArrayIndex]->SendPacket(pPacket);
		}
	}

	pPacket->Free();
}
void CMMOServer::SendPacket(CPacketBuffer *pPacket, ULONG64 ClientID)
{
	int iIndex = GET_SESSIONARRAY(ClientID);

	CNetSession *pSession = _pSessionArray[iIndex];

	pPacket->AddRefCount();
	pSession->SendPacket(pPacket);
	pPacket->Free();
}
/////////////////////////////////////////////////////////////////////////
// Accept 부
/////////////////////////////////////////////////////////////////////////
UINT WINAPI	CMMOServer::AcceptThread(LPVOID arg)
{
	CMMOServer *pServer = reinterpret_cast<CMMOServer *>(arg);

	return pServer->AcceptThread_Work();
}

bool CMMOServer::AcceptThread_Work()
{
	SOCKET socket;
	SOCKADDR_IN clientAddr;
	int addrlen = sizeof(clientAddr);


	while (_bServerOpen)
	{
		socket = accept(_ListenSocket, reinterpret_cast<SOCKADDR *>(&clientAddr), &addrlen);

		if (socket == INVALID_SOCKET)
		{
			// 에러 처리
			OnError(ACCEPT_INVALID_SOCKET, L"Accept Function Returned Invalid Socket");
			continue;
		}

		if (!_bEnableNagle)
		{
			int opt = 1;
			setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(int));
		}

		st_CLIENT_CONNECT *pConnector = _ClientConnectPool.Alloc();
		pConnector->socket = socket;
		InetNtop(AF_INET, &clientAddr, pConnector->_IP, 16);
		pConnector->_Port = ntohs(clientAddr.sin_port);

		_AcceptSocketQueue.Enqueue(&pConnector);
		InterlockedAdd(&_Monitor_AcceptSocket, 1);
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////
// Auth 부
/////////////////////////////////////////////////////////////////////////
UINT WINAPI	CMMOServer::AuthThread(LPVOID arg)
{
	CMMOServer *pServer = reinterpret_cast<CMMOServer *>(arg);

	return pServer->AuthThread_Work();
}

bool	CMMOServer::AuthThread_Work()
{
	while (_bServerOpen)
	{
		ProcAuth_Accept();

		ProcAuth_Packet();

		OnAuth_Update();
		InterlockedAdd(&_Monitor_Counter_AuthUpdate, 1);

		ProcAuth_Logout();

		ProcAuth_TimeoutCheck();


		Sleep(20);
	}

	return true;
}


void CMMOServer::ProcAuth_Accept(void)
{
	// 1. AcceptSocketQueue에 들어온 정보를 뽑는다.
	st_CLIENT_CONNECT *pConnector;
	CNetSession *pSession;
	CPacketBuffer *pBuffer;

	while (_AcceptSocketQueue.Dequeue(&pConnector))
	{
		int index;
		pSession = nullptr;

		if (!_BlankSessionIndexStack.Pop(&index))
		{
			OnError(ACCEPT_SOCKET_ARRAY_ISFULL, L"Accept Function Returned Invalid Socket");
			_ClientConnectPool.Free(pConnector);

			InterlockedAdd(&_Monitor_AcceptSocket, -1);
			closesocket(pConnector->socket);
			continue;
		}

		// 해당 공간에 신규 세션을 할당
		pSession = _pSessionArray[index];

		memcpy(&pSession->_ClientInfo, pConnector, sizeof(st_CLIENT_CONNECT));

		_ClientConnectPool.Free(pConnector);

		InterlockedAdd(&_Monitor_Counter_Accept, 1);
		InterlockedAdd(&_Monitor_SessionAllMode, 1);
		InterlockedAdd(&_Monitor_SessionAuthMode, 1);

		CreateIoCompletionPort((HANDLE)pSession->_ClientInfo.socket, _hIOCP, (ULONG64)pSession, 0);

		// IOCP 연결
		pSession->_Mode = MODE_AUTH;
		pSession->_bAuthToGameFlag = false;
		pSession->_bLogoutFlag = false;
		InterlockedIncrement64(&pSession->_IOCount);
		pSession->OnAuth_SessionJoin();
		pSession->RecvPost();
		if (InterlockedDecrement64(&pSession->_IOCount) == 0)
			pSession->_bLogoutFlag = true;

		pSession->_LastRecvPacketTime = CUpdateTime::GetTickCount();
	}
}

void CMMOServer::ProcAuth_Packet(void)
{
	CNetSession *pSession;
	CPacketBuffer *pBuffer;

	int iPacketProc = 1;
	int Proc = 0;
	for (int iIndex = 0; iIndex < _iMaxSession; iIndex++)
	{
		pSession = _pSessionArray[iIndex];

		if (pSession == nullptr)
			continue;

		if (pSession->_Mode != MODE_AUTH)
			continue;

		Proc = 0;
		while (pSession->_CompleteRecvQ.Dequeue(&pBuffer))
		{
			pBuffer->AddRefCount();
			pSession->OnAuth_Packet(pBuffer);
			pBuffer->Free();

			pSession->_LastRecvPacketTime = CUpdateTime::GetTickCount();
			InterlockedAdd(&_Monitor_Counter_PacketProc, 1);

			Proc++;

			if (Proc == iPacketProc)
				break;
		}

	}
}

void CMMOServer::ProcAuth_Logout(void)
{
	CNetSession *pSession;
	CPacketBuffer *pBuffer;

	for (int iIndex = 0; iIndex < _iMaxSession; iIndex++)
	{
		pSession = _pSessionArray[iIndex];

		if (pSession == nullptr)
			continue;

		if (pSession->_bLogoutFlag && pSession->_Mode == MODE_AUTH)
			pSession->_Mode = MODE_LOGOUT_IN_AUTH;

		if (pSession->_Mode == MODE_LOGOUT_IN_AUTH && pSession->_ISendIO == 0)
		{
			pSession->_Mode = MODE_WAIT_LOGOUT;
			pSession->OnAuth_SessionLeave();
			InterlockedAdd(&_Monitor_SessionAuthMode, -1);
		}

		if (pSession->_bAuthToGameFlag && pSession->_Mode == MODE_AUTH)
		{
			pSession->_Mode = MODE_AUTH_TO_GAME;
			pSession->OnAuth_SessionLeave(1);
			InterlockedAdd(&_Monitor_SessionAuthMode, -1);
		}

	}
}

void CMMOServer::ProcAuth_TimeoutCheck(void)
{
	CNetSession *pSession;

	for (int iIndex = 0; iIndex < _iMaxSession; iIndex++)
	{
		pSession = _pSessionArray[iIndex];

		if (pSession == nullptr)
			continue;

		if (pSession->_Mode != MODE_AUTH)
			continue;

		if (CUpdateTime::GetTickCount() - pSession->_LastRecvPacketTime >= eTIMEOUT)
		{
			pSession->_Mode = MODE_LOGOUT_IN_AUTH;
			pSession->OnAuth_Timeout();
		}
	}
}
/////////////////////////////////////////////////////////////////////////
// GameUpdate 부
/////////////////////////////////////////////////////////////////////////
UINT WINAPI CMMOServer::GameUpdateThread(LPVOID arg)
{
	CMMOServer *pServer = reinterpret_cast<CMMOServer *>(arg);

	return pServer->GameUpdateThread_Work();
}

bool	CMMOServer::GameUpdateThread_Work()
{
	while (_bServerOpen)
	{
		ProcGame_AuthToGame();

		ProcGame_Packet();

		OnGame_Update();
		InterlockedAdd(&_Monitor_Counter_GameUpdate, 1);

		ProcGame_Logout();

		ProcGame_Release();

		ProcGame_TimeoutCheck();

		Sleep(5);
	}

	return true;
}

void CMMOServer::ProcGame_AuthToGame(void)
{
	CNetSession *pSession;

	for (int iIndex = 0; iIndex < _iMaxSession; iIndex++)
	{
		pSession = _pSessionArray[iIndex];

		if (pSession == nullptr)
			continue;

		if (pSession->_Mode == MODE_AUTH_TO_GAME)
		{
			pSession->_Mode = MODE_GAME;

			pSession->OnGame_SessionJoin();
			InterlockedAdd(&_Monitor_SessionGameMode, 1);
		}
	}
}


void CMMOServer::ProcGame_Packet(void)
{
	CNetSession *pSession;
	CPacketBuffer *pBuffer;

	int iPacketProc = 1;
	int Proc = 0;
	for (int iIndex = 0; iIndex < _iMaxSession; iIndex++)
	{
		pSession = _pSessionArray[iIndex];

		if (pSession == nullptr)
			continue;

		if (pSession->_Mode != MODE_GAME)
			continue;

		Proc = 0;
		while (pSession->_CompleteRecvQ.Dequeue(&pBuffer))
		{
			pBuffer->AddRefCount();
			pSession->OnGame_Packet(pBuffer);
			pBuffer->Free();

			InterlockedAdd(&_Monitor_Counter_PacketProc, 1);

			Proc++;

			pSession->_LastRecvPacketTime = CUpdateTime::GetTickCount();

			if (Proc == iPacketProc)
				break;
		}

	}
}

void CMMOServer::ProcGame_Logout(void)
{
	CNetSession *pSession;
	CPacketBuffer *pBuffer;

	for (int iIndex = 0; iIndex < _iMaxSession; iIndex++)
	{
		pSession = _pSessionArray[iIndex];

		if (pSession == nullptr)
			continue;

		if (pSession->_bLogoutFlag && pSession->_Mode == MODE_GAME)
			pSession->_Mode = MODE_LOGOUT_IN_GAME;
		
		if (pSession->_Mode == MODE_LOGOUT_IN_GAME && pSession->_ISendIO == 0)
		{
			pSession->_Mode = MODE_WAIT_LOGOUT;
			pSession->OnGame_SessionLeave();
			InterlockedAdd(&_Monitor_SessionGameMode, -1);
		}

	}
}

void CMMOServer::ProcGame_Release(void)
{
	CNetSession *pSession;
	CPacketBuffer *pBuffer;

	for (int iIndex = 0; iIndex < _iMaxSession; iIndex++)
	{
		pSession = _pSessionArray[iIndex];

		if (pSession == nullptr)
			continue;

		if (pSession->_Mode == MODE_WAIT_LOGOUT)
		{
			pSession->_Mode = MODE_NONE;
			pSession->Disconnect();

			InterlockedAdd(&_Monitor_SessionAllMode, -1);
			InterlockedAdd(&_Monitor_AcceptSocket, -1);

			pSession->_RecvQ.ClearBuffer();
			while (pSession->_CompleteRecvQ.Dequeue(&pBuffer))
				pBuffer->Free();
			while (pSession->_SendQ.Dequeue(&pBuffer))
				pBuffer->Free();
			while (pSession->_PeekQ.Dequeue(&pBuffer))
				pBuffer->Free();

			pSession->_bAuthToGameFlag = false;
			pSession->_bLogoutFlag = false;

			_BlankSessionIndexStack.Push(pSession->_iArrayIndex);
		}
	}
}

void CMMOServer::ProcGame_TimeoutCheck(void)
{
	CNetSession *pSession;

	for (int iIndex = 0; iIndex < _iMaxSession; iIndex++)
	{
		pSession = _pSessionArray[iIndex];

		if (pSession == nullptr)
			continue;

		if (pSession->_Mode != MODE_GAME)
			continue;

		if (CUpdateTime::GetTickCount() - pSession->_LastRecvPacketTime >= eTIMEOUT)
		{
			pSession->_Mode = MODE_LOGOUT_IN_GAME;
			pSession->OnAuth_Timeout();
		}
	}
}
/////////////////////////////////////////////////////////////////////////
// IOCP WORKER 부
/////////////////////////////////////////////////////////////////////////
UINT WINAPI CMMOServer::IOCPWorkerThread(LPVOID arg)
{
	CMMOServer *pServer = reinterpret_cast<CMMOServer *>(arg);

	return pServer->IOCPWorker_Work();
}

bool CMMOServer::IOCPWorker_Work()
{
	DWORD cbTransffered;
	LPOVERLAPPED pOverlapped;
	CNetSession *pSession;

	while (1)
	{
		cbTransffered = 0;
		pOverlapped = nullptr;
		pSession = nullptr;

		int retval = GetQueuedCompletionStatus(_hIOCP, &cbTransffered, (PULONG_PTR)&pSession, &pOverlapped, INFINITE);

		// 워커스레드 정상종료 체크
		if (pOverlapped == nullptr && pSession == nullptr && cbTransffered == 0)
		{
			PostQueuedCompletionStatus(_hIOCP, 0, 0, 0);
			break;
		}

		if (pSession == nullptr)
			continue;

		if (pSession->_Mode == MODE_NONE)
			continue;

		if (cbTransffered == 0)
			pSession->Disconnect(true);
		else
		{
			if (pOverlapped == &pSession->_RecvOverlapped)
				pSession->CompleteRecv(cbTransffered);

			if (pOverlapped == &pSession->_SendOverlapped)
				pSession->CompleteSend(cbTransffered);
		}
		
		if (InterlockedDecrement64(&pSession->_IOCount) == 0)
			pSession->_bLogoutFlag = true;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////
// Send Thread 부
/////////////////////////////////////////////////////////////////////////
UINT WINAPI CMMOServer::SendThread(LPVOID arg)
{
	CMMOServer *pServer = reinterpret_cast<CMMOServer *>(arg);

	return pServer->SendThread_Work();
}

bool CMMOServer::SendThread_Work()
{
	CNetSession *pSession;
	while (_bServerOpen)
	{
		for (int iIndex = 0; iIndex < _iMaxSession; iIndex++)
		{
			pSession = _pSessionArray[iIndex];

			if (pSession == nullptr)
				continue;

			if (InterlockedCompareExchange(&pSession->_ISendIO, 1, 0) != 0)
				continue;

			if (pSession->_SendQ.GetUseSize() <= 0)
			{
				InterlockedExchange(&pSession->_ISendIO, false);
				continue;
			}

			if (pSession->_Mode == MODE_NONE || pSession->_Mode == MODE_WAIT_LOGOUT
				|| pSession->_Mode == MODE_LOGOUT_IN_AUTH ||
				pSession->_Mode == MODE_LOGOUT_IN_GAME)
			{
				InterlockedExchange(&pSession->_ISendIO, false);
				continue;
			}

			pSession->SendPost();

			InterlockedAdd(&_Monitor_Counter_PacketSend, 1);

		}

		Sleep(0);
	}
	return true;

}

void CMMOServer::Error(int ErrorCode, WCHAR *szFormatStr, ...)
{
	WCHAR szInMessage[dfLOGMSGLEN_MAX];

	va_list va;
	va_start(va, szInMessage);
	HRESULT hResult = StringCchVPrintf(szInMessage, eERROR_MAX_LEN, szFormatStr, va);
	va_end(va);

	if (FAILED(va))
	{
		// 로그
		SYSLOG(L"ERROR", LOG_ERROR, L"LOG ERROR BUFFER SIZE OVER \n");
		CCrashDump::Crash();
	}

	StringCchPrintf(_szErrorStr, dfLOGMSGLEN_MAX, L"%s\n", szInMessage);
	OnError(ErrorCode, _szErrorStr);
}