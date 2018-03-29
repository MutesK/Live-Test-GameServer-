#include <iostream>
#include "Morntoring.h"
#include "UpdateTime.h"
using namespace std;
void KeyProcess();
bool isMornitor();
CMornitoring Mornitoring(L"Config.cfg");
int main()
{
	Mornitoring.StartServer();

	while (1)
	{
		KeyProcess();

		if (isMornitor())
		{
			system("cls");
			Mornitoring.Mornitor();
			Mornitoring.Update();
		}
		
	}

	return 0;
}


void KeyProcess()
{

}

bool isMornitor()
{
	static ULONG64 LastTick = *CUpdateTime::GetTickCount();
	ULONG64 CurrentTick = *CUpdateTime::GetTickCount();

	if (CurrentTick - LastTick >= 1000)
	{
		LastTick = CurrentTick;
		return true;
	}
	return false;
}