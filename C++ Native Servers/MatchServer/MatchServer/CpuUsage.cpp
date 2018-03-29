#include <Windows.h>
#include "CpuUsage.h"

CCpuUsage::CCpuUsage(HANDLE hProcess)
{
	// ���μ��� �ڵ� �Է��� ���ٸ� �ڱ� �ڽ��� ������� �Ѵ�.
	if (hProcess == INVALID_HANDLE_VALUE)
	{
		_hProcess = GetCurrentProcess();
	}

	// ���μ��� ������ Ȯ���Ѵ�.
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
	// ���μ��� ������ �����Ѵ�.
	// ������ ��� ����ü�� FILETIME ������, ULARGE_INTERGER �� ������ �����Ƿ� �̸� �����.
	// FILETIME ����ü�� 100 ns ������ �ð� ������ ǥ���ϴ� ����ü��.
	//---------------------------------------------------------------------------
	ULARGE_INTEGER Idle;
	ULARGE_INTEGER Kernel;
	ULARGE_INTEGER User;

	//---------------------------------------------------------------------------
	// �ý��� ��� �ð��� ���Ѵ�.
	// ���̵� Ÿ�� / Ŀ�� ��� Ÿ�� (���̵� ����) / ���� ��� Ÿ��
	//---------------------------------------------------------------------------
	if (GetSystemTimes((PFILETIME)&Idle, (PFILETIME)&Kernel, (PFILETIME)&User) == false)
		return;

	// Ŀ�� Ÿ�ӿ��� ���̵� Ÿ���� ���Ե�.
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
	// ������ ���μ��� ������ �����Ѵ�.
	//--------------------------------------------------------------
	ULARGE_INTEGER None;
	ULARGE_INTEGER NowTime;

	GetSystemTimeAsFileTime((LPFILETIME)&NowTime);
	_hProcess = GetCurrentProcess();

	GetProcessTimes(_hProcess, (LPFILETIME)&None, (LPFILETIME)&None,
		(LPFILETIME)&Kernel, (LPFILETIME)&User);

	//--------------------------------------------------------------
	// ������ ����� ���μ��� �ð����� ���� ���ؼ� ������ ���� �ð��� �������� Ȯ��
	// �׸��� ���� ������ �ð����� ������ ������ ����.
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
