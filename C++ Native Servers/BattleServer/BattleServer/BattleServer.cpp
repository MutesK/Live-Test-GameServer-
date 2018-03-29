#include "BattleServer.h"
#include "Engine/MMOServer.h"
#include "Player.h"
#include "MasterLanClient.h"
#include "ChatLanServer.h"
#include "Engine/Parser.h"
#include "Engine/HttpPost.h"
#include "MornitoringAgent.h"
#include "Morntoring.h"


CBattleServer::CBattleServer(int MaxSession)
	:CMMOServer(MaxSession), MaxSession(MaxSession)
{
	ConnectTokenUpdateCount = 0;

	ConnectToken_RamdomGenerator();

	OpenRoom = ReadyRoom = GameRoom = 0;


	Players = new CPlayer[MaxSession];

	for (int i = 0; i < MaxSession; i++)
	{
		SetSessionArray(i, &Players[i]);
		Players[i].SetBattleServer(this);
	}

	
}


CBattleServer::~CBattleServer()
{
}

bool CBattleServer::Start(WCHAR *szServerConfig, bool bNagle)
{
	//CParser *pParser = new CParser(szServerConfig);
	unique_ptr<CParser> pParser{ new CParser(szServerConfig) };
	int BattlePort;
	int PacketCode, PacketKey1, PacketKey2;
	int WorkerThread;
	int ServerNo;

	pParser->GetValue(L"SERVER_NO", (int *)&ServerNo, L"NETWORK");
	pParser->GetValue(L"BIND_IP", BattleServerIP, L"NETWORK");
	pParser->GetValue(L"BIND_PORT", &BattlePort, L"NETWORK");

	pParser->GetValue(L"PACKET_CODE", &PacketCode, L"NETWORK");
	pParser->GetValue(L"PACKET_KEY1", &PacketKey1, L"NETWORK");
	pParser->GetValue(L"PACKET_KEY2", &PacketKey2, L"NETWORK");

	pParser->GetValue(L"WORKER_THREAD", &WorkerThread, L"NETWORK");

	pParser->GetValue(L"MAX_USER", &MAX_USER, L"SYSTEM");
	pParser->GetValue(L"VER_CODE", (int *)&Ver_Code, L"SYSTEM");
	pParser->GetValue(L"MAX_BATTLEROOM", (int *)&MAX_BATTLEROOM, L"SYSTEM");

	Port = BattlePort;
	Rooms = new CRoom[MAX_BATTLEROOM];

	for (int i = 0; i < MAX_BATTLEROOM; i++)
	{
		Rooms[i].RoomNo = i;
		Rooms[i].MaxUser = MAX_USER;
		Rooms[i].RoomStatus = eNOT_USE;
		Rooms[i].GameIndex = 0;
	}

	if (!CMMOServer::Start(L"0.0.0.0", BattlePort, WorkerThread, bNagle, PacketCode, PacketKey1, PacketKey2))
	{
		CCrashDump::Crash();
		return false;
	}

	/////////////////////////////////////////////////////////////////
	_ChatServer.pChatServer = new CChatLanServer(this);

	pParser->GetValue(L"CHAT_IP", _ChatServer.ServerIP, L"NETWORK");
	pParser->GetValue(L"CHAT_PORT", &_ChatServer.ServerPort, L"NETWORK");

	if (!_ChatServer.pChatServer->Start(L"0.0.0.0", _ChatServer.ServerPort, WorkerThread, 100, false))
	{
		CCrashDump::Crash();
		return false;
	}

	/////////////////////////////////////////////////////////////////
	_MasterServer.pMasterServer = new CMasterLanClient(this);

	pParser->GetValue(L"MASTER_IP", _MasterServer.ServerIP, L"NETWORK");
	pParser->GetValue(L"MASTER_PORT", &_MasterServer.ServerPort, L"NETWORK");

	if (!_MasterServer.pMasterServer->Connect(_MasterServer.ServerIP, _MasterServer.ServerPort, false))
	{
		CCrashDump::Crash();
		return false;
	}

	WCHAR _MasterToken[50];
	pParser->GetValue(L"TOKEN", _MasterToken, L"SYSTEM");
	UTF16toUTF8(MasterToken, _MasterToken, 32);

	pParser->GetValue(L"SHDB_API", shAPIURL, L"NETWORK");
	HTTPURLParse();

	for(int i=0; i<5; i++)
		hLoginThread[i] = (HANDLE)_beginthreadex(nullptr, 0, LoginThread, this, 0, nullptr);

	array<BYTE, 32> LoginSessionKey;
	WCHAR _LoginKey[32];

	pParser->GetValue(L"LOGIN_TOKEN", _LoginKey, L"SYSTEM");
	UTF16toUTF8(reinterpret_cast<char *>(LoginSessionKey.data()), _LoginKey, 32);

	array<WCHAR, 16> MornitorIP;
	int MorintorPort;

	pParser->GetValue(L"MORNITOR_IP", &MornitorIP[0], L"NETWORK");
	pParser->GetValue(L"MORNITOR_PORT", &MorintorPort, L"NETWORK");

	MonitorSenderAgent = make_shared<CMornitoringAgent>(*this, ServerNo, LoginSessionKey);
	if (!MonitorSenderAgent->Connect(MornitorIP, MorintorPort, true))
		return false;

	pMonitorAgent = make_shared<CMornitoring>();
}

