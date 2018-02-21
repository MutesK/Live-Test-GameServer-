#include "MatchLanServer.h"
#include "BattleLanServer.h"
#include "Engine/Parser.h"
#include "Engine/HttpPost.h"
CCrashDump Dump;
long CCrashDump::_DumpCount;
BattleLanServer BattleServer;
CMatchLanServer MatchServer(&BattleServer);

void Start(WCHAR *szServerConfig, bool bNagleOption);
int main()
{
	SYSLOG_DIRECTROYSET(L"MasterServer_Log");
	Start(L"_MasterServer.cnf", false);

	wprintf(L"Server Open!\n");

	while (1)
	{
		MatchServer.Monitoring();
		BattleServer.Monitoring();
		Sleep(1000);
	}


	return 0;
}

void Start(WCHAR *szServerConfig, bool bNagleOption)
{
	// 매치서버와 배틀서버를 실행, 마스터 토큰을 만든다.
	CParser *pParser = new CParser(szServerConfig);
	char MasterToken[32];

	WCHAR MatchIP[16];
	int MatchPort;
	int MatchThread;
	int MatchMaxSession;

	WCHAR BattleIP[16];
	int BattlePort;
	int BattleThread;
	int BattleMaxSession;

	pParser->GetValue(L"MATCH_IP", MatchIP, L"NETWORK");
	pParser->GetValue(L"MATCH_PORT", &MatchPort, L"NETWORK");
	pParser->GetValue(L"MATCH_WORKER_THREAD", &MatchThread, L"NETWORK");
	pParser->GetValue(L"MATCH_MAX", &MatchMaxSession, L"NETWORK");

	pParser->GetValue(L"BATTLE_IP", BattleIP, L"NETWORK");
	pParser->GetValue(L"BATTLE_PORT", &BattlePort, L"NETWORK");
	pParser->GetValue(L"BATTLE_WORKER_THREAD", &BattleThread, L"NETWORK");
	pParser->GetValue(L"BATTLE_MAX", &BattleMaxSession, L"NETWORK");

	WCHAR _MasterToken[50];
	pParser->GetValue(L"TOKEN", _MasterToken, L"SYSTEM");
	UTF16toUTF8(MasterToken, _MasterToken, 32);


	if (!MatchServer.Start(MatchIP, MatchPort, MatchThread, MatchMaxSession, bNagleOption, MasterToken))
		CCrashDump::Crash();

	if (!BattleServer.Start(BattleIP, BattlePort, BattleThread, BattleMaxSession, bNagleOption, MasterToken))
		CCrashDump::Crash();

}