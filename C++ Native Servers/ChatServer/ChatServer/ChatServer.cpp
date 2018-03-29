#include "ChatServer.h"
#include "Engine/Parser.h"
#include "LanBattle.h"
#include "MornitoringAgent.h"
#include "Morntoring.h"
using namespace std;


CChatServer::CChatServer()
{
	CPacketBuffer::AllocCount = 0;
	m_WorkingRoom = 0;
}

CChatServer::~CChatServer()
{

}


bool CChatServer::Start(WCHAR *szServerConfig, bool nagleOption)
{
	CParser Parser(szServerConfig);

	WCHAR szOpenIP[16];
	int port;
	int workerCount;
	int MaxSession;
	int PacketCode, PacketKey1, PacketKey2;

	Parser.GetValue(L"BIND_IP", szOpenIP, L"NETWORK");
	Parser.GetValue(L"BIND_PORT", &port, L"NETWORK");
	Parser.GetValue(L"WORKER_THREAD", &workerCount, L"NETWORK");

	Parser.GetValue(L"CLIENT_MAX", &MaxSession, L"SYSTEM");

	Parser.GetValue(L"PACKET_CODE", &PacketCode, L"SYSTEM");
	Parser.GetValue(L"PACKET_KEY1", &PacketKey1, L"SYSTEM");
	Parser.GetValue(L"PACKET_KEY2", &PacketKey2, L"SYSTEM");

	if (!CNetServer::Start(L"0.0.0.0", port, workerCount, nagleOption, MaxSession, PacketCode, PacketKey1,
		PacketKey2))
		return false;

	WCHAR szBattleOpenIP[16];
	int szBattlePort;

	Parser.GetValue(L"BATTLE_SERVER_IP", szBattleOpenIP, L"NETWORK");
	Parser.GetValue(L"BATTLE_SERVER_PORT", &szBattlePort, L"NETWORK");

	pBattleServer = new CLanBattle(this, szOpenIP, port);

	if (!pBattleServer->Connect(szBattleOpenIP, szBattlePort, false))
		return false;

	hUpdateThread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, UpdateThread, this, 0, nullptr));
	b_ExitFlag = false;
	StartTime = CUpdateTime::GetSystemTime();
	hUpdateEvent = CreateEvent(nullptr, false, false, nullptr);

	int ServerNo;
	array<BYTE, 32> LoginSessionKey;
	WCHAR _LoginKey[32];

	Parser.GetValue(L"SERVER_NO", &ServerNo, L"NETWORK");

	Parser.GetValue(L"LOGIN_TOKEN", _LoginKey, L"SYSTEM");
	UTF16toUTF8(reinterpret_cast<char *>(LoginSessionKey.data()), _LoginKey, 32);

	array<WCHAR, 16> MornitorIP;
	int MorintorPort;

	Parser.GetValue(L"MORNITOR_IP", &MornitorIP[0], L"NETWORK");
	Parser.GetValue(L"MORNITOR_PORT", &MorintorPort, L"NETWORK");

	MonitorAgent = make_shared<CMornitoringAgent>(*this, ServerNo, LoginSessionKey);
	if (!MonitorAgent->Connect(MornitorIP, MorintorPort, true))
		return false;

	Monitor = make_shared<CMornitoring>();

	return true;
}

void CChatServer::Stop()
{
	CNetServer::Stop();

	b_ExitFlag = true;
	WaitForSingleObject(hUpdateThread, INFINITE);


}
//================================================================================================
void CChatServer::OnClientJoin(ULONG64 OutSessionID, WCHAR *pClientIP, int Port)
{

}

void CChatServer::OnClientLeave(ULONG64 SessionID)
{
	Queue_DATA *pNewData = UpdatePool.Alloc();

	pNewData->Type = enDISSession;
	pNewData->SessionID = SessionID;
	UpdateQueue.Enqueue(&pNewData);
	SetEvent(hUpdateEvent);
}

bool CChatServer::OnConnectionRequest(WCHAR *szIP, int Port)
{
	return true;
}

