#include "ChatServer.h"
using namespace std;
void Mornitoring();
void InsertKey();
CChatServer Server;
CCrashDump Dump;
long CCrashDump::_DumpCount;
int main()
{
	SYSLOG_DIRECTROYSET(L"ChatServer_Log");

	if (!Server.Start(L"_ChatServer.cnf", true))
	{
		CCrashDump::Crash();
		return false;
	}
	while (1)
	{
		Mornitoring();
		InsertKey();
	}
}

void Mornitoring()
{
	static ULONG64 Tick = CUpdateTime::GetTickCount();

	if (CUpdateTime::GetTickCount() - Tick >= 1000)
	{
		Tick = CUpdateTime::GetTickCount();
		Server.Mornitoring();
	}
}

void InsertKey()
{
	if (_kbhit())
	{
		char ch = _getch();

		if (ch == 'q' || ch == 'Q')
			Server.Stop();
		else if (ch == 's' || ch == 'S')
			Server.Start(L"_ChatServer.cnf", false);
	}
}