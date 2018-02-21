#include "BattleLanServer.h"
#include "MasterServer.h"
#include "CommonProtocol.h"


BattleLanServer::BattleLanServer()
{
	PacketError = 0;
	Disconnected = 0;
}


BattleLanServer::~BattleLanServer()
{
}


bool BattleLanServer::Start(WCHAR* szServerIP, int Port, int WorkerThread, int MaxSession, bool NagleOption,  char *pMasterToken)
{
	if (!CLanServer::Start(szServerIP, Port, WorkerThread, NagleOption, MaxSession))
		return false;

	memcpy(MasterToken, pMasterToken, 32);

	return true;
}

void BattleLanServer::Stop()
{

}

void BattleLanServer::SyncBattleInfo(CBattleInfo *pArrInfo[])
{
	for (int i = 0; i < MAX_BATTLESERVER; i++)
	{
		pArrInfo[i] = &BattleServerArr[i];
	}
}

void BattleLanServer::OnClientJoin(ULONG64 OutSessionID, WCHAR *pClientIP, int Port)
{
	
}

void BattleLanServer::OnClientLeave(ULONG64 SessionID)
{
	ServerDisconnect(SessionID);
}

bool BattleLanServer::OnConnectionRequest(WCHAR *szIP, int Port)
{
	return true;
}
void BattleLanServer::Monitoring()
{
	wcout << L"========== BattleServer =============\n";
	wcout << L"Online Server : " << BattleServerNum << endl;
	wcout << L"Open Room Total :" << ActivateGameRoomTotal << endl;
	wcout << L"PacketError :" << PacketError << endl;
	wcout << L"Disconnect Dectected :" << Disconnected << endl;
	wcout << L"Recv PacketTPS :" << m_RecvPacketTPS << endl;
	wcout << L"Send PacketTPS :" << m_SendPacketTPS << endl;
	wcout << L"Packet Pool : " << CPacketBuffer::PacketPool.GetChunkSize() << L"\n";
	wcout << L"Packet Pool Use: " << CPacketBuffer::PacketPool.GetAllocCount() << L"\n\n";
	m_RecvPacketTPS = m_SendPacketTPS = 0;
}

void BattleLanServer::OnRecv(ULONG64 OutSessionID, CPacketBuffer *pOutBuffer)
{
	WORD Type;

	*pOutBuffer >> Type;

	switch (Type)
	{
	case en_PACKET_BAT_MAS_REQ_SERVER_ON:
		ReqServerOn(OutSessionID, pOutBuffer);
		//	��Ʋ������ ������ �������� ���� ���� �˸�
		break;
	case en_PACKET_BAT_MAS_REQ_CONNECT_TOKEN:
		//	��Ʋ������ ������ū ����� �˸�.
		ReqConnectToken(OutSessionID, pOutBuffer);
		break;
	case en_PACKET_BAT_MAS_REQ_CREATED_ROOM:
		//	��Ʋ ������ �ű� ��� �� ���� �˸�
		ReqCreatedRoom(OutSessionID, pOutBuffer);
		break;
	case en_PACKET_BAT_MAS_REQ_CLOSED_ROOM:
		//	�� ���� �˸�
		ReqClosedRoom(OutSessionID, pOutBuffer);
		break;
	case en_PACKET_BAT_MAS_REQ_LEFT_USER:
		//	�濡�� ������ ������, 1�� ������ ���� ����.
		ReqLeftONEUser(OutSessionID, pOutBuffer);
		break;
	default:
		PacketError++;
		break;
	}

	pOutBuffer->Free();
}

void BattleLanServer::OnSend(ULONG64 OutSessionID, int SendSize)
{

}

void BattleLanServer::OnError(int errorcode, WCHAR *szMessage)
{

}

void BattleLanServer::OnWorkerThreadBegin()
{

}
void BattleLanServer::OnWorkerThreadEnd()
{

}

bool BattleLanServer::ServerDisconnect(ULONG64 SessionID)
{
	for (int i = 0; i < MAX_BATTLESERVER; i++)
	{
		if (BattleServerArr[i].OutSessionID == SessionID)
		{
			if (BattleServerArr[i].isUse)
			{
				Disconnected++;
				BattleServerArr[i].isUse = false;
				_InterlockedAdd64(&BattleServerNum, -1);

				for (int j = 0; j < MAX_BATTLEROOM; j++)
				{
					CRoom *pRoom = &BattleServerArr[i].Rooms[j];

					if (pRoom->isUse)
					{
						pRoom->isUse = false;
						ActivateGameRoomTotal--;
					}
					
				}
			}
			return true;
		}
	}

	return false;
}

void BattleLanServer::ReqServerOn(ULONG64 SessionID, CPacketBuffer *pBuffer)
{
	int BattleIndex = ServerOn(SessionID, pBuffer);

	if (BattleIndex == -1)
	{
		SYSLOG(L"BATTLE", LOG_SYSTEM, L"Master Token isn't Matched");
		Disconnect(SessionID);
		CCrashDump::Crash();
		return;
	}

	ResServerOn(SessionID, BattleIndex);
}

