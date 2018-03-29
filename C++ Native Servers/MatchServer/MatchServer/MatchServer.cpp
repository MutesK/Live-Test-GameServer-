#include "MatchServer.h"
#include "LanMatchServer.h"
#include "Engine/Parser.h"
#include "Engine/HttpPost.h"
#include "Engine/DBConnectorTLS.h"
#include "Engine/rapidjson\document.h"
#include "Engine/rapidjson\writer.h"
#include "Engine/rapidjson\stringbuffer.h"
#include "MornitoringAgent.h"
#include "Morntoring.h"
using namespace rapidjson;


CMatchServer::CMatchServer()
{
	InitializeSRWLock(&PlayerLock);
	InitializeSRWLock(&PlayerUniqueLock);
	ReqGameRoomTPS = RoomEnterSuccessTPS = RoomEnterFailTPS = 0;

	LoginRequest = RoomRequest = 0;
	UniqueIndex = 0;
}


CMatchServer::~CMatchServer()
{
	Stop();
}

bool CMatchServer::Start(WCHAR *szServerConfig, bool NagleOption)
{
	//CParser* Parser = new CParser(szServerConfig);
	unique_ptr<CParser> Parser {new CParser(szServerConfig)};
	WCHAR IP[16];
	int Port;

	WCHAR MasterServerIP[16];
	WCHAR _MasterToken[32];

	int MasterServerPort;

	int WorkerThread;
	int MaxSession;

	int PacketKey1, PacketKey2, PacketCode;

	Parser->GetValue(L"SERVER_NO", (int *)&ServerNo, L"NETWORK");

	Parser->GetValue(L"BIND_IP", IP, L"NETWORK");
	Parser->GetValue(L"BIND_PORT", &Port, L"NETWORK");

	Parser->GetValue(L"MASTER_IP", MasterServerIP, L"NETWORK");
	Parser->GetValue(L"MASTER_PORT", &MasterServerPort, L"NETWORK");

	Parser->GetValue(L"WORKER_THREAD", &WorkerThread, L"NETWORK");
	Parser->GetValue(L"SHDB_API", shAPIURL, L"NETWORK");

	Parser->GetValue(L"CLIENT_MAX", &MaxSession, L"SYSTEM");
	Parser->GetValue(L"PACKET_CODE", &PacketCode, L"SYSTEM");
	Parser->GetValue(L"PACKET_KEY1", &PacketKey1, L"SYSTEM");
	Parser->GetValue(L"PACKET_KEY2", &PacketKey2, L"SYSTEM");

	Parser->GetValue(L"TOKEN", _MasterToken, L"SYSTEM");
	UTF16toUTF8(MasterToken, _MasterToken, 32);

	HTTPURLParse();

	Parser->GetValue(L"VER_CODE", (int *)&Ver_Code, L"SYSTEM");



	// DB MatchMaking_Status 에 정보 기입 준비
	WCHAR DB_IP[16];
	WCHAR DB_Name[100];
	WCHAR DB_User[100];
	WCHAR DB_Pass[1000];

	int DB_Port;
	Parser->GetValue(L"DB_IP", DB_IP, L"DATABASE");
	Parser->GetValue(L"DB_PORT", &DB_Port, L"DATABASE");
	Parser->GetValue(L"DB_NAME", DB_Name, L"DATABASE");
	Parser->GetValue(L"DB_USER", DB_User, L"DATABASE");
	Parser->GetValue(L"DB_PASS", DB_Pass, L"DATABASE");



	if (!CNetServer::Start(L"0.0.0.0", Port, WorkerThread, NagleOption, MaxSession, PacketCode,
		PacketKey1, PacketKey2))
		return false;

	hTimeOut = (HANDLE)_beginthreadex(nullptr, 0, hTimedOutThread, this, 0, nullptr);

	pClient = make_shared<CLanMatchServer>(this, ServerNo, MasterToken);
	//pClient = new CLanMatchServer(this, ServerNo, MasterToken);
	if (!pClient->Connect(MasterServerIP, MasterServerPort, true))
		return false;

	pConnect = make_shared<CDBConnectorTLS>(DB_IP, DB_User, DB_Pass, DB_Name, DB_Port, 1000);
	FirstDBInsert(IP, Port);

	array<BYTE, 32> LoginSessionKey;
	WCHAR _LoginKey[32];

	Parser->GetValue(L"LOGIN_TOKEN", _LoginKey, L"SYSTEM");
	UTF16toUTF8(reinterpret_cast<char *>(LoginSessionKey.data()), _LoginKey, 32);
	
	array<WCHAR, 16> MornitorIP;
	int MorintorPort;

	Parser->GetValue(L"MORNITOR_IP", &MornitorIP[0], L"NETWORK");
	Parser->GetValue(L"MORNITOR_PORT", &MorintorPort, L"NETWORK");

	pMornitorSender = make_shared<CMornitoringAgent>(*this, ServerNo, LoginSessionKey);
	if (!pMornitorSender->Connect(MornitorIP, MorintorPort, true))
		return false;

	pMonitorAgent = make_shared<CMornitoring>();


	return true;
}
void CMatchServer::FirstDBInsert(WCHAR *IP, int Port)
{
	pConnect->Query(L"INSERT INTO server (serverno, ip, port, connectuser, heartbeat) VALUES (%d, \"%s\", %d, 0, NOW()) ON DUPLICATE KEY UPDATE ip = \"%s\", port = %d", ServerNo, IP, Port, IP, Port);
}

