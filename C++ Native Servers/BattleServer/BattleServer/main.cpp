#include "BattleServer.h"

CBattleServer *pBattleServer;
CCrashDump Dump;
long CCrashDump::_DumpCount;
int main()
{
	_getch();

	SYSLOG_DIRECTROYSET(L"BattleServer_Log");

	pBattleServer = new CBattleServer(6000);

	if (!pBattleServer->Start(L"_BattleServer.cnf", true))
		return 0;
	
	while (1)
	{
		pBattleServer->Monitoring();
		Sleep(1000);
	}

	return 0;
}