void CBattleServer::Stop(void)
{
	CMMOServer::Stop();

	_MasterServer.pMasterServer->Stop();
	_ChatServer.pChatServer->Stop();

	WaitForMultipleObjects(10, hLoginThread, true, INFINITE);
}

void CBattleServer::Monitoring()
{
	
	wprintf(L"====================================================\n");
	wprintf(L"\t\t Monitoring \t\n");
	wprintf(L"====================================================\n");
	wprintf(L"Accpet : %d\n", _Monitor_AcceptSocket);
	wprintf(L"ALL Mode : %d\n", _Monitor_SessionAllMode);
	wprintf(L"Auth Mode : %d\n", _Monitor_SessionAuthMode);
	wprintf(L"Game Mode : %d\n", _Monitor_SessionGameMode);
	wprintf(L"====================================================\n");
	wprintf(L"Auth Update TPS : %d\n", _Monitor_Counter_AuthUpdate);
	wprintf(L"Game Update TPS : %d\n", _Monitor_Counter_GameUpdate);
	wprintf(L"Accept Counter TPS : %d\n", _Monitor_Counter_Accept);
	wprintf(L"Packet Proc TPS : %d\n", _Monitor_Counter_PacketProc);
	wprintf(L"Packet Send TPS : %d\n", _Monitor_Counter_PacketSend);
	wprintf(L"====================================================\n");
	wprintf(L"Login Request : %d\n", LoginQueue.GetUseSize());
	wprintf(L"====================================================\n");
	wprintf(L"Open Room : %d\n", OpenRoom);
	wprintf(L"Ready Room : %d\n", ReadyRoom);
	wprintf(L"Game Room : %d\n", GameRoom);
	wprintf(L"Max User : %d \n", MAX_USER);
	wprintf(L"================== Room User Status ===============\n");
	RoomStatusMonitoring();
	wprintf(L"====================================================\n");
	MonitoringSender();
	wcout << L"Packet Pool : " << CPacketBuffer::PacketPool.GetChunkSize() << L"\n";
	wcout << L"Packet Pool Use: " << CPacketBuffer::PacketPool.GetAllocCount() << L"\n\n";
	_ChatServer.pChatServer->Monitoring();
	_MasterServer.pMasterServer->Monitoring();
	wprintf(L"====================================================\n");



	_Monitor_Counter_AuthUpdate = _Monitor_Counter_GameUpdate = _Monitor_Counter_Accept
		= _Monitor_Counter_PacketProc = _Monitor_Counter_PacketSend = 0;
}

