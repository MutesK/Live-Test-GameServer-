#include "LanMatchServer.h"
#include "MatchServer.h"


CLanMatchServer::CLanMatchServer(CMatchServer *pServer, WORD _ServerNo, char * MasterToken)
{
	this->pMatchServer = pServer;

	ServerNo = _ServerNo;
	pMasterToken = MasterToken;

	MasterOpen = false;
}


CLanMatchServer::~CLanMatchServer()
{
}


bool CLanMatchServer::Connect(WCHAR *szOpenIP, int port, bool nagleOption)
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

void CLanMatchServer::Stop()
{
	CLanClient::Stop();
}
void CLanMatchServer::OnConnected()
{
	MasterOpen = true;

	// �α��� ��Ŷ ����
	WORD Type = en_PACKET_MAT_MAS_REQ_SERVER_ON;

	CPacketBuffer *pBuffer = CPacketBuffer::Alloc();
	*pBuffer << Type << ServerNo;

	pBuffer->PutData(pMasterToken, 32);

	SendPacket(pBuffer);

	pMatchServer->MasterServerConnect();
}

void CLanMatchServer::OnDisconnect()
{
	// ������ �����Ѵ�.
	pMatchServer->MasterServerDisconnect();
	while (!CLanClient::Connect(OpenIP, Port, NagleOption))
	{
		wprintf(L"Disconnected MasterServer ReConnect Try\n");
	}
}

void CLanMatchServer::OnRecv(CPacketBuffer *pOutBuffer)
{
	// ��Ŷ ���ν���
	WORD Type;

	*pOutBuffer >> Type;

	switch (Type)
	{
	case en_PACKET_MAT_MAS_RES_SERVER_ON:
		//	��ġ����ŷ ������ ���� ���� Ȯ��
		MasterOpen = true;
		break;
	case en_PACKET_MAT_MAS_RES_GAME_ROOM:
		//	������ ������ �����ϴ� ���ӹ� ����
		pMatchServer->MasterServerResGameRoom(pOutBuffer);
		break;
	}

	pOutBuffer->Free();
}

void CLanMatchServer::OnSend(int SendSize)
{

}
void CLanMatchServer::OnError(int errorcode, WCHAR *msg)
{

}


bool CLanMatchServer::MasterSendPacket(CPacketBuffer *pBuffer)
{
	return SendPacket(pBuffer);
}

void CLanMatchServer::Monitoring()
{


}