#pragma once

#include "Engine/LanClient.h"

class CMatchServer;

class CLanMatchServer : public CLanClient
{
public:
	CLanMatchServer(CMatchServer *pServer, WORD ServerNo, char * MasterToken);
	virtual ~CLanMatchServer();

	virtual bool Connect(WCHAR *szOpenIP, int port, bool nagleOption);
	virtual void Stop();

	bool MasterSendPacket(CPacketBuffer *pBuffer);
	void Monitoring();
private:
	virtual void OnConnected();
	virtual void OnDisconnect();
	virtual void OnRecv(CPacketBuffer *pOutBuffer);
	virtual void OnSend(int SendSize);

	virtual void OnError(int errorcode, WCHAR *);
private:
	CMatchServer *pMatchServer;

	WCHAR OpenIP[16];
	int Port;
	bool NagleOption;

	int ServerNo;
	char *pMasterToken;

	bool MasterOpen;

};

