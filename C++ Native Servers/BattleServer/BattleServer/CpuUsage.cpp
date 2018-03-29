#include <Windows.h>
#include "CpuUsage.h"

CCpuUsage::CCpuUsage(HANDLE hProcess)
{
	// 프로세스 핸들 입력이 없다면 자기 자신을 대상으로 한다.
	if (hProcess == INVALID_HANDLE_VALUE)
	{
		_hProcess = GetCurrentProcess();
	}

	// 프로세서 갯수를 확인한다.
	SYSTEM_INFO SystemInfo;

	GetSystemInfo(&SystemInfo);
	_iNumberOfProcessor = SystemInfo.dwNumberOfProcessors;

	_fProcessorTotal = 0;
	_fProcessorUser = 0;
	_fProcessorKernel = 0;

	_fProcessTotal = 0;
	_fProcessUser = 0;
	_fProcessKernel = 0;

	 _ftProcessor_LastKernel.QuadPart = 0;
	 _ftProcessor_LastUser.QuadPart = 0;
	 _ftProcessor_LastIdle.QuadPart = 0;

	 _ftProcess_LastKernel.QuadPart = 0;
	 _ftProcess_LastUser.QuadPart = 0;
	 _ftProcess_LastTime.QuadPart = 0;

	 UpdateCpuTime();
}





void CCpuUsage::UpdateCpuTime(void)
{
	//---------------------------------------------------------------------------
	// 프로세서 사용률을 갱신한다.
	// 본래의 사용 구조체는 FILETIME 이지만, ULARGE_INTERGER 와 구조가 같으므로 이를 사용함.
	// FILETIME 구조체는 100 ns 단위의 시간 단위를 표현하는 구조체임.
	//---------------------------------------------------------------------------
	ULARGE_INTEGER Idle;
	ULARGE_INTEGER Kernel;
	ULARGE_INTEGER User;

	//---------------------------------------------------------------------------
	// 시스템 사용 시간을 구한다.
	// 아이들 타임 / 커널 사용 타임 (아이들 포함) / 유저 사용 타임
	//---------------------------------------------------------------------------
	if (GetSystemTimes((PFILETIME)&Idle, (PFILETIME)&Kernel, (PFILETIME)&User) == false)
		return;

	// 커널 타임에는 아이들 타임이 포함됨.
	ULONG64 KernelDiff = Kernel.QuadPart - _ftProcessor_LastKernel.QuadPart;
	ULONG64 UserDiff = User.QuadPart - _ftProcessor_LastUser.QuadPart;
	ULONG64 IdleDiff = Idle.QuadPart - _ftProcessor_LastIdle.QuadPart;

	ULONG64 Total = KernelDiff + UserDiff;
	ULONG64 TimeDiff;

	if (Total == 0)
		_fProcessorTotal = _fProcessorUser = _fProcessorKernel = 0.0f;
	else
	{
		_fProcessorTotal = (float)((double)(Total - IdleDiff) / Total * 100.0f);
		_fProcessorUser = (float)((double)UserDiff  / Total * 100.0f);
		_fProcessorKernel = (float)((double)(KernelDiff - IdleDiff) / Total * 100.0f);
	}

	_ftProcessor_LastKernel = Kernel;
	_ftProcessor_LastUser = User;
	_ftProcessor_LastIdle = Idle;

	//--------------------------------------------------------------
	// 지정된 프로세스 사용률을 갱신한다.
	//--------------------------------------------------------------
	ULARGE_INTEGER None;
	ULARGE_INTEGER NowTime;

	GetSystemTimeAsFileTime((LPFILETIME)&NowTime);
	_hProcess = GetCurrentProcess();

	GetProcessTimes(_hProcess, (LPFILETIME)&None, (LPFILETIME)&None,
		(LPFILETIME)&Kernel, (LPFILETIME)&User);

	//--------------------------------------------------------------
	// 이전에 저장된 프로세스 시간과의 차를 구해서 실제로 얼마의 시간이 지났는지 확인
	// 그리고 실제 지나온 시간으로 나누면 사용률이 나옴.
	//--------------------------------------------------------------
	TimeDiff = NowTime.QuadPart - _ftProcess_LastTime.QuadPart;
	UserDiff = User.QuadPart - _ftProcess_LastUser.QuadPart;
	KernelDiff = Kernel.QuadPart - _ftProcess_LastKernel.QuadPart;

	Total = KernelDiff + UserDiff;

	_fProcessTotal = (float)(Total / (double)_iNumberOfProcessor / (double)TimeDiff * 100.0f);
	_fProcessUser = (float)(KernelDiff  / (double)_iNumberOfProcessor / (double)TimeDiff * 100.0f);
	_fProcessKernel = (float)(UserDiff / (double)_iNumberOfProcessor / (double)TimeDiff * 100.0f);

	_ftProcess_LastTime.QuadPart = NowTime.QuadPart;
	_ftProcess_LastKernel.QuadPart = Kernel.QuadPart;
	_ftProcess_LastUser.QuadPart= User.QuadPart;
}