void CChatServer::OnRecv(ULONG64 OutSessionID, CPacketBuffer *pOutBuffer)
{
	Queue_DATA *pNewData = UpdatePool.Alloc();


	pNewData->Type = enMSGSession;
	pNewData->pBuffer = pOutBuffer;
	pNewData->SessionID = OutSessionID;

	pOutBuffer->AddRefCount();
	UpdateQueue.Enqueue(&pNewData);
	pOutBuffer->Free();

	SetEvent(hUpdateEvent);

}

void CChatServer::OnSend(ULONG64 OutSessionID, int SendSize)
{
}

void CChatServer::OnError(int errorcode, WCHAR *szMessage)
{
	SYSLOG(L"ERROR", LOG_ERROR, szMessage);
}

void CChatServer::OnWorkerThreadBegin()
{

}
void CChatServer::OnWorkerThreadEnd()
{

}
//================================================================================================

UINT WINAPI CChatServer::UpdateThread(LPVOID arg)
{
	CChatServer *pServer = reinterpret_cast<CChatServer *>(arg);

	return pServer->UpdateWork();
}


bool CChatServer::UpdateWork()
{
	Queue_DATA* Data = nullptr;
	while (!b_ExitFlag)
	{
		WaitForSingleObject(hUpdateEvent, INFINITE);
		while (UpdateQueue.Dequeue(&Data))
		{
			switch (Data->Type)
			{
			case enDISSession:
				DisconnectSession(Data->SessionID);
				break;
			case enMSGSession:
				MsgProc(Data->SessionID, Data->pBuffer);
				break;
			}

			UpdatePool.Free(Data);
			Data = nullptr;
			m_UpdateTPS++;
		}
	}
	return true;
}



void CChatServer::MsgProc(ULONG64 SessionID, CPacketBuffer *pBuffer)
{

	WORD Type;
	*pBuffer >> Type;

	switch (Type)
	{
	case en_PACKET_CS_CHAT_REQ_LOGIN:
		PRO_BEGIN(L"ReqLogin");
		ReqLogin(SessionID, pBuffer);
		PRO_END(L"ReqLogin");
		break;
	case en_PACKET_CS_CHAT_REQ_ENTER_ROOM:
		PRO_BEGIN(L"ReqSectorMove");
		ReqEnterRoom(SessionID, pBuffer);
		PRO_END(L"ReqSectorMove");
		break;
	case en_PACKET_CS_CHAT_REQ_MESSAGE:
		PRO_BEGIN(L"ReqChatMessage");
		ReqChatMessage(SessionID, pBuffer);
		PRO_END(L"ReqChatMessage");
		break;
	case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
		PRO_BEGIN(L"ReqHeartBeat");
		ReqHeartBeat(SessionID);
		PRO_END(L"ReqHeartBeat");
		break;
	default:
		SYSLOG(L"ERROR", LOG_ERROR, L"Packet Type Error : %d", Type);
		Disconnect(SessionID);
		break;
	}

	pBuffer->Free();


}


void CChatServer::JoinSession(ULONG64 SessionID)
{

}

void CChatServer::ReqLogin(ULONG64 SessionID, CPacketBuffer *pBuffer)
{
	if (SessionID == 0)
		return;

	CPlayer *pPlayer = PlayerPool.Alloc();

	pPlayer->SessionID = SessionID;
	pPlayer->LastRecvTick = CUpdateTime::GetTickCount();
	pPlayer->Logined = false;
	pPlayer->EnterRoom = false;

	pair <ULONG64, CPlayer *> Sessionpair = { SessionID, pPlayer };
	auto result = PlayerMap.insert(Sessionpair);

	if (result.second == false)
	{
		PlayerPool.Free(pPlayer);
		Disconnect(SessionID);
		SYSLOG(L"DISCONNECT", LOG_ERROR, L"ReqLogin JoinSession Already SessionID Exist");
		return;
	}

	__int64 AccountNo;

	char _ConnectToken[32];

	*pBuffer >> AccountNo;
	pBuffer->GetData((char *)pPlayer->ID, 40);
	pBuffer->GetData((char *)pPlayer->NickName, 40);
	pBuffer->GetData((char *)_ConnectToken, 32);
	pPlayer->AccountNo = AccountNo;
	

	if (memcmp(ConnectToken, _ConnectToken, 32) != 0)
	{
		ResLogin(pPlayer, false);
		return;
	}

	ResLogin(pPlayer, true);

	pPlayer->Logined = true;
} 