int BattleLanServer::ServerOn(ULONG64 BattleSessionID, CPacketBuffer *pOutBuffer)
{
	//	��Ʋ������ ������ �������� ���� ���� �˸�
	// ���� �ʿ��ϴ�.
	char _MasterToken[32];
	BattleServerIndex = InterlockedAdd(&BattleServerIndex, 1) % MAX_BATTLESERVER;

	CBattleInfo *pServer = &BattleServerArr[BattleServerIndex];

	pServer->BattleServerNo = BattleServerIndex;
	pServer->OutSessionID = BattleSessionID;
	pOutBuffer->GetData(reinterpret_cast<char *>(pServer->ServerIP), 32);
	*pOutBuffer >> pServer->Port;

	pOutBuffer->GetData(reinterpret_cast<char *>(pServer->ConnectToken), 32);
	pOutBuffer->GetData(reinterpret_cast<char *>(_MasterToken), 32);

	pOutBuffer->GetData(reinterpret_cast<char *>(pServer->ChatServerIP), 32);
	*pOutBuffer >> pServer->ChatSeverPort;

	if (memcmp(_MasterToken, MasterToken, 32) != 0)
		return -1; // ��ū�� ���� ����.

	pServer->isUse = true;
	_InterlockedAdd64(&BattleServerNum, 1);

	return pServer->BattleServerNo;
}

void BattleLanServer::ResServerOn(ULONG64 SessionID, int BattleIndex)
{
	WORD Type = en_PACKET_BAT_MAS_RES_SERVER_ON;

	CPacketBuffer *pSendBuffer = CPacketBuffer::Alloc();

	*pSendBuffer << Type << BattleIndex;
	SendPacket(SessionID, pSendBuffer);
}

void BattleLanServer::ReqConnectToken(ULONG64 SessionID, CPacketBuffer *pBuffer)
{
	char connectToken[32];
	UINT ReqSeqence;

	pBuffer->GetData(connectToken, 32);
	*pBuffer >> ReqSeqence;

	ConnectToken(SessionID, connectToken);

	ResConnectToken(SessionID, ReqSeqence);
}

int BattleLanServer::ConnectToken(ULONG64 SessionID, char *pToken)
{
	//	��Ʋ������ ������ū ����� �˸�.
	for (int i = 0; i < MAX_BATTLESERVER; i++)
	{
		if (!BattleServerArr[i].isUse)
			continue;

		if (BattleServerArr[i].OutSessionID == SessionID)
		{
			memcpy(BattleServerArr[i].ConnectToken, pToken, 32);
			return true;
		}
	}

	return -1;
}

void BattleLanServer::ResConnectToken(ULONG64 SessionID, UINT ReqSeqence)
{
	WORD Type = en_PACKET_BAT_MAS_RES_CONNECT_TOKEN;

	CPacketBuffer *pSendBuffer = CPacketBuffer::Alloc();

	*pSendBuffer << Type << ReqSeqence;
	SendPacket(SessionID, pSendBuffer);
}

void BattleLanServer::ReqCreatedRoom(ULONG64 SessionID, CPacketBuffer *pBuffer)
{
	//	��Ʋ������ �ڽ��� ������ ������ ������ ���� �˾Ƽ� 1�� �̻��� ���� �����Ͽ� ������ �������� ���� �Ѵ�
	//	���� ���� �� �̻� ���� �� ���� ���¶�� ��Ʋ ������ �˾Ƽ� ������� �����Ѵ�. 
	//	�� ���� ������ ������ ���ڰ� �����Ͽ� ������ �������� ���� �Ѵ�.
	//
	//	������ ������ �ϳ��ϳ� �� ������ ��û���� �ʰ�,  ��Ʋ������ 1�� �̻��� ������ ���� �˷��ִ� ���.
	//
	//	������ ������ �� ��Ʋ������ IP/PORT �� �˰� �����Ƿ� �濡 ���� ������ ���� �Ѵ�.

	int BattleServerNo;
	int RoomNo;
	int MaxUser;
	char EnterToken[32];

	UINT ReqSeqence;

	*pBuffer >> BattleServerNo >> RoomNo >> MaxUser;
	pBuffer->GetData(EnterToken, 32);

	*pBuffer >> ReqSeqence;
	CreateRoom(SessionID, BattleServerNo, RoomNo, MaxUser, EnterToken);
	ResCreateRoom(SessionID, RoomNo, ReqSeqence);
}

