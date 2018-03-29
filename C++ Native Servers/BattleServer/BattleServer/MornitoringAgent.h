#pragma once

#include "Engine/LanClient.h"
#include <array>

class CBattleServer;

class CMornitoringAgent : public CLanClient
{
public:
	CMornitoringAgent(CBattleServer& Server, WORD ServerNo, const array<BYTE, 32>& LoginSessionKey);
	virtual ~CMornitoringAgent();


	virtual bool Connect(const array<WCHAR, 16>& ServerIP, int port, bool nagleOption);
	virtual void Stop();

	bool Send(const BYTE& DataType, const int& DataValue, const int& TimeStamp);

private:
	virtual void OnConnected();
	virtual void OnDisconnect();
	virtual void OnRecv(CPacketBuffer *pOutBuffer);
	virtual void OnSend(int SendSize);

	virtual void OnError(int errorcode, WCHAR *);

	bool isMornMode = false;

    array<BYTE, 32>& LoginSessionKey;
	CBattleServer& Server;
	int ServerNo;

	array<WCHAR, 16> _OpenIP;
	int _port;
};

