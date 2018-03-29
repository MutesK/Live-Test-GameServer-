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



	// DB MatchMaking_Status �� ���� ���� �غ�
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

	if (pPlayer->ReqRoomInfo)	// �� ���� ���и� ����
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
		// ��ġ����ŷ ������ �α��� ��û
	case en_PACKET_CS_MATCH_REQ_LOGIN:
		ReqLogin(OutSessionID, pOutBuffer);
		break;
		// �� ���� ��û
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

	// 1. URL�� http://�� �־�� �ϴ� ������. 
	WCHAR *szPointer = shAPIURL;

	if (wcsncmp(szPointer, L"http://", 7) != 0)
	{
		SYSLOG(L"HTTP", LOG_ERROR, L"Http Header isn't Exist");
		CCrashDump::Crash();
	}
	szPointer += 7;

	// 2. : �� ���ų� /�� �ö����� �ѱ��.
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
	
	// 3. httpURL�� �ִ´�.
	WCHAR httpDomain[100];
	int Len = szPointer - szStartPointer;
	memcpy(httpDomain, szStartPointer, Len * 2);
	httpURL[Len] = L'\0';

	DomainToIP(httpDomain, httpURL);
	

	// 4. szPointer�� : �̶�� ������ ��Ʈ�� �Է�, �ƴ϶�� Path�� ����.
	if (*szPointer == L':')
	{
		// ��Ʈ �Է��Ѵ�.
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

	// Path �Է�
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
	//	��ġ����ŷ ������ Ŭ���̾�Ʈ ���ӽ� SessionKey �� shDB ���� Ȯ���Ͽ� �α��� ���� ��.
	//	���� �� Ŭ���̾�Ʈ ���� Ű (Client Key) �� �����Ͽ� �����Ѵ�. (�� ��ġ����ŷ ���� ����� ����ũ �� �̾�� ��)
	//
	//	Client Key �� �뵵�� ������ �������� Ŭ���̾�Ʈ�� �����ϴ� ������ 
	//	��ġ����ŷ ������ ������ �������� �� ���� ��û, ����Ȯ�� �� ��� ��.
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

	// shDB���� Ȯ���Ѵ�.
	WCHAR PathURL[200];
	StringCchPrintf(PathURL, 200, L"%s%s", httpPath, L"select_account.php");

	// AccountNo�� JSON���� ��� ���ߵȴ�.
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
		// ���� �α� & ��Ÿ ����
		Result = 4;
		ResLogin(OutSessionID, Result);
		SYSLOG(L"LOGIN", LOG_ERROR, L"Request Login, HTTP POST ERROR. Result Code : %d", ret);

		//CCrashDump::Crash();
		return;
	}

	// RecvDATA�� RapidJSON�� �̿��ؼ� ���� ���´�.
	Document document;
	document.Parse(RecvDATA);
	
	ResultCode = document["result"].GetInt();
	if (ResultCode == -10)
	{
		// AccountNo ����
		Result = 3;
		ResLogin(OutSessionID, Result);
		return;
	}

	if (ResultCode != 1)
	{
		// ��Ÿ ����
		Result = 4;
		ResLogin(OutSessionID, Result);
		SYSLOG(L"LOGIN", LOG_ERROR, L"SHDB LogDB ETC Error");
		SYSLOG(L"LOGIN", LOG_ERROR, L"%s", RecvDATA);

		return;
	}

	if (memcmp(SessionKey, document["sessionkey"].GetString(), 64) != 0)
	{
		// ����Ű ����
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
			//	��ġ����ŷ ������ ������ �������� ���ӹ� ������ ��û
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
	//	Ŭ���̾�Ʈ�� ��ġ����ŷ �������� �� ������ ��û ��.
	//
	//	��ġ����ŷ ������ ������ �������� ClientKey �� ������ ��û�� ���� ��
	//	������ �������Լ� ���� ������ Ŭ���̾�Ʈ���� �����ָ� �ȴ�.

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
	// ������ �������� ���ӹ� ������ ��û�Ѵ�.
	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();
	//	��ġ����ŷ ������ ������ �������� ���ӹ� ������ ��û
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
	// �� ���� ���� �˸�
	//	Ŭ���̾�Ʈ�� en_PACKET_CS_MATCH_RES_GAME_ROOM �� ������ ������ ��Ʋ������ ������ �õ��Ѵ�.
	//
	//	Ŭ���̾�Ʈ�� ��Ʋ������ ���� �� ���� ���� �ȳ��� �ޱ� �� ������
	//	��ġ����ŷ ������ ��Ʋ���� 2���� ������ ��� ������ �����ϰ� ����
	//
	//	��Ʋ���� �� ������ �����Ǹ�, ��ġ����ŷ �������� �� ��Ŷ�� ������ �� ���� ������ �˷���.
	//
	//	��ġ����ŷ ������ �� ��Ŷ�� ������ ������ �������� ���� ���ָ�, ���� �ش� ���� ������ �ο��� ���� �Ѵ�.


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
	// �� ���� �������θ� �����Ϳ��� �����Ѵ�.
	ReqGameRoomEnterSuccess(pPlayer, BattleServerNo, RoomNo);
	///////////////////////////////////////////////////////////////////////////////////


	pPlayer->LastRecv = CUpdateTime::GetTickCount();

	// Ŭ���̾�Ʈ���� ����Ȯ���� �����ش�.
	ResGameRoomEnter(pPlayer);
}

