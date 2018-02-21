#include "MasterLanClient.h"
#include "BattleServer.h"

CMasterLanClient::CMasterLanClient(CBattleServer *pBattleServer)
	:pBattleServer(pBattleServer)
{
}


CMasterLanClient::~CMasterLanClient()
{
}


bool CMasterLanClient::Connect(WCHAR *szOpenIP, int port, bool nagleOption)
{
	lstrcpyW(OpenIP, szOpenIP);
	Port = port;
	NagleOption = nagleOption;

	while (!CLanClient::Connect(OpenIP, Port, NagleOption))
	{
		wprintf(L"Disconnected MasterServer ReConnect Try\n");
	}

	return true;
}

void CMasterLanClient::Stop()
{
	CLanClient::Stop();
}

void CMasterLanClient::OnConnected()
{
	pBattleServer->MasterServerReqServerON();
}
void CMasterLanClient::OnDisconnect()
{
	pBattleServer->MasterServerDisconnect();

	while (!CLanClient::Connect(OpenIP, Port, NagleOption))
	{
		wprintf(L"Disconnected MasterServer ReConnect Try\n");
	}
}
void CMasterLanClient::MasterSendPacket(CPacketBuffer *pBuffer)
{
	if (!SendPacket(pBuffer))
		pBuffer->Free();
}

void CMasterLanClient::OnRecv(CPacketBuffer *pOutBuffer)
{
	WORD Type;
	*pOutBuffer >> Type;

	switch (Type)
	{
	case en_PACKET_BAT_MAS_RES_SERVER_ON:
		//pBattleServer->MastserServerResServerOn();
		ResServerOn(pOutBuffer);
		break;
	case en_PACKET_BAT_MAS_RES_CONNECT_TOKEN:
		//pBattleServer->MasterServerResConnectToken();
		ResConnectToken(pOutBuffer);
		break;
	case en_PACKET_BAT_MAS_RES_CREATED_ROOM:
		//pBattleServer->MasterServerResCreateRoom();
		ResCreatedRoom(pOutBuffer);
		break;
	case en_PACKET_BAT_MAS_RES_CLOSED_ROOM:
		//pBattleServer->MasterServerResClosedRoom();
		ResClosedRoom(pOutBuffer);
		break;
	case en_PACKET_BAT_MAS_RES_LEFT_USER:
		//pBattleServer->MasterServerResLeftUser();
		ResLeftUser(pOutBuffer);
		break;
	}

	pOutBuffer->Free();
}
void CMasterLanClient::OnSend(int SendSize)
{
}


void CMasterLanClient::OnError(int errorcode, WCHAR *errMsg)
{
}

void CMasterLanClient::ResServerOn(CPacketBuffer *pOutBuffer)
{
	int BattleServerNo;

	*pOutBuffer >> BattleServerNo;

	pBattleServer->MastserServerResServerOn(BattleServerNo);
}
void CMasterLanClient::ResConnectToken(CPacketBuffer *pOutBuffer)
{
	UINT ReqSequence;
	*pOutBuffer >> ReqSequence;

	pBattleServer->MasterServerResConnectToken(ReqSequence);
}
void CMasterLanClient::ResCreatedRoom(CPacketBuffer *pOutBuffer)
{
	int RoomNo;
	UINT	ReqSequence;

	*pOutBuffer >> RoomNo >> ReqSequence;
	pBattleServer->MasterServerResCreateRoom(RoomNo, ReqSequence);

}
void CMasterLanClient::ResClosedRoom(CPacketBuffer *pOutBuffer)
{
	int RoomNo;
	UINT	ReqSequence;

	*pOutBuffer >> RoomNo >> ReqSequence;
	pBattleServer->MasterServerResClosedRoom(RoomNo, ReqSequence);
}
void CMasterLanClient::ResLeftUser(CPacketBuffer *pOutBuffer)
{
	int RoomNo;
	UINT	ReqSequence;

	*pOutBuffer >> RoomNo >> ReqSequence;
	pBattleServer->MasterServerResLeftUser(RoomNo, ReqSequence);
}

void CMasterLanClient::Monitoring()
{
	wcout << L"MasterServer: Recv PacketTPS :" << m_RecvPacketTPS << endl;
	wcout << L"MasterServer: Send PacketTPS :" << m_SendPacketTPS << endl;

	m_RecvPacketTPS = m_SendPacketTPS = 0;
}