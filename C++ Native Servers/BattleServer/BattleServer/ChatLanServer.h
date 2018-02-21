#pragma once

#include "Engine/LanServer.h"

class CBattleServer;

class CChatLanServer : public CLanServer
{
public:
	CChatLanServer(CBattleServer *pServer);
	virtual ~CChatLanServer();

	virtual bool Start(WCHAR* szServerIP, int Port, int WorkerThread, int MaxSession, bool NagleOption);
	virtual void Stop();

	void ChatDisconnect(ULONG64 SessionID);
	void ChatSendPacket(ULONG64 SessionID, CPacketBuffer *pBuffer);

	void Monitoring();
private:
	virtual void OnClientJoin(ULONG64 OutSessionID, WCHAR *pClientIP, int Port);
	virtual void OnClientLeave(ULONG64 SessionID);

	virtual bool OnConnectionRequest(WCHAR *szIP, int Port);


	virtual void OnRecv(ULONG64 OutSessionID, CPacketBuffer *pOutBuffer);
	virtual void OnSend(ULONG64 OutSessionID, int SendSize);

	virtual void OnError(int errorcode, WCHAR *szMessage);

	virtual void OnWorkerThreadBegin();
	virtual void OnWorkerThreadEnd();


	void ReqChatServerOn(ULONG64 OutSessionID, CPacketBuffer *pOutBuffer);
	void ResConnectToken(ULONG64 OutSessionID, CPacketBuffer *pOutBuffer);
	void ResCreateRoom(ULONG64 OutSessionID, CPacketBuffer *pOutBuffer);
	void ResDestroyRoom(ULONG64 OutSessionID, CPacketBuffer *pOutBuffer);

	
private:
	CBattleServer *pBattleServer;
};