void CMatchServer::ConnectUserDBUpdate(int ConnectUser)
{
	//UPDATE `matchmaking_status`.`server` SET `connectuser`='5' WHERE `serverno`='1';
	pConnect->Query(L"UPDATE server SET connectuser=%d WHERE serverno=%d", ConnectUser, ServerNo);
}
void CMatchServer::Stop()
{
	CNetServer::Stop();

	pConnect->Query(L"DELETE FROM `matchmaking_status`.`server` WHERE `serverno`='%d'", ServerNo);
}

void CMatchServer::OnWorkerThreadBegin()
{

}
void CMatchServer::OnWorkerThreadEnd()
{

}

void CMatchServer::OnClientJoin(ULONG64 OutSessionID, WCHAR *pClientIP, int Port)
{

}

void CMatchServer::OnClientLeave(ULONG64 SessionID)
{
	DisconnectPlayer(SessionID);
}

void CMatchServer::DisconnectPlayer(ULONG64 SessionID)
{
	AcquireSRWLockShared(&PlayerLock);
	auto finditer = PlayerMap.find(SessionID);

	if (finditer == PlayerMap.end())
	{
		ReleaseSRWLockShared(&PlayerLock);
		return;
	}
	ReleaseSRWLockShared(&PlayerLock);


	Player *pPlayer = finditer->second;

	if (pPlayer->ReqRoomInfo)	// 방 입장 실패를 전송
		ReqGameRoomEnterFail(pPlayer);

	AcquireSRWLockExclusive(&PlayerLock);
	PlayerMap.erase(SessionID);
	ReleaseSRWLockExclusive(&PlayerLock);

	AcquireSRWLockExclusive(&PlayerUniqueLock);
	PlayerUniqueMap.erase(pPlayer->ClientUniqueKey);
	ReleaseSRWLockExclusive(&PlayerUniqueLock);

	memset(pPlayer, 0, sizeof(Player));

	PlayerPool.Free(pPlayer);
}

bool CMatchServer::OnConnectionRequest(WCHAR *szIP, int Port)
{
	return ServerOpen;
}

void CMatchServer::OnRecv(ULONG64 OutSessionID, CPacketBuffer *pOutBuffer)
{
	WORD PacketType;

	*pOutBuffer >> PacketType;

	switch (PacketType)
	{
		// 매치메이킹 서버로 로그인 요청
	case en_PACKET_CS_MATCH_REQ_LOGIN:
		ReqLogin(OutSessionID, pOutBuffer);
		break;
		// 방 정보 요청
	case en_PACKET_CS_MATCH_REQ_GAME_ROOM:
		ReqGameRoom(OutSessionID);
		break;
	case en_PACKET_CS_MATCH_REQ_GAME_ROOM_ENTER:
		ReqGameRoomEnter(OutSessionID, pOutBuffer);
		break;
	default:
		CCrashDump::Crash();
		break;
	}

	pOutBuffer->Free();
}

