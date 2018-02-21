#include "ChatLanServer.h"
#include "BattleServer.h"

CChatLanServer::CChatLanServer(CBattleServer *pServer)
{
	pBattleServer = pServer;
}


CChatLanServer::~CChatLanServer()
{
}


bool CChatLanServer::Start(WCHAR* szServerIP, int Port, int WorkerThread, int MaxSession, bool NagleOption)
{
	if (!CLanServer::Start(szServerIP, Port, WorkerThread, NagleOption, MaxSession))
		return false;

	return true;
}

void CChatLanServer::Stop()
{

}

void CChatLanServer::OnClientJoin(ULONG64 OutSessionID, WCHAR *pClientIP, int Port)
{

}

void CChatLanServer::OnClientLeave(ULONG64 SessionID)
{
	pBattleServer->ChatServerDisconnect();
}

void CChatLanServer::ChatDisconnect(ULONG64 SessionID)
{
	Disconnect(SessionID);
}

void CChatLanServer::ChatSendPacket(ULONG64 SessionID, CPacketBuffer *pBuffer)
{
	if (!SendPacket(SessionID, pBuffer))
		pBuffer->Free();
}

bool CChatLanServer::OnConnectionRequest(WCHAR *szIP, int Port)
{
	return true;
}

void CChatLanServer::OnRecv(ULONG64 OutSessionID, CPacketBuffer *pOutBuffer)
{
	WORD Type;

	*pOutBuffer >> Type;

	switch (Type)
	{
	case en_PACKET_CHAT_BAT_REQ_SERVER_ON:
		ReqChatServerOn(OutSessionID, pOutBuffer);
		break;
	case en_PACKET_CHAT_BAT_RES_CONNECT_TOKEN:
		ResConnectToken(OutSessionID, pOutBuffer);
		break;
	case en_PACKET_CHAT_BAT_RES_CREATED_ROOM:
		ResCreateRoom(OutSessionID, pOutBuffer);
		break;
	case en_PACKET_CHAT_BAT_RES_DESTROY_ROOM:
		ResDestroyRoom(OutSessionID, pOutBuffer);
		break;
	}

	pOutBuffer->Free();
}

void CChatLanServer::OnSend(ULONG64 OutSessionID, int SendSize)
{

}

void CChatLanServer::OnError(int errorcode, WCHAR *szMessage)
{

}

void CChatLanServer::OnWorkerThreadBegin()
{

}
void CChatLanServer::OnWorkerThreadEnd()
{

}

void CChatLanServer::ReqChatServerOn(ULONG64 OutSessionID, CPacketBuffer *pOutBuffer)
{
	WCHAR ChatServerIP[16];
	WORD ChatServerPort;

	pOutBuffer->GetData(reinterpret_cast<char *>(ChatServerIP), 32);
	*pOutBuffer >> ChatServerPort;

	pBattleServer->ChatServerReqServerOn(OutSessionID, ChatServerIP, ChatServerPort);


}
void CChatLanServer::ResConnectToken(ULONG64 OutSessionID, CPacketBuffer *pOutBuffer)
{
	//	배틀서버의 연결토큰 재발행 수신 확인 
	UINT ReqSequence;
	*pOutBuffer >> ReqSequence;

	pBattleServer->ChatServerResConnectToken(ReqSequence);
}
void CChatLanServer::ResCreateRoom(ULONG64 OutSessionID, CPacketBuffer *pOutBuffer)
{
	//	배틀 서버의 신규 대기 방 생성 수신 응답
	int RoomNo;
	UINT ReqSequence;
	*pOutBuffer >> RoomNo >> ReqSequence;

	pBattleServer->ChatServerResCreateRoom(RoomNo, ReqSequence);
}
void CChatLanServer::ResDestroyRoom(ULONG64 OutSessionID, CPacketBuffer *pOutBuffer)
{
	int RoomNo;
	UINT ReqSequence;
	*pOutBuffer >> RoomNo >> ReqSequence;

	pBattleServer->ChatServerResDestoryRoom(RoomNo, ReqSequence);
}

void CChatLanServer::Monitoring()
{
	wcout << L"ChatServer: Recv PacketTPS :" << m_RecvPacketTPS << endl;
	wcout << L"ChatServer: Send PacketTPS :" << m_SendPacketTPS << endl;

	m_RecvPacketTPS = m_SendPacketTPS = 0;
}