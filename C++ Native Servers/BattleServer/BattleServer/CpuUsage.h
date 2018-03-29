#pragma once

class CCpuUsage
{
public:
	// ������, Ȯ�� ��� ���μ��� �ڵ�, ���Է½� �ڱ� �ڽ�
	CCpuUsage(HANDLE hProcess = INVALID_HANDLE_VALUE);


	void UpdateCpuTime(void);


	float ProcessorTotal(void) { return _fProcessorTotal;  }
	float ProcessorUser(void) { return _fProcessorUser; }
	float ProcessorKernel(void) { return _fProcessorKernel; }

	float ProcessTotal(void) { return _fProcessTotal; }
	float ProcessUser(void) { return _fProcessUser; }
	float ProcessKernel(void) { return _fProcessKernel; }

private:
	HANDLE _hProcess;
	int _iNumberOfProcessor;

	float _fProcessorTotal;
	float _fProcessorUser;
	float _fProcessorKernel;

	float _fProcessTotal;
	float _fProcessUser;
	float _fProcessKernel;

	ULARGE_INTEGER _ftProcessor_LastKernel;
	ULARGE_INTEGER _ftProcessor_LastUser;
	ULARGE_INTEGER _ftProcessor_LastIdle;

	ULARGE_INTEGER _ftProcess_LastKernel;
	ULARGE_INTEGER _ftProcess_LastUser;
	ULARGE_INTEGER _ftProcess_LastTime;

};