void CMatchServer::OnSend(ULONG64 OutSessionID, int SendSize)
{

}

void CMatchServer::OnError(int errorcode, WCHAR *szMessage)
{

}

void CMatchServer::Monitoring()
{
	wcout << L"===========================================================\n";
	wprintf(L"Client Connect : %d\n", m_NowSession);

	pClient->Monitoring();
	wcout << L"Packet Pool : " << CPacketBuffer::PacketPool.GetChunkSize() << L"\n";
	wcout << L"Packet Pool Use: " << CPacketBuffer::PacketPool.GetAllocCount() << L"\n\n";

	wcout << L"Player Pool : " << PlayerPool.GetAllocCount() << L"\n";
	wcout << L"Player Count : " << PlayerMap.size() << L"\n\n";

	wcout << L"Accept Total : " << m_AcceptTotal << L"\n";
	wcout << L"Accpet TPS : " << m_AcceptTPS << L"\n";

	wcout << L"Login Request : " << LoginRequest << L"\n";
	wcout << L"Room Request : " << RoomRequest << L"\n";

	wcout << L"Recv PacketTPS :" << m_RecvPacketTPS << endl;
	wcout << L"Send PacketTPS :" << m_SendPacketTPS << endl;

	wprintf(L"Request Game Room Info Process  : %d\n", ReqGameRoomTPS);
	wprintf(L"Request Game Enter Success Process  : %d\n", RoomEnterSuccessTPS);
	wprintf(L"Request Game Enter Fail Process  : %d\n", RoomEnterFailTPS);
	wcout << L"===========================================================\n";

	ConnectUserDBUpdate(m_NowSession);
	MonitoringSend();

	m_RecvPacketTPS = m_SendPacketTPS = 0;
	m_AcceptTPS = 0;
	ReqGameRoomTPS = RoomEnterSuccessTPS = RoomEnterFailTPS = 0;
}

void CMatchServer::HTTPURLParse()
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
	httpURL[Len] = L'\0';

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

