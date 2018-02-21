
#include "CSystemLog.h"

using namespace std;


WCHAR szDirectory[dfLOGMSGLEN_MAX];

CSystemLog::CSystemLog(en_LOG_LEVEL LogLevel)
{
	SetLogLevel(LogLevel);
	_wsetlocale(LC_ALL, L"");
	InitializeSRWLock(&_srwLock);
}

CSystemLog::~CSystemLog()
{

}

void CSystemLog::Log(WCHAR *szType, en_LOG_LEVEL LogLevel, WCHAR *szStringFormat, ...)
{
	if (_SaveLogLevel > LogLevel)
		return;

	WCHAR szInMessage[dfLOGMSGLEN_MAX];

	InterlockedIncrement(&_LogNo);

	va_list va;
	va_start(va, szStringFormat);
	HRESULT hResult = StringCchVPrintf(szInMessage, dfLOGMSGLEN_MAX, szStringFormat, va);
	va_end(va);

	if (FAILED(hResult))
	{
		// 로그 에러를 출력
		Log(L"ERROR", LOG_ERROR, L"LOG ERROR BUFFER SIZE OVER \n");
	}

	SYSTEMTIME Time = CUpdateTime::GetSystemTime();
	//SetLogDirectory(szType);

	// 파일 이름 설정
	FILE *fp;
	WCHAR FileName[150];
	StringCchPrintf(FileName, 150, L"%s%d%02d%02d_%s.txt", _SaveDirectory, Time.wYear, Time.wMonth, Time.wDay, szType);

	WCHAR Data[dfLOGMSGLEN_MAX];

	switch (LogLevel)
	{
	case LOG_DEBUG:
		StringCchPrintf(Data, dfLOGMSGLEN_MAX, L"[%s][%4d-%02d-%02d %02d-%02d-%02d / DEBUG / %09d] %s\n",
			szType, Time.wYear, Time.wMonth, Time.wDay, Time.wHour, Time.wMinute, Time.wSecond, _LogNo, szInMessage);

		//wprintf(L"[%s][%02d-%02d-%02d / DEBUG / %09d] %s \n", szType, Time.wHour, Time.wMinute, Time.wSecond, _LogNo, szInMessage);
		break;
	case LOG_WARNING:
		StringCchPrintf(Data, dfLOGMSGLEN_MAX, L"[%s][%4d-%02d-%02d %02d-%02d-%02d / WARNN / %09d] %s\n",
			szType, Time.wYear, Time.wMonth, Time.wDay, Time.wHour, Time.wMinute, Time.wSecond, _LogNo, szInMessage);

		//wprintf(L"[%s][%02d-%02d-%02d / WARNN / %09d] %s \n", szType, Time.wHour, Time.wMinute, Time.wSecond, _LogNo, szInMessage);
		break;
	case LOG_ERROR:
		StringCchPrintf(Data, dfLOGMSGLEN_MAX, L"[%s][%4d-%02d-%02d %02d-%02d-%02d / ERROR / %09d] %s\n",
			szType, Time.wYear, Time.wMonth, Time.wDay, Time.wHour, Time.wMinute, Time.wSecond, _LogNo, szInMessage);

		//wprintf(L"[%s][%02d-%02d-%02d / ERROR / %09d] %s \n", szType, Time.wHour, Time.wMinute, Time.wSecond, _LogNo, szInMessage);
			break;
	case LOG_SYSTEM:
		StringCchPrintf(Data, dfLOGMSGLEN_MAX, L"[%s][%4d-%02d-%02d %02d-%02d-%02d/ SYSTM / %09d] %s\n",
			szType, Time.wYear, Time.wMonth, Time.wDay, Time.wHour, Time.wMinute, Time.wSecond, _LogNo, szInMessage);

		//wprintf(L"[%s][%02d-%02d-%02d / SYSTM / %09d] %s \n", szType, Time.wHour, Time.wMinute, Time.wSecond, _LogNo, szInMessage);
		break;
	}


	// 파일 연다.
	AcquireSRWLockExclusive(&_srwLock);  // 락 -> Multiple Thread 환경을 위해서
	_wfopen_s(&fp, FileName, L"a");
	
	fwprintf(fp, Data);

	fclose(fp);
	ReleaseSRWLockExclusive(&_srwLock);

}

