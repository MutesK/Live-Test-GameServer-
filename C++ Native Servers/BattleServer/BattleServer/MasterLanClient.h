#pragma once

#include "Engine/LanClient.h"

class CBattleServer;

class CMasterLanClient : public CLanClient
{
public:
	CMasterLanClient(CBattleServer *pBattleServer);
	virtual ~CMasterLanClient();

	virtual bool Connect(WCHAR *szOpenIP, int port, bool nagleOption);

	virtual void Stop();

	void Monitoring();
	void MasterSendPacket(CPacketBuffer *pBuffer);
private:
	virtual void OnConnected();
	virtual void OnDisconnect();

	virtual void OnRecv(CPacketBuffer *pOutBuffer);
	virtual void OnSend(int SendSize);


	virtual void OnError(int errorcode, WCHAR *errMsg);

	void ResServerOn(CPacketBuffer *pOutBuffer);
	void ResConnectToken(CPacketBuffer *pOutBuffer);
	void ResCreatedRoom(CPacketBuffer *pOutBuffer);
	void ResClosedRoom(CPacketBuffer *pOutBuffer);
	void ResLeftUser(CPacketBuffer *pOutBuffer);
private:
	CBattleServer *pBattleServer;

	WCHAR OpenIP[16];
	int Port;
	bool NagleOption;
};