void CMatchServer::ResGameRoomEnter(Player *pPlayer)
{
	//	��ġ����ŷ ������ ���� �� ���� ���� ��Ŷ�� ������ ������ �������� �̸� ���� �� �ڿ�
	//	������ �����κ��� ȸ���� ���� Ŭ���̾�Ʈ���� ����� �����ش�.
	//
	//	�� ��Ŷ�� ������ Ŭ���̾�Ʈ�� ��ġ����ŷ �������� ������ ���� �ȴ�.
	//	���� RES Ȯ�� ��Ŷ�� �ִ� ������, Ŭ���̾�Ʈ�� �Ϲ������� ������ ���� ������ 
	//	��ġ����ŷ �������� 100% ���� �Ǿ��ٴ� Ȯ���� ����� �� �����Ƿ� REQ �� RES �� �ξ ����.

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
		// �� ���� ��� ����
		ResGameRoom(pPlayer);
		break;
	case 1:
		// �� ���� ��� ����
		ResGameRoom(pPlayer, 1, pBuffer);
		break;
	}
}


////////////////////////////////////////////////////////////////////////////////////////////
// �÷��̾� Ÿ�Ӿƿ� üũ
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
// ������ �������� ���� ��� 
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
	// ���� ���� List
	// 2. CPU ���� (Kernel + User)
	time_t   current_time;
	int TimeStamp = time(&current_time);

	// 4. ��ŶǮ ��뷮
	pMornitorSender->Send(dfMONITOR_DATA_TYPE_MATCH_PACKET_POOL, CPacketBuffer::PacketPool.GetAllocCount(), TimeStamp);

	pMornitorSender->Send(dfMONITOR_DATA_TYPE_MATCH_SERVER_ON, 1, TimeStamp);

	int CpuUsage = pMonitorAgent->GetCpuUsage();
	pMornitorSender->Send(dfMONITOR_DATA_TYPE_MATCH_CPU, CpuUsage, TimeStamp);


	// 3. ��ġ����ŷ �޸� ���� Ŀ�� ��뷮

	int MemoryUsage = pMonitorAgent->GetMemoryUsage();
	pMornitorSender->Send(dfMONITOR_DATA_TYPE_MATCH_MEMORY_COMMIT, MemoryUsage, TimeStamp);



	// 5. ���� ����

	pMornitorSender->Send(dfMONITOR_DATA_TYPE_MATCH_SESSION, m_NowSession, TimeStamp);

	// 6. ���� ����

	pMornitorSender->Send(dfMONITOR_DATA_TYPE_MATCH_PLAYER, PlayerMap.size(), TimeStamp);


	// 7. �ʴ� ���� ���� ��

	pMornitorSender->Send(dfMONITOR_DATA_TYPE_MATCH_MATCHSUCCESS, RoomEnterSuccessTPS, TimeStamp);

}