void CSystemLog::LogHex(WCHAR *szType, en_LOG_LEVEL LogLevel, WCHAR *szLog, BYTE *pByte, int iByteLen)
{
	if (_SaveLogLevel > LogLevel)
		return;

	InterlockedIncrement(&_LogNo);

	SYSTEMTIME Time = CUpdateTime::GetSystemTime();


	FILE *fp;
	WCHAR FileName[150];
	StringCchPrintf(FileName, 150, L"%s%d%02d%02d_%s.txt", _SaveDirectory, Time.wYear, Time.wMonth, Time.wDay, szType);

	WCHAR Data[dfLOGMSGLEN_MAX];
	

	switch (LogLevel)
	{
	case LOG_DEBUG:
		StringCchPrintf(Data, dfLOGMSGLEN_MAX, L"[%s][%4d-%02d-%02d %02d-%02d-%02d / DEBUG / %09d] %s [",
			szType, Time.wYear, Time.wMonth, Time.wDay, Time.wHour, Time.wMinute, Time.wSecond, _LogNo, szLog);

		//wprintf(L"[%s][%02d-%02d-%02d / DEBUG / %09d] Hex %s \n", szType, Time.wHour, Time.wMinute, Time.wSecond, _LogNo, szLog);
		break;
	case LOG_WARNING:
		StringCchPrintf(Data, dfLOGMSGLEN_MAX, L"[%s][%4d-%02d-%02d %02d-%02d-%02d / WARNN / %09d] %s [",
			szType, Time.wYear, Time.wMonth, Time.wDay, Time.wHour, Time.wMinute, Time.wSecond, _LogNo, szLog);

		wprintf(L"[%s][%02d-%02d-%02d / DEBUG / %09d] Hex %s \n", szType, Time.wHour, Time.wMinute, Time.wSecond, _LogNo, szLog);
		break;
	case LOG_ERROR:
		StringCchPrintf(Data, dfLOGMSGLEN_MAX, L"[%s][%4d-%02d-%02d %02d-%02d-%02d / ERROR / %09d] %s [",
			szType, Time.wYear, Time.wMonth, Time.wDay, Time.wHour, Time.wMinute, Time.wSecond, _LogNo, szLog);

		wprintf(L"[%s][%02d-%02d-%02d / DEBUG / %09d] Hex %s \n", szType, Time.wHour, Time.wMinute, Time.wSecond, _LogNo, szLog);
		break;
	case LOG_SYSTEM:
		StringCchPrintf(Data, dfLOGMSGLEN_MAX, L"[%s][%4d-%02d-%02d %02d-%02d-%02d / SYSTM / %09d] %s [",
			szType, Time.wYear, Time.wMonth, Time.wDay, Time.wHour, Time.wMinute, Time.wSecond, _LogNo, szLog);

		wprintf(L"[%s][%02d-%02d-%02d / DEBUG / %09d] Hex %s \n", szType, Time.wHour, Time.wMinute, Time.wSecond, _LogNo, szLog);
		break;
	}



	AcquireSRWLockExclusive(&_srwLock);  // 락 -> Multiple Thread 환경을 위해서
	_wfopen_s(&fp, FileName, L"a");

	fwprintf(fp, Data);
	// pByte를 0부터 iByteLen까지 순회하면서 %x로 출력한다.

	for (int i = iByteLen - 2; i >= 0; i-=2)
	{
		fwprintf(fp, L"%x%x", pByte[i], pByte[i+1]);
	}
	fwprintf(fp, L"]\n");

	fclose(fp);
	ReleaseSRWLockExclusive(&_srwLock);
}