void CMatchServer::ReqLogin(ULONG64 OutSessionID, CPacketBuffer *pBuffer)
{
	//	매치메이킹 서버는 클라이언트 접속시 SessionKey 를 shDB 에서 확인하여 로그인 인증 함.
	//	접속 후 클라이언트 고유 키 (Client Key) 를 생성하여 관리한다. (전 매치메이킹 서버 대상의 유니크 값 이어야 함)
	//
	//	Client Key 의 용도는 마스터 서버에서 클라이언트를 구분하는 값으로 
	//	매치메이킹 서버가 마스터 서버에게 방 생성 요청, 입장확인 시 사용 함.
	Player *pPlayer = PlayerPool.Alloc();

	memset(pPlayer, 0, sizeof(Player));

	pPlayer->SessionID = OutSessionID;

	INSERT_SESSION(pPlayer->ClientUniqueKey, ServerNo, UniqueIndex);
	InterlockedIncrement64(&UniqueIndex);

	pPlayer->LastRecv = CUpdateTime::GetTickCount();

	pair <ULONG64, Player *> insertpair = { OutSessionID , pPlayer };

	AcquireSRWLockExclusive(&PlayerLock);
	PlayerMap.insert(insertpair);
	ReleaseSRWLockExclusive(&PlayerLock);

	__int64 AccountNo;
	char SessionKey[64];
	BYTE Result = 1;
	UINT VerCode;

	InterlockedAdd64(&LoginRequest, 1);

	*pBuffer >> AccountNo;
	pBuffer->GetData(SessionKey, 64);
	*pBuffer >> VerCode;

	// shDB에서 확인한다.
	WCHAR PathURL[200];
	StringCchPrintf(PathURL, 200, L"%s%s", httpPath, L"select_account.php");

	// AccountNo를 JSON으로 묶어서 쏴야된다.
	char PostDATA[100];
	char RecvDATA[1400] = "";

	sprintf_s(PostDATA, 100, "{\"accountno\":%I64d}", AccountNo);
	
	int retry = 20000;
	int ret = -1;
	int ResultCode = 0;
	while (1)
	{
		ret = HTTP_POST(httpURL, PathURL, PostDATA, RecvDATA);

		if (ret == 200 || retry == 0)
			break;

		retry--;
	}


	if (retry == 0 || ret == -1)
	{
		// 에러 로그 & 기타 오류
		Result = 4;
		ResLogin(OutSessionID, Result);
		SYSLOG(L"LOGIN", LOG_ERROR, L"Request Login, HTTP POST ERROR. Result Code : %d", ret);

		//CCrashDump::Crash();
		return;
	}

	// RecvDATA를 RapidJSON을 이용해서 값을 얻어온다.
	Document document;
	document.Parse(RecvDATA);
	
	ResultCode = document["result"].GetInt();
	if (ResultCode == -10)
	{
		// AccountNo 없음
		Result = 3;
		ResLogin(OutSessionID, Result);
		return;
	}

	if (ResultCode != 1)
	{
		// 기타 오류
		Result = 4;
		ResLogin(OutSessionID, Result);
		SYSLOG(L"LOGIN", LOG_ERROR, L"SHDB LogDB ETC Error");
		SYSLOG(L"LOGIN", LOG_ERROR, L"%s", RecvDATA);

		return;
	}

	if (memcmp(SessionKey, document["sessionkey"].GetString(), 64) != 0)
	{
		// 세션키 오류
		Result = 2;
		ResLogin(OutSessionID, Result);
		return;
	}

	if (VerCode != Ver_Code)
	{
		Result = 5;
		ResLogin(OutSessionID, Result);
		return;
	}
	
	////////////////////////////////////////////////////////////////////////////////////////////////////
	AcquireSRWLockShared(&PlayerLock);
	auto iter = PlayerMap.find(OutSessionID);

	if (iter == PlayerMap.end())
	{
		ReleaseSRWLockShared(&PlayerLock);
		Disconnect(OutSessionID);
		return;
	}
	ReleaseSRWLockShared(&PlayerLock);

	pPlayer = iter->second;

	pair <ULONG64, Player *> insertUnipair = { pPlayer->ClientUniqueKey , pPlayer };
	pPlayer->AccountNo = AccountNo;
	pPlayer->LastRecv = CUpdateTime::GetTickCount();
	memcpy(pPlayer->SessionKey, SessionKey, 64);

	AcquireSRWLockExclusive(&PlayerUniqueLock);
	auto resultpair = PlayerUniqueMap.insert(insertUnipair);
	ReleaseSRWLockExclusive(&PlayerUniqueLock);

	if (!resultpair.second)
	{
		Disconnect(OutSessionID);
		return;
	}

	if (ResLogin(OutSessionID, 1) == 0)
		DisconnectPlayer(pPlayer->SessionID);

	InterlockedAdd64(&LoginRequest, -1);

	pPlayer->isLogined = true;
}
void CMatchServer::MasterServerConnect()
{
	ServerOpen = true;

	AcquireSRWLockShared(&PlayerLock);
	for (auto iter = PlayerMap.begin(); iter != PlayerMap.end(); ++iter)
	{
		Player * pPlayer = iter->second;

		if (pPlayer->reqRoom)
		{
			CPacketBuffer *pBuffer = CPacketBuffer::Alloc();
			//	매치메이킹 서버가 마스터 서버에게 게임방 정보를 요청
			WORD Type = en_PACKET_MAT_MAS_REQ_GAME_ROOM;

			*pBuffer << Type << pPlayer->ClientUniqueKey;
			if (!pClient->MasterSendPacket(pBuffer))
				ResGameRoom(pPlayer, 0, nullptr);
			else
				InterlockedAdd64(&RoomRequest, 1);

			pPlayer->LastRecv = CUpdateTime::GetTickCount();
		}
	}
	ReleaseSRWLockShared(&PlayerLock);
}
void CMatchServer::MasterServerDisconnect()
{
	ServerOpen = false;
}

