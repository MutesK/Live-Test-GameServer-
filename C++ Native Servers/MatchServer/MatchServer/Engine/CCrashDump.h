#ifndef __CRASH_DUMP_
#define __CRASH_DUMP_

#include "CommonInclude.h"
#include "APIHook.h"


////////////////////////////////////////////////////////////////////////
////		덤프 클래스
////////////////////////////////////////////////////////////////////////
class CCrashDump
{
public:
	CCrashDump()
	{
		_DumpCount = 0;

		_invalid_parameter_handler oldHandler, newHandler;
		newHandler = MyInvalidParameterHandler;

		oldHandler = _set_invalid_parameter_handler(newHandler);		// CRT함수에 nullptr 등으로 인해 크래시
		_CrtSetReportMode(_CRT_WARN, 0);
		_CrtSetReportMode(_CRT_ASSERT, 0);
		_CrtSetReportMode(_CRT_ERROR, 0);

		_CrtSetReportHook(_custom_Report_hook);

		// 순수 가상 함수 호출 에러 핸들러를 사용자 정의 함수로 우회시킨다.
		_set_purecall_handler(myPurecallHandler);

		_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
		signal(SIGABRT, signalHandler);
		signal(SIGINT, signalHandler);
		signal(SIGILL, signalHandler);
		signal(SIGFPE, signalHandler);
		signal(SIGSEGV, signalHandler);
		signal(SIGTERM, signalHandler);

		//wprintf(L"Dump Constructure start  \n");

		SetHandlerDump();
	}
	static void SetHandlerDump()
	{
		SetUnhandledExceptionFilter(MyExceptionFilter);

		//	wprintf(L"Hooking Start \n");

		static CAPIHook apiHook("Kernel32.dll", "SetUnhandledExceptionFilter", (PROC)RedirectedSetUnhandledExcpetionFilter, true);
	}

	static LONG WINAPI MyExceptionFilter(__in PEXCEPTION_POINTERS pExcetionPointer)
	{
		int iWorkingMemory = 0;
		SYSTEMTIME stNowTime;

		long DumpCount = InterlockedIncrement(&_DumpCount);

		// 현재 날짜와 시간을 알아온다.
		WCHAR fileName[MAX_PATH];
		GetLocalTime(&stNowTime);

		wsprintf(fileName, L"Dump_%d%02d%02d_%02d.%02d.%02d.%02d_%d.dmp", stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay,
			stNowTime.wHour, stNowTime.wMinute, stNowTime.wSecond, DumpCount);

		wprintf(L"\n\n\n !!! Crush Error !!! %d.%d.%d / %d:%d:%d \n", stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay,
			stNowTime.wHour, stNowTime.wMinute, stNowTime.wSecond);

		wprintf(L"Now Saving Dump File \n");

		HANDLE hDumpFile = CreateFile(fileName, GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

		if (hDumpFile != INVALID_HANDLE_VALUE)
		{
			_MINIDUMP_EXCEPTION_INFORMATION MiniDumpExceptionInfomation;

			MiniDumpExceptionInfomation.ThreadId = GetCurrentThreadId();
			MiniDumpExceptionInfomation.ExceptionPointers = pExcetionPointer;
			MiniDumpExceptionInfomation.ClientPointers = true;

			MiniDumpWriteDump(
				GetCurrentProcess(),
				GetCurrentProcessId(),
				hDumpFile,
				MiniDumpWithFullMemory,
				&MiniDumpExceptionInfomation,
				nullptr, nullptr);

			CloseHandle(hDumpFile);

			wprintf(L"CrashDump Save Finished \n");
		}

		return EXCEPTION_EXECUTE_HANDLER;
	}

	static LONG WINAPI RedirectedSetUnhandledExcpetionFilter(EXCEPTION_POINTERS *ExceptionInfo)
	{
		MyExceptionFilter(ExceptionInfo);

		return EXCEPTION_EXECUTE_HANDLER;
	}

	static void Crash(void)
	{
		int *p = nullptr;
		*p = 0;
	}

	static void MyInvalidParameterHandler(const WCHAR *expression, const WCHAR * function, const WCHAR *filfile, UINT line,
		uintptr_t pReserved)
	{
		Crash();
	}

	static int _custom_Report_hook(int iRepostType, char *message, int *pReturnValue)
	{
		Crash();
		return true;
	}
	static void myPurecallHandler(void)
	{
		Crash();
	}

	static void signalHandler(int Error)
	{
		Crash();
	}

	static long _DumpCount;
};






#endif