void CChatServer::ResLogin(CPlayer *pPlayer, BYTE bResult)
{
	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();
	WORD Type = en_PACKET_CS_CHAT_RES_LOGIN;


	*pBuffer << Type << bResult << pPlayer->AccountNo;

	if (!SendPacket(pPlayer->SessionID, pBuffer))
		pBuffer->Free();
}

void CChatServer::ReqEnterRoom(ULONG64 SessionID, CPacketBuffer *pBuffer)
{
	__int64 AccountNo;
	*pBuffer >> AccountNo;

	auto iter = PlayerMap.find(SessionID);

	if (iter == PlayerMap.end())
	{
		SYSLOG(L"DISCONNECT", LOG_ERROR, L"ReqEnterRoom SessionID %I64d isn't Exist in PlayerMap", SessionID);
		Disconnect(SessionID);
		return;

	}

	CPlayer *pPlayer = iter->second;

	if (pPlayer->SessionID != SessionID)
	{
		SYSLOG(L"DISCONNECT", LOG_ERROR, L"ReqEnterRoom SessionID %I64d isn't Matched", SessionID);

		if (!Disconnect(SessionID))
			DisconnectSession(pPlayer->SessionID);

		return;
	}

	char EnterToken[32];

	*pBuffer >> pPlayer->RoomNo;
	pBuffer->GetData(EnterToken, 32);

	if (memcmp(RoomArray[pPlayer->RoomNo].EnterToken, EnterToken, 32) != 0)
	{
		ResEnterRoom(pPlayer, 2);
		return;
	}

	if (!RoomArray[pPlayer->RoomNo].isUse)
	{
		ResEnterRoom(pPlayer, 3);
		return;
	}

	RoomArray[pPlayer->RoomNo].Lock();
	RoomArray[pPlayer->RoomNo].RoomPlayer.push_back(pPlayer);
	RoomArray[pPlayer->RoomNo].UnLock();

	pPlayer->EnterRoom = true;
	ResEnterRoom(pPlayer, 1);
}

void CChatServer::ResEnterRoom(CPlayer *pPlayer, BYTE bResult)
{
	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();

	WORD Type = en_PACKET_CS_CHAT_RES_ENTER_ROOM;

	*pBuffer << Type << pPlayer->AccountNo << pPlayer->RoomNo << bResult;

	if (!SendPacket(pPlayer->SessionID, pBuffer))
		pBuffer->Free();
}



void CChatServer::ReqChatMessage(ULONG64 SessionID, CPacketBuffer *pBuffer)
{
	__int64 AccountNo;
	*pBuffer >> AccountNo;

	auto iter = PlayerMap.find(SessionID);

	if (iter == PlayerMap.end())
	{
		SYSLOG(L"DISCONNECT", LOG_ERROR, L"ReqChatMessage SessionID %I64d isn't Exist in PlayerMap", SessionID);
		Disconnect(SessionID);
		return;
	}

	CPlayer *pPlayer = iter->second;

	if (pPlayer->SessionID != SessionID)
	{
		SYSLOG(L"DISCONNECT", LOG_ERROR, L"ReqChatMessage SessionID %I64d isn't Matched", SessionID);

		if (!Disconnect(SessionID))
			DisconnectSession(pPlayer->SessionID);

		return;
	}


	WORD MessageLen;
	WCHAR* MessageStr;

	*pBuffer >> MessageLen;
	MessageStr = new WCHAR[MessageLen / 2];
	pBuffer->GetData((char *)MessageStr, MessageLen);

	ResChatMessage(pPlayer, MessageLen, MessageStr);

	delete[] MessageStr;

}

