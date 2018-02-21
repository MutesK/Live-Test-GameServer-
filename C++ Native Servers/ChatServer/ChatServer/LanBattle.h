#pragma once

#include "Engine/LanClient.h"

class CChatServer;

class CLanBattle : public CLanClient
{
public:
	CLanBattle(CChatServer *pServer, WCHAR* ChatServerIP, int Port);
	virtual ~CLanBattle();

	virtual bool Connect(WCHAR *szOpenIP, int port, bool nagleOption);
	virtual void Stop();

	bool SendPacketToBattleServer(CPacketBuffer *pBuffer);
private:
	virtual void OnConnected();
	virtual void OnDisconnect();
	virtual void OnRecv(CPacketBuffer *pOutBuffer);
	virtual void OnSend(int SendSize);
	virtual void OnError(int errorcode, WCHAR *);
private:
	WCHAR OpenIP[16];

	int Port;
	bool NagleOption;

	CChatServer *pChatServer;

	WCHAR ChatServerIP[16];
	int	  CharServerPort;
};