void CBattleServer::HTTPURLParse()
{
	httpPort = 80;

	// 1. URL에 http://가 있어야 하는 전제임. 
	WCHAR *szPointer = shAPIURL;

	if (wcsncmp(szPointer, L"http://", 7) != 0)
	{
		SYSLOG(L"HTTP", LOG_ERROR, L"Http Header isn't Exist");
		CCrashDump::Crash();
	}
	szPointer += 7;

	// 2. : 이 오거나 /가 올때까지 넘긴다.
	WCHAR *szStartPointer = szPointer;
	while (1)
	{
		if (*szPointer == L'\0')
		{
			SYSLOG(L"HTTP", LOG_ERROR, L"Http URL Syntax Error");
			CCrashDump::Crash();
		}
		if (*szPointer == L':' || *szPointer == L'/')
			break;

		szPointer++;
	}

	// 3. httpURL을 넣는다.
	WCHAR httpDomain[100];
	int Len = szPointer - szStartPointer;
	memcpy(httpDomain, szStartPointer, Len * 2);
	httpDomain[Len] = L'\0';

	DomainToIP(httpDomain, httpURL);


	// 4. szPointer가 : 이라면 다음은 포트를 입력, 아니라면 Path에 들어간다.
	if (*szPointer == L':')
	{
		// 포트 입력한다.
		szPointer++;
		szStartPointer = szPointer;

		while (1)
		{
			if (*szPointer == L'\0')
			{
				SYSLOG(L"HTTP", LOG_ERROR, L"Http URL Syntax Error");

				CCrashDump::Crash();
			}

			if (*szPointer == L'/')
				break;

			szPointer++;
		}

		WCHAR PortTemp[6];
		int Len = szPointer - szStartPointer;
		memcpy(PortTemp, szStartPointer, Len * 2);
		PortTemp[Len] = L'\0';

		httpPort = _wtoi(PortTemp);
	}

	// Path 입력
	szStartPointer = szPointer;

	while (1)
	{
		if (*szPointer == L'\0')
			break;

		szPointer++;
	}

	Len = szPointer - szStartPointer;
	memcpy(httpPath, szStartPointer, Len * 2);
	httpPath[Len] = L'\0';
}

void CBattleServer::OnError(int iErrorCode, WCHAR *szError)
{
	SYSLOG(L"ERROR", LOG_ERROR, szError);
}

void	CBattleServer::LoginProcessReq(CPlayer *pPlayer)
{
	LoginQueue.Enqueue(&pPlayer);
}
UINT WINAPI CBattleServer::LoginThread(LPVOID arg)
{
	CBattleServer *pServer = (CBattleServer *)arg;

	return pServer->LoginWork();
}

UINT CBattleServer::LoginWork()
{
	CPlayer *pPlayer = nullptr;
	
	while (_bServerOpen)
	{
		while (LoginQueue.Dequeue(&pPlayer))
		{
			if(pPlayer != nullptr)
				ReqLogin(pPlayer);

			pPlayer = nullptr;

			Sleep(0);
		}

		pPlayer = nullptr;

		Sleep(20);
	}

	return true;
}

void CBattleServer::RoomStatusMonitoring()
{
	for (int i = 10; i <= MAX_BATTLEROOM; i+=10)
	{
		for (int j = i - 10; j < i; j++)
		{
			wprintf(L"%d ", Rooms[j].PlayerMap.size());
		}

		wprintf(L"\n");
	}
}

