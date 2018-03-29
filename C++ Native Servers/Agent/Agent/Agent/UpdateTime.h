#pragma once

#pragma comment(lib, "winmm.lib")
#include <Windows.h>
#include <process.h>

/////////////////////////////////////////////////////////////////////
//			시간 자동 갱신 클래스
/////////////////////////////////////////////////////////////////////
class CUpdateTime
{
private:
	CUpdateTime()
	{
		timeBeginPeriod(1);
		b_ThreadWork = false;
		b_Exit = false;
	}

	~CUpdateTime()
	{
		timeEndPeriod(1);
	}


public:
	static CUpdateTime* GetInstance()
	{
		static CUpdateTime Instance;
		Instance.Run();

		return &Instance;
	}

	static SYSTEMTIME* GetSystemTime()
	{
		return &CUpdateTime::GetInstance()->NowTime;
	}

	static ULONGLONG* GetTickCount()
	{
		return &CUpdateTime::GetInstance()->TickCount;
	}

	void Run()
	{
		if (!b_ThreadWork)
		{
			b_ThreadWork = true;
			TickCount = GetTickCount64();
			GetLocalTime(&NowTime);

			hTimeUpdateThread = (HANDLE)_beginthreadex(nullptr, 0, UpdateTime, this, FALSE, nullptr);
		}
	}

	static UINT WINAPI UpdateTime(LPVOID arg)
	{
		CUpdateTime *pUpdateTime = (CUpdateTime *)arg;


		while (!pUpdateTime->b_Exit)
		{
			pUpdateTime->TickCount = GetTickCount64();
			GetLocalTime(&pUpdateTime->NowTime);

			Sleep(5);
		}

		return 1;
	}


	void CloseThread()
	{
		b_Exit = true;
	}

	SYSTEMTIME NowTime;
	ULONGLONG TickCount;
private:
	HANDLE hTimeUpdateThread;
	bool b_Exit;
	bool b_ThreadWork;
};