void CChatServer::ResChatMessage(CPlayer *pPlayer, WORD &MessageLen, WCHAR* pMessageStr)
{
	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();
	WORD Type = en_PACKET_CS_CHAT_RES_MESSAGE;

	*pBuffer << Type;
	*pBuffer << pPlayer->AccountNo;
	pBuffer->PutData(reinterpret_cast<char *>(pPlayer->ID), 40);
	pBuffer->PutData(reinterpret_cast<char *>(pPlayer->NickName), 40);

	*pBuffer << MessageLen;
	pBuffer->PutData(reinterpret_cast<char *>(pMessageStr), MessageLen);

	if (pBuffer->GetLastError() != 0)
	{
		pBuffer->Free();
		return;
	}

	list<CPlayer *> *RoomsPlayer = &RoomArray[pPlayer->RoomNo].RoomPlayer;

	RoomArray[pPlayer->RoomNo].Lock();
	auto iter = RoomArray[pPlayer->RoomNo].RoomPlayer.begin();
	while (1)
	{
		if (iter == RoomArray[pPlayer->RoomNo].RoomPlayer.end())
			break;
		
		pBuffer->AddRefCount();
		if (!SendPacket((*iter)->SessionID, pBuffer))
			pBuffer->Free();

		iter++;
	}
	RoomArray[pPlayer->RoomNo].UnLock();


	pBuffer->Free();
}


// 즉각적으로 연결해제가 되지않는게 문제인듯.
void CChatServer::DisconnectSession(ULONG64 SessionID)
{
	auto iter = PlayerMap.find(SessionID);

	if (iter == PlayerMap.end())
		return;

	CPlayer *pPlayer = (*iter).second;

	if (pPlayer->EnterRoom)
	{
		RoomArray[pPlayer->RoomNo].Lock();
		RoomArray[pPlayer->RoomNo].RoomPlayer.remove(pPlayer);
		RoomArray[pPlayer->RoomNo].UnLock();
	}

	if (!pPlayer->Logined && !pPlayer->EnterRoom)
	{
		// Error Disconnect 일 가능성이 큼.
		SYSLOG(L"DISCONNECT", LOG_ERROR, L"Maybe Error Disconnected SessionID : %I64d", SessionID);
	}

	PlayerMap.erase(SessionID);
	PlayerPool.Free(pPlayer);
}


// 성능 감소 요인 3
void CChatServer::ReqHeartBeat(ULONG64 SessionID)
{
	for (auto iter = PlayerMap.begin(); iter != PlayerMap.end(); ++iter)
	{
		CPlayer *pPlayer = iter->second;

		if (pPlayer->SessionID == SessionID)
		{
			pPlayer->LastRecvTick = CUpdateTime::GetTickCount();
			break;
		}
	}
}

void CChatServer::Mornitoring()
{
	wcout << L"===========================================================\n";
	wprintf(L"StartTime : %4d.%2d.%2d. %02d:%02d:%02d \n", StartTime.wYear, StartTime.wMonth, StartTime.wDay, StartTime.wHour,
		StartTime.wMinute, StartTime.wSecond);
	wcout << L"SessionNum : " << m_NowSession << L"\n";
	MornitorSender();
	wcout << L"Packet Pool : " << CPacketBuffer::PacketPool.GetChunkSize() << L"\n";
	wcout << L"Packet Pool Use: " << CPacketBuffer::PacketPool.GetAllocCount() << L"\n\n";


	wcout << L"Update Pool : " << UpdatePool.GetAllocCount() << L"\n";
	wcout << L"Update Queue : " << UpdateQueue.GetUseSize() << L"\n\n";

	wcout << L"Player Pool : " << PlayerPool.GetAllocCount() << L"\n";
	wcout << L"Player Count : " << PlayerMap.size() << L"\n\n";

	wcout << L"Accept Total : " << m_AcceptTotal << L"\n";
	wcout << L"Accpet TPS : " << m_AcceptTPS << L"\n";
	wcout << L"Update TPS : " << m_UpdateTPS << L"\n\n";

	wcout << L"Work Room : " << m_WorkingRoom << L"\n";
	wcout << L"===========================================================\n";

	m_SendPacketTPS = m_RecvPacketTPS = m_AcceptTPS = m_UpdateTPS = 0;

}