int BattleLanServer::CreateRoom(ULONG64 SessionID, int ServerNo, int RoomNo, int MaxUser, char *pToken)
{
	// �̿� �ִ±迡 �Ѵ� ����~
	if (BattleServerArr[ServerNo].OutSessionID == SessionID)
	{
		CRoom *pRoom = &BattleServerArr[ServerNo].Rooms[RoomNo];
		
		if (!pRoom->isUse)
		{
			_InterlockedAdd64(&ActivateGameRoomTotal, 1);

			pRoom->BattleServerNo = ServerNo;
			pRoom->MaxUser = MaxUser;
			pRoom->RoomNo = RoomNo;
			memcpy(pRoom->EnterToken, pToken, 32);
			pRoom->isUse = true;

			return true;
		}
	}

	return -1;
}

void BattleLanServer::ResCreateRoom(ULONG64 SessionID, int RoomNo, UINT ReqSeqence)
{
	WORD Type = en_PACKET_BAT_MAS_RES_CREATED_ROOM;
	CPacketBuffer *pSendBuffer = CPacketBuffer::Alloc();

	*pSendBuffer << Type << RoomNo << ReqSeqence;
	SendPacket(SessionID, pSendBuffer);
}

void BattleLanServer::ReqClosedRoom(ULONG64 SessionID, CPacketBuffer *pBuffer)
{
	//	������ �÷��̸� ����, �Ǵ� ��Ÿ ������ ���ؼ� ���� �������� �����Ϳ��� �˸�
	//	������ ������ �ش� ���� ������ �����ϰ� ����ڸ� �������� �ʴ´�.
	//
	//	���� -> �÷���		�� ��Ŷ �۽�
	//	�÷��� -> ����		�̹� �÷��̽� �������Ƿ� �۽� ���� ����.

	int RoomNo;
	UINT ReqSequence;

	*pBuffer >> RoomNo >> ReqSequence;

	CloseRoom(SessionID, RoomNo);
	ResCloseRoom(SessionID, RoomNo, ReqSequence);
}

int BattleLanServer::CloseRoom(ULONG64 SessionID, int RoomNo)
{
	for (int i = 0; i < MAX_BATTLESERVER; i++)
	{
		if (!BattleServerArr[i].isUse)
			continue;

		if (BattleServerArr[i].OutSessionID == SessionID)
		{
			CRoom *pRoom = &BattleServerArr[i].Rooms[RoomNo];
			pRoom->isUse = false;
			_InterlockedAdd64(&ActivateGameRoomTotal, -1);
			return true;
		}
	}

	return -1;
}

void BattleLanServer::ResCloseRoom(ULONG64 SessionID, int RoomNo, UINT ReqSeqence)
{
	WORD Type = en_PACKET_BAT_MAS_RES_CLOSED_ROOM;
	CPacketBuffer *pSendBuffer = CPacketBuffer::Alloc();

	*pSendBuffer << Type << RoomNo << ReqSeqence;
	SendPacket(SessionID, pSendBuffer);
}
void BattleLanServer::ReqLeftONEUser(ULONG64 SessionID, CPacketBuffer *pBuffer)
{
	//	����ڰ� ���� �κ��� ������ ������ �˾Ƽ� ī���� ������, 
	//	��Ʋ������ ���ٰ� ���� ������ ������ ������ �ľ� �Ұ���.
	//
	//	�׷��Ƿ� ������ ���� ��쿡�� ��Ʋ���� -> �����ͼ��� �� ���� �Ѵ�.
	//
	//	���� �ο��� ��ġ�� ���� �ϴ°��� �ƴ� -1 ���� ������ �ǹ��̹Ƿ� �����ͼ����� 
	//	�ش� �� �������� �ο��� +1 �����ָ� ��.

	int RoomNo;
	UINT ReqSequence;

	*pBuffer >> RoomNo >> ReqSequence;

	LeftOneUser(SessionID, RoomNo);
	ResLeftOneUser(SessionID, RoomNo, ReqSequence);
}

int BattleLanServer::LeftOneUser(ULONG64 SessionID, int RoomNo)
{
	for (int i = 0; i < MAX_BATTLESERVER; i++)
	{
		if (!BattleServerArr[i].isUse)
			continue;

		if (BattleServerArr[i].OutSessionID == SessionID)
		{
			CRoom *pRoom = &BattleServerArr[i].Rooms[RoomNo];
			InterlockedAdd(&pRoom->MaxUser, 1);
			//	���� �ο��� ��ġ�� ���� �ϴ°��� �ƴ� -1 ���� ������ �ǹ��̹Ƿ� �����ͼ����� 
			//	�ش� �� �������� �ο��� +1 �����ָ� ��.
			return true;
		}
	}

	return -1;
}

void BattleLanServer::ResLeftOneUser(ULONG64 SessionID, int RoomNo, UINT ReqSequence)
{
	WORD Type = en_PACKET_BAT_MAS_RES_LEFT_USER;
	CPacketBuffer *pSendBuffer = CPacketBuffer::Alloc();

	*pSendBuffer << Type << RoomNo << ReqSequence;
	SendPacket(SessionID, pSendBuffer);
}