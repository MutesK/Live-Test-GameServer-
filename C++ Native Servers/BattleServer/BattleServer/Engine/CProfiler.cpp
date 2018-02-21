#include <iostream>
#include <locale.h>
#include "CProfiler.h"


CProfileThread::CProfileThread()
{
	setlocale(LC_ALL, "");
	dwThreadID = GetCurrentThreadId();

	for (int i = 0; i < PROFILE_NUMMAX; i++)
	{
		Data[i].flag = false;
		Data[i].iTotalTime = 0;
		Data[i].iMin[0] = INT64_MAX;
		Data[i].iMin[1] = INT64_MAX;

		Data[i].iMax[0] = 0;
		Data[i].iMax[1] = 0;

		Data[i].iCall = 0;
	}
}

CProfile::CProfile()
{
	QueryPerformanceFrequency(&lFreqency);
	fMicroFreqeuncy = 1000;

	tlsIndex = TlsAlloc();

	if (tlsIndex == TLS_OUT_OF_INDEXES)
	{
		// Crash Dump
		//SYSLOG(L"DEBUG", LOG_DEBUG, L"CProfile TLS Error \n");

	}

	InitializeCriticalSection(&WriteSection);

}
CProfile::~CProfile()
{
	TlsFree(tlsIndex);

	for (auto iter = ProfileList.begin(); iter != ProfileList.end();)
	{
		CProfileThread * pThread = (*iter);
		iter = ProfileList.erase(iter);
		delete pThread;
	}

	DeleteCriticalSection(&WriteSection);

}

void CProfile::ProfileBegin(WCHAR *_fName)
{
	CProfileThread *pThread = (CProfileThread *)TlsGetValue(tlsIndex);

	if (pThread == nullptr)
	{
		// 해당 스레드의 데이터를 새로 추가한다.
		CProfileThread *pNewThread = new CProfileThread;
		ProfileList.push_back(pNewThread);

		if (!TlsSetValue(tlsIndex, pNewThread))
		{
			wprintf(L"Tls Set Error \n");
			int *p = nullptr;
			*p = 0;
		}

		pThread = pNewThread;
	}

	ListExistAddCounter(pThread, _fName);

}


void CProfile::ListExistAddCounter(CProfileThread *pThread, WCHAR *_fName)
{
	int index = 0;

	while (index < PROFILE_NUMMAX)
	{
		if (pThread->Data[index].flag == true)
		{
			if (lstrcmpW(pThread->Data[index].szName, _fName) == 0)
			{
				pThread->Data[index].iCall++;
				QueryPerformanceCounter(&(pThread->Data[index].startTime));
				return;
			}
			index++;
			continue;
		}


		pThread->Data[index].flag = true;
		lstrcpynW(pThread->Data[index].szName, _fName, 64);

		pThread->Data[index].iCall++;
		QueryPerformanceCounter(&(pThread->Data[index].startTime));
		return;
	}
}

void CProfile::ProfileEnd(WCHAR *_fName)
{
	CProfileThread *pThread = (CProfileThread *)TlsGetValue(tlsIndex);

	if (pThread == nullptr)
		return;		// TLS에 등록조차 안되어 있다면 볼필요도없다.

	int index = 0;

	while (index < PROFILE_NUMMAX)
	{
		if (pThread->Data[index].flag == true)
		{
			if (lstrcmpW(_fName, pThread->Data[index].szName) == 0)
			{
				LARGE_INTEGER endTime;
				QueryPerformanceCounter(&endTime);

				__int64 time = (endTime.QuadPart - pThread->Data[index].startTime.QuadPart);


				if (time < pThread->Data[index].iMin[1])
				{
					if (time < pThread->Data[index].iMin[0])
					{
						pThread->Data[index].iMin[1] = pThread->Data[index].iMin[0];
						pThread->Data[index].iMin[0] = time;
					}
					else
						pThread->Data[index].iMin[1] = time;
				}



				if (time > pThread->Data[index].iMax[1])
				{
					if (time > pThread->Data[index].iMax[0])
					{
						pThread->Data[index].iMax[1] = pThread->Data[index].iMax[0];
						pThread->Data[index].iMax[0] = time;
					}
					else
						pThread->Data[index].iMax[1] = time;

				}

				pThread->Data[index].iTotalTime += time;

				return;
			}
		}

		index++;
	}
}

void CProfile::ProfileOutText(WCHAR *szFileName)
{
	EnterCriticalSection(&WriteSection);
	FILE *fp;
	_wfopen_s(&fp, szFileName, L"w");

	fwprintf(fp, L"      ThreadID      |        Name        |        Average     |          Min       |         Max        |   Call   |\n");  // Len = 99
	fwprintf(fp, L"------------------------------------------------------------------------------------------------------------------- \n");

	auto begin = ProfileList.begin();
	auto end = ProfileList.end();

	for (auto iter = begin; iter != end; ++iter)
	{
		CProfileThread *pThread = (*iter);
		PROFILE_DATA* pData = pThread->Data;
		for (int i = 0; i < PROFILE_NUMMAX; i++)
		{
			if (pData[i].flag)
			{
				if (pData[i].iMin[1] == INT64_MAX)
					pData[i].iMin[1] = pData[i].iMin[0];

				if (pData[i].iMax[1] == 0)
					pData[i].iMax[1] = pData[i].iMax[0];

				fwprintf(fp, L"%20d|%20s|%18f㎲|%18f㎲|%18f㎲|%10d|\n",
					pThread->dwThreadID,
					pData[i].szName,
					((pData[i].iTotalTime / (double)pData[i].iCall) / lFreqency.QuadPart) * fMicroFreqeuncy,
					(double)pData[i].iMin[1] / (double)lFreqency.QuadPart * fMicroFreqeuncy,
					(double)pData[i].iMax[1] / (double)lFreqency.QuadPart * fMicroFreqeuncy,
					pData[i].iCall);
			}
		}
		fwprintf(fp, L"------------------------------------------------------------------------------------------------------------------- \n");
	}

	fclose(fp);
	LeaveCriticalSection(&WriteSection);
}

// 모든 칼럼을 지우고 
void CProfile::ProfileReset()
{
	QueryPerformanceFrequency(&lFreqency);
	fMicroFreqeuncy = 1000;

	for (auto iter = ProfileList.begin(); iter != ProfileList.end();)
	{
		CProfileThread * pThread = (*iter);
	}
}