bool CMatchServer::ResLogin(ULONG64 OutSessionID, BYTE ResultStatus)
{
	WORD Type = en_PACKET_CS_MATCH_RES_LOGIN;

	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

	*pBuffer << Type << ResultStatus;

	return SendPacket(OutSessionID, pBuffer);
		
}
/////////////////////////////////////////////////////////////////////////////////////////////
void CMatchServer::ReqGameRoom(ULONG64 OutSessionID)
{
	//	클라이언트가 매치메이킹 서버에게 방 정보를 요청 함.
	//
	//	매치메이킹 서버는 마스터 서버에게 ClientKey 와 방정보 요청을 보낸 뒤
	//	마스터 서버에게서 받은 정보를 클라이언트에게 돌려주면 된다.

	AcquireSRWLockShared(&PlayerLock);
	auto finditer = PlayerMap.find(OutSessionID);
	if (finditer == PlayerMap.end())
	{
		Disconnect(OutSessionID);
		ReleaseSRWLockShared(&PlayerLock);
		return;
	}
	ReleaseSRWLockShared(&PlayerLock);


	Player *pPlayer = finditer->second;
	pPlayer->ReqRoomInfo = true;
	pPlayer->reqRoom = true;

	///////////////////////////////////////////////////////////////////////////////////
	// 마스터 서버에게 게임방 정보를 요청한다.
	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();
	//	매치메이킹 서버가 마스터 서버에게 게임방 정보를 요청
	WORD Type = en_PACKET_MAT_MAS_REQ_GAME_ROOM;

	*pBuffer << Type << pPlayer->ClientUniqueKey;
	if (!pClient->MasterSendPacket(pBuffer))
		ResGameRoom(pPlayer, 0, nullptr);
	else
		InterlockedAdd64(&RoomRequest, 1);

	pPlayer->LastRecv = CUpdateTime::GetTickCount();
}

void CMatchServer::ResGameRoom(Player *pPlayer, BYTE Status, CPacketBuffer *BattleServerInfo)
{

	if (!pPlayer->isLogined)
		return;

	switch (Status)
	{
	case 0:
	{
		CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

		WORD Type = en_PACKET_CS_MATCH_RES_GAME_ROOM;

		*pBuffer << Type << Status;

		if (SendPacket(pPlayer->SessionID, pBuffer) == 0)
			DisconnectPlayer(pPlayer->SessionID);
	}
	break;
	case 1:
	{
		*BattleServerInfo >> pPlayer->BattleServerNo;
		BattleServerInfo->GetData(reinterpret_cast<char *>(pPlayer->BattleServerIP), 32);
		*BattleServerInfo >> pPlayer->BattleServerPort >> pPlayer->RoomNo;
		BattleServerInfo->GetData(reinterpret_cast<char *>(pPlayer->ConnectToken), 32);
		BattleServerInfo->GetData(reinterpret_cast<char *>(pPlayer->EnterToken), 32);

		BattleServerInfo->GetData(reinterpret_cast<char *>(pPlayer->ChatServerIP), 32);
		*BattleServerInfo >> pPlayer->ChatServerPort;

		CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

		WORD Type = en_PACKET_CS_MATCH_RES_GAME_ROOM;

		*pBuffer << Type << Status << pPlayer->BattleServerNo;
		pBuffer->PutData(reinterpret_cast<char *>(pPlayer->BattleServerIP), 32);
		*pBuffer << pPlayer->BattleServerPort << pPlayer->RoomNo;
		pBuffer->PutData(reinterpret_cast<char *>(pPlayer->ConnectToken), 32);
		pBuffer->PutData(reinterpret_cast<char *>(pPlayer->EnterToken), 32);

		pBuffer->PutData(reinterpret_cast<char *>(pPlayer->ChatServerIP), 32);
		*pBuffer << pPlayer->ChatServerPort;

		if (SendPacket(pPlayer->SessionID, pBuffer) == 0)
			DisconnectPlayer(pPlayer->SessionID);
	}
	break;
	}

	// pPlayer->ReqRoomInfo = false;

	InterlockedAdd64(&RoomRequest, -1);
	InterlockedAdd64(&ReqGameRoomTPS, 1);
}
/////////////////////////////////////////////////////////////////////////////////////////////

