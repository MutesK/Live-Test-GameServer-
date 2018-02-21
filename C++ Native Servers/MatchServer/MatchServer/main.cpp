
#include "MatchServer.h"

CMatchServer Server;
CCrashDump Dump;
long CCrashDump::_DumpCount;

int main()
{
	SYSLOG_DIRECTROYSET(L"MatchServer_Log");

	if (!Server.Start(L"_MatchServer.cnf", true))
		return false;

	wprintf(L"Server Open!\n");

	while (1)
	{
		Server.Monitoring();
		Sleep(1000);
		PRO_TEXT(L"HTTP PROCESS TIME");
	}


	return 0;
}