//////////////////////////////////////////////////////////////////
// 배틀서버로 부터 온 패킷 처리 
//////////////////////////////////////////////////////////////////
int CChatServer::ReqConnectToken(char* pConnectToken)
{
	memcpy(ConnectToken, pConnectToken, 32);
	
	return true;
}

int CChatServer::ReqCreateRoom(int BattleServerNo, int RoomNo, int MaxUser, char *pEnterToken)
{
	memcpy(RoomArray[RoomNo].EnterToken, pEnterToken, 32);

	if (RoomArray[RoomNo].isUse)
		return 0;

	InterlockedIncrement64(&RoomArray[RoomNo].isUse);

	RoomArray[RoomNo].BattleServerNo = BattleServerNo;
	RoomArray[RoomNo].MaxUser = MaxUser;
	m_WorkingRoom++;

	return true;
}

int CChatServer::ReqDestoryRoom(int BattleServerNo, int RoomNo)
{
	if (RoomArray[RoomNo].isUse == 0)
		return 0;

	InterlockedDecrement64(&RoomArray[RoomNo].isUse);

	if (RoomArray[RoomNo].RoomPlayer.size() != 0)
	{
		RoomArray[RoomNo].Lock();
		auto iter = RoomArray[RoomNo].RoomPlayer.begin();

		while (1)
		{
			if (iter == RoomArray[RoomNo].RoomPlayer.end())
				break;

			if (!(*iter)->EnterRoom)
			{
				CCrashDump::Crash();
			}

			Disconnect((*iter)->SessionID);
			iter = RoomArray[RoomNo].RoomPlayer.erase(iter);
		}
		RoomArray[RoomNo].UnLock();
	}

	m_WorkingRoom--;


	return true;
}

void CChatServer::DisconnectBattleServer()
{
	m_WorkingRoom = 0;

	for (int RoomNo = 0; RoomNo < MAX_BATTLEROOM; RoomNo++)
	{
		RoomArray[RoomNo].isUse = 0;

		RoomArray[RoomNo].Lock();
		auto iter = RoomArray[RoomNo].RoomPlayer.begin();

		while (1)
		{
			if (iter == RoomArray[RoomNo].RoomPlayer.end())
				break;

			Disconnect((*iter)->SessionID);
			iter = RoomArray[RoomNo].RoomPlayer.erase(iter);
		}
		RoomArray[RoomNo].UnLock();
	}
}

void CChatServer::MornitorSender()
{
	time_t   current_time;
	int TimeStamp = time(&current_time);

	MonitorAgent->Send(dfMONITOR_DATA_TYPE_MATCH_PACKET_POOL, CPacketBuffer::PacketPool.GetAllocCount(), TimeStamp);

	MonitorAgent->Send(dfMONITOR_DATA_TYPE_CHAT_SERVER_ON, 1, TimeStamp);

	int CpuUsage = Monitor->GetCpuUsage();
	MonitorAgent->Send(dfMONITOR_DATA_TYPE_CHAT_CPU, CpuUsage, TimeStamp);

	int MemUsage = Monitor->GetMemoryUsage();
	MonitorAgent->Send(dfMONITOR_DATA_TYPE_CHAT_MEMORY_COMMIT, MemUsage, TimeStamp);

	MonitorAgent->Send(dfMONITOR_DATA_TYPE_CHAT_SESSION, m_NowSession, TimeStamp);

	MonitorAgent->Send(dfMONITOR_DATA_TYPE_CHAT_PLAYER, PlayerMap.size(), TimeStamp);

	MonitorAgent->Send(dfMONITOR_DATA_TYPE_CHAT_ROOM, m_WorkingRoom, TimeStamp);


}