void CMatchServer::ReqGameRoomEnter(ULONG64 OutSessionID, CPacketBuffer *pBuffer)
{
	// 방 입장 성공 알림
	//	클라이언트는 en_PACKET_CS_MATCH_RES_GAME_ROOM 방 정보를 받으면 배틀서버에 접속을 시도한다.
	//
	//	클라이언트가 배틀서버로 부터 방 입장 성공 안내를 받기 전 까지는
	//	매치메이킹 서버와 배틀서버 2개의 서버에 모두 접속을 유지하고 있음
	//
	//	배틀서버 방 입장이 성공되면, 매치메이킹 서버에게 이 패킷을 보내서 방 입장 성공을 알려줌.
	//
	//	매치메이킹 서버는 본 패킷을 받으면 마스터 서버에게 전달 해주며, 실제 해당 방의 접속자 인원을 변경 한다.


	AcquireSRWLockShared(&PlayerLock);
	auto finditer = PlayerMap.find(OutSessionID);
	if (finditer == PlayerMap.end())
	{
		SYSLOG(L"RoomEnter", LOG_SYSTEM, L"SessionID : %llu isn't Found in PlayerMap !!");
		ReleaseSRWLockShared(&PlayerLock);
		return;
	}
	ReleaseSRWLockShared(&PlayerLock);

	Player *pPlayer = finditer->second;
	pPlayer->ReqRoomInfo = false;

	WORD BattleServerNo;
	int RoomNo;

	*pBuffer >> BattleServerNo >> RoomNo;

	///////////////////////////////////////////////////////////////////////////////////
	// 방 입장 성공여부를 마스터에게 전달한다.
	ReqGameRoomEnterSuccess(pPlayer, BattleServerNo, RoomNo);
	///////////////////////////////////////////////////////////////////////////////////


	pPlayer->LastRecv = CUpdateTime::GetTickCount();

	// 클라이언트에게 수신확인을 보내준다.
	ResGameRoomEnter(pPlayer);
}

void CMatchServer::ResGameRoomEnter(Player *pPlayer)
{
	//	매치메이킹 서버는 위의 방 입장 성공 패킷을 받으면 마스터 서버에게 이를 전달 한 뒤에
	//	마스터 서버로부터 회신이 오면 클라이언트에게 결과를 보내준다.
	//
	//	이 패킷을 받으면 클라이언트는 매치메이킹 서버와의 연결을 끊게 된다.
	//	굳이 RES 확인 패킷이 있는 이유는, 클라이언트가 일방적으로 보내고 끊어 버리면 
	//	매치메이킹 서버에게 100% 전달 되었다는 확신이 어려울 수 있으므로 REQ 와 RES 를 두어서 진행.

	WORD Type = en_PACKET_CS_MATCH_RES_GAME_ROOM_ENTER;

	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();
	*pBuffer << Type;

	SendPacket(pPlayer->SessionID, pBuffer);
}



/////////////////////////////////////////////////////////////
// MasterServer -> MatchServer -> Client
/////////////////////////////////////////////////////////////
void CMatchServer::MasterServerResGameRoom(CPacketBuffer *pBuffer)
{
	UINT64 ClientKey;
	BYTE Status;

	*pBuffer >> ClientKey >> Status;

	AcquireSRWLockShared(&PlayerUniqueLock);
	auto finditer = PlayerUniqueMap.find(ClientKey);
	if (finditer == PlayerUniqueMap.end())
	{
		ReleaseSRWLockShared(&PlayerUniqueLock);
		return;
	}
	ReleaseSRWLockShared(&PlayerUniqueLock);

	Player *pPlayer = finditer->second;

	if (pPlayer == nullptr)
		return;
	
	switch (Status)
	{
	case 0:
		// 방 정보 얻기 실패
		ResGameRoom(pPlayer);
		break;
	case 1:
		// 방 정보 얻기 성공
		ResGameRoom(pPlayer, 1, pBuffer);
		break;
	}
}


////////////////////////////////////////////////////////////////////////////////////////////
// 플레이어 타임아웃 체크
////////////////////////////////////////////////////////////////////////////////////////////
UINT WINAPI CMatchServer::hTimedOutThread(LPVOID arg)
{
	CMatchServer *pServer = reinterpret_cast<CMatchServer *>(arg);

	return pServer->TimeOutCheck();
}