void	CBattleServer::SendPacketAllInRoom(CPacketBuffer *pBuffer, CPlayer *pExceptPlayer, bool isExcept)
{
	std::map<INT64, CPlayer *> *pPlayerMap = &Rooms[pExceptPlayer->RoomNo].PlayerMap;

	Rooms[pExceptPlayer->RoomNo].ReadLock();
	for (auto iter = pPlayerMap->begin(); iter != pPlayerMap->end(); ++iter)
	{
		CPlayer *pSender = iter->second;

		if (isExcept && pSender == pExceptPlayer)
			continue;

		pBuffer->AddRefCount();
		if (!pSender->SendPacket(pBuffer))
			pBuffer->Free();

		pSender->LastTick = CUpdateTime::GetTickCount();
	}
	Rooms[pExceptPlayer->RoomNo].ReadUnLock();

	pBuffer->Free();
}

void	CBattleServer::SendPacketAllInRoom(CPacketBuffer *pBuffer, CRoom *pRoom)
{
	std::map<INT64, CPlayer *> *pPlayerMap = &pRoom->PlayerMap;

	pRoom->ReadLock();
	for (auto iter = pPlayerMap->begin(); iter != pPlayerMap->end(); ++iter)
	{
		CPlayer *pSender = iter->second;

		pBuffer->AddRefCount();
		if (!pSender->SendPacket(pBuffer))
			pBuffer->Free();

		pSender->LastTick = CUpdateTime::GetTickCount();
	}
	pRoom->ReadUnLock();

	pBuffer->Free();
}

void CBattleServer::MonitoringSender()
{
	time_t   current_time;
	int TimeStamp = time(&current_time);

	// 전송 리스트

	// 4. 패킷풀 사용
	MonitorSenderAgent->Send(dfMONITOR_DATA_TYPE_BATTLE_PACKET_POOL, CPacketBuffer::PacketPool.GetAllocCount(), TimeStamp);

	// 1. ON / OFF
	MonitorSenderAgent->Send(dfMONITOR_DATA_TYPE_BATTLE_SERVER_ON, 1, TimeStamp);
	// 2. CPU 사용률

	int CpuUsage = pMonitorAgent->GetCpuUsage();
	MonitorSenderAgent->Send(dfMONITOR_DATA_TYPE_BATTLE_CPU, CpuUsage, TimeStamp);

	// 3. 메모리 유저 커밋사용
	int MemoryUsage = pMonitorAgent->GetMemoryUsage();
	MonitorSenderAgent->Send(dfMONITOR_DATA_TYPE_BATTLE_MEMORY_COMMIT, MemoryUsage, TimeStamp);


	// 5. Auth 스레드 초당 루프 수
	MonitorSenderAgent->Send(dfMONITOR_DATA_TYPE_BATTLE_AUTH_FPS, _Monitor_Counter_AuthUpdate, TimeStamp);

	// 6. Game 스레드 초당 루프 수
	MonitorSenderAgent->Send(dfMONITOR_DATA_TYPE_BATTLE_GAME_FPS, _Monitor_Counter_GameUpdate, TimeStamp);
	// 7. 배틀 서버 전체 접속수
	MonitorSenderAgent->Send(dfMONITOR_DATA_TYPE_BATTLE_SESSION_ALL, _Monitor_SessionAllMode, TimeStamp);
	// 8. Auth 모드 인원
	MonitorSenderAgent->Send(dfMONITOR_DATA_TYPE_BATTLE_SESSION_AUTH, _Monitor_SessionAuthMode, TimeStamp);
	// 9. Game 모드 인원
	MonitorSenderAgent->Send(dfMONITOR_DATA_TYPE_BATTLE_SESSION_GAME, _Monitor_SessionGameMode, TimeStamp);
	// 10. 대기방 수
	MonitorSenderAgent->Send(dfMONITOR_DATA_TYPE_BATTLE_ROOM_WAIT, OpenRoom, TimeStamp);
	// 11. 플레이 방 수
	MonitorSenderAgent->Send(dfMONITOR_DATA_TYPE_BATTLE_ROOM_PLAY, GameRoom, TimeStamp);
}