UINT CMatchServer::TimeOutCheck()
{
	ULONG64 SpendTick;

	while (m_bServerOpen)
	{
		Sleep(2000);

		AcquireSRWLockExclusive(&PlayerLock);
		auto iter = PlayerMap.begin();
		ReleaseSRWLockExclusive(&PlayerLock);

		while (1)
		{
			AcquireSRWLockExclusive(&PlayerLock);
			if (iter == PlayerMap.end())
			{
				ReleaseSRWLockExclusive(&PlayerLock);
				break;
			}
			ReleaseSRWLockExclusive(&PlayerLock);

			SpendTick = CUpdateTime::GetTickCount() - iter->second->LastRecv;
			if (SpendTick >= 30000)
			{
				Player *pPlayer = iter->second;
				//SYSLOG(L"Timeout", LOG_SYSTEM, L"Timeout Dectected %I64d", pPlayer->SessionID);
				//Disconnect(pPlayer->SessionID);
			}
			AcquireSRWLockExclusive(&PlayerLock);
			iter++;
			ReleaseSRWLockExclusive(&PlayerLock);
		}
	}

	return 1;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// 마스터 서버에게 입장 결과 
void CMatchServer::ReqGameRoomEnterSuccess(Player *pPlayer, WORD BattleServerNo, int RoomNo)
{
	WORD Type = en_PACKET_MAT_MAS_REQ_ROOM_ENTER_SUCCESS;

	CPacketBuffer *pSendBuffer = CPacketBuffer::Alloc();
	*pSendBuffer << Type << BattleServerNo << RoomNo << pPlayer->ClientUniqueKey;

	pClient->MasterSendPacket(pSendBuffer);

	InterlockedAdd64(&RoomEnterSuccessTPS, 1);
}
void CMatchServer::ReqGameRoomEnterFail(Player *pPlayer)
{
	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

	WORD Type = en_PACKET_MAT_MAS_REQ_ROOM_ENTER_FAIL;

	*pBuffer << Type << pPlayer->ClientUniqueKey;

	pClient->MasterSendPacket(pBuffer);
	InterlockedAdd64(&RoomEnterFailTPS, 1);

	SYSLOG(L"ROOM", LOG_SYSTEM, L"Room Enter Fail. CHAT IP :%s PORT : %d, BATTLE NO : %d, BATTLE IP : %s, PORT : %d", pPlayer->ChatServerIP,
		pPlayer->ChatServerPort, pPlayer->BattleServerNo, pPlayer->BattleServerIP, pPlayer->BattleServerPort);

}
/////////////////////////////////////////////////////////////////////////////////////////////


void CMatchServer::MonitoringSend()
{
	// 보낼 정보 List
	// 2. CPU 사용률 (Kernel + User)
	time_t   current_time;
	int TimeStamp = time(&current_time);

	// 4. 패킷풀 사용량
	pMornitorSender->Send(dfMONITOR_DATA_TYPE_MATCH_PACKET_POOL, CPacketBuffer::PacketPool.GetAllocCount(), TimeStamp);

	pMornitorSender->Send(dfMONITOR_DATA_TYPE_MATCH_SERVER_ON, 1, TimeStamp);

	int CpuUsage = pMonitorAgent->GetCpuUsage();
	pMornitorSender->Send(dfMONITOR_DATA_TYPE_MATCH_CPU, CpuUsage, TimeStamp);


	// 3. 메치메이킹 메모리 유저 커밋 사용량

	int MemoryUsage = pMonitorAgent->GetMemoryUsage();
	pMornitorSender->Send(dfMONITOR_DATA_TYPE_MATCH_MEMORY_COMMIT, MemoryUsage, TimeStamp);



	// 5. 접속 세션

	pMornitorSender->Send(dfMONITOR_DATA_TYPE_MATCH_SESSION, m_NowSession, TimeStamp);

	// 6. 접속 유저

	pMornitorSender->Send(dfMONITOR_DATA_TYPE_MATCH_PLAYER, PlayerMap.size(), TimeStamp);


	// 7. 초당 배정 성공 수

	pMornitorSender->Send(dfMONITOR_DATA_TYPE_MATCH_MATCHSUCCESS, RoomEnterSuccessTPS, TimeStamp);

}