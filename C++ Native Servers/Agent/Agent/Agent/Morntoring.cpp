
#include "Morntoring.h"
#include "Parser.h"

CMornitoring::CMornitoring(WCHAR *fileName)
{
	pParser = new CParser(fileName);

	PdhOpenQuery(NULL, NULL, &_pdh_Query);

	int iCnt = 0;
	bool bErr = false;
	WCHAR *szCur = NULL;
	WCHAR *szCounters = NULL;
	WCHAR *szInterfaces = NULL;
	DWORD dwCounterSize = 0, dwInterfaceSize = 0;
	WCHAR szQuery[1024] = { 0, };

	PdhEnumObjectItems(NULL, NULL, L"Network Interface", szCounters, &dwCounterSize, szInterfaces, &dwInterfaceSize, PERF_DETAIL_WIZARD, 0);

	szCounters = new WCHAR[dwCounterSize];
	szInterfaces = new WCHAR[dwInterfaceSize];

	if (PdhEnumObjectItems(NULL, NULL, L"Network Interface", szCounters, &dwCounterSize, szInterfaces, 
		&dwInterfaceSize, PERF_DETAIL_WIZARD, 0) != ERROR_SUCCESS)
		exit(-1);

	iCnt = 0;
	szCur = szInterfaces;

	//---------------------------------------------------------
	// szInterfaces 에서 문자열 단위로 끊으면서 , 이름을 복사받는다.
	//---------------------------------------------------------
	for (; *szCur != L'\0' && iCnt < df_PDH_ETHERNET_MAX; szCur += wcslen(szCur) + 1, iCnt++)
	{
		_Ehternet[iCnt]._bUse = true;
		_Ehternet[iCnt]._szName[0] = L'\0';

		wcscpy_s(_Ehternet[iCnt]._szName, szCur);

		szQuery[0] = L'\0';
		StringCbPrintf(szQuery, sizeof(WCHAR) * 1024, L"\\Network Interface(%s)\\Bytes Received/sec", szCur);
		PdhAddCounter(_pdh_Query, szQuery, NULL, &_Ehternet[iCnt]._pdh_Counter_Network_RecvBytes);

		szQuery[0] = L'\0';
		StringCbPrintf(szQuery, sizeof(WCHAR) * 1024, L"\\Network Interface(%s)\\Bytes Sent/sec", szCur);
		PdhAddCounter(_pdh_Query, szQuery, NULL, &_Ehternet[iCnt]._pdh_Counter_Network_SendBytes);
	}


	int Server = 0;
	WCHAR ParserTagName[200];
	WCHAR Name[200];
	pParser->GetValue(L"NUM_OF_SERVER", &Server, L"SYSTEM");

	if (Server >= 1)
	{
		for (int i = 1; i <= Server; i++)
		{
			CServerMornitor *pMonitor = new CServerMornitor;
			Monitor.push_back(pMonitor);

			wsprintf(ParserTagName, L"NAME_%d", i);

			// 서버 감시 대상 이름 가져온다.
			pParser->GetValue(ParserTagName, Name, L"SYSTEM");

			// 해당 이름으로 EXECUTE, Name을 가져온다.
			pParser->GetValue(L"EXECUTE", pMonitor->_szExcute, Name);
			pParser->GetValue(L"NAME", pMonitor->_szName, Name);

			PDH_STATUS Status;

			// 스레드 수
			StringCbPrintf(szQuery, sizeof(WCHAR) * 1024, L"\\Process(%s)\\Private Bytes", pMonitor->_szName);
			Status = PdhAddCounter(_pdh_Query, szQuery, NULL, &pMonitor->_pdh_Counter_Handle_NumOfThread);

			// 사용 메모리
			StringCbPrintf(szQuery, sizeof(WCHAR) * 1024, L"\\Process(%s)\\Working Set", pMonitor->_szName);
			Status = PdhAddCounter(_pdh_Query, szQuery, NULL, &pMonitor->_pdh_Counter_Handle_WorkingSet);

		}
	}


	PDH_STATUS Status;

	// 스레드 수
	StringCbPrintf(szQuery, sizeof(WCHAR) * 1024, L"\\Process(%s)\\Private Bytes", L"Agent");
	Status = PdhAddCounter(_pdh_Query, szQuery, NULL, &_pdh_Counter_Handle_NumOfThread);

	// 사용 가능 메모리
	StringCbPrintf(szQuery, sizeof(WCHAR) * 1024, L"\\Memory\\Available MBytes");
	PdhAddCounter(_pdh_Query, szQuery, NULL, &_pdh_Counter_Handle_AvailableMBytes);

	StringCbPrintf(szQuery, sizeof(WCHAR) * 1024, L"\\Memory\\Pool Nonpaged Bytes");
	PdhAddCounter(_pdh_Query, szQuery, NULL, &_pdh_Counter_Handle_NonPagedPool);

	// 프로세스 토탈
	StringCbPrintf(szQuery, sizeof(WCHAR) * 1024, L"\\Processor(_Total)\\%% Processor Time");
	PdhAddCounter(_pdh_Query, szQuery, NULL, &_pdh_Counter_Handle_ProcessorTotal);


}


void CMornitoring::Update()
{
	// 쿼리 데이터 갱신
	PdhCollectQueryData(_pdh_Query);

	for (auto iter : Monitor)
	{
		CServerMornitor *pMonitor = iter;

		iter->Update();
	}

	PDH_STATUS Status;
	PDH_FMT_COUNTERVALUE CounterValue;
	//////////////////////////////////////////////////////////////////////////////////////
	// 네트워크와 사용가능 메모리, 논페이지 메모리를 받아온다.
	//////////////////////////////////////////////////////////////////////////////////////

	for (int iCnt = 0; iCnt < df_PDH_ETHERNET_MAX; iCnt++)
	{
		if (_Ehternet[iCnt]._bUse)
		{
			Status = PdhGetFormattedCounterValue(_Ehternet[iCnt]._pdh_Counter_Network_RecvBytes,
				PDH_FMT_DOUBLE, NULL, &CounterValue);
			if (Status == 0)
				_pdh_Value_Network_RecvBytes += CounterValue.doubleValue;

			Status = PdhGetFormattedCounterValue(_Ehternet[iCnt]._pdh_Counter_Network_SendBytes,
				PDH_FMT_DOUBLE, NULL, &CounterValue);
			if (Status == 0) 
				_pdh_Value_Newtork_SendBytes += CounterValue.doubleValue;
		}
	}

	Status = PdhGetFormattedCounterValue(_pdh_Counter_Handle_NumOfThread,
		PDH_FMT_DOUBLE, NULL, &CounterValue);
	if (Status == 0)
		m_ProcessTota2l = CounterValue.doubleValue;

	Status = PdhGetFormattedCounterValue(_pdh_Counter_Handle_AvailableMBytes,
		PDH_FMT_DOUBLE, NULL, &CounterValue);
	if (Status == 0)
		m_dAvaibleMemory = CounterValue.doubleValue;

	Status = PdhGetFormattedCounterValue(_pdh_Counter_Handle_NonPagedPool,
		PDH_FMT_DOUBLE, NULL, &CounterValue);
	if (Status == 0)
		m_dNonPagedMemory = CounterValue.doubleValue;

	Status = PdhGetFormattedCounterValue(_pdh_Counter_Handle_ProcessorTotal,
		PDH_FMT_DOUBLE, NULL, &CounterValue);
	if (Status == 0)
		m_ProcessTotal = CounterValue.doubleValue;

}

void CMornitoring::StartServer()
{
	for (auto iter : Monitor)
	{
		CServerMornitor *pMonitor = iter;

		iter->StartServer();
	}
}

void CServerMornitor::Update()
{
	if (pCPUUsage == nullptr)
		pCPUUsage = new CCpuUsage(hServer);
	else
	{
		//////////////////////////////////////////////////////////////////////////
		// 해당 CPU 사용률 업데이트 시킨다.
		///////////////////////////////////////////////////////////////////////////
		pCPUUsage->UpdateCpuTime();

		m_dProcessKernel = pCPUUsage->ProcessKernel();
		m_dProcessUser = pCPUUsage->ProcessUser();
		m_dProcessTotal = pCPUUsage->ProcessTotal();
	}

	///////////////////////////////////////////////////////////////////////////
	//  PDH 데이터 뽑기
	//  스레드 수, 워킹 셋
	///////////////////////////////////////////////////////////////////////////
	PDH_STATUS Status;
	PDH_FMT_COUNTERVALUE CounterValue;

	Status = PdhGetFormattedCounterValue(_pdh_Counter_Handle_NumOfThread, PDH_FMT_DOUBLE, NULL, &CounterValue);
	if (Status == 0)
		m_dNumOfThread = CounterValue.doubleValue;

	Status = PdhGetFormattedCounterValue(_pdh_Counter_Handle_WorkingSet, PDH_FMT_DOUBLE, NULL, &CounterValue);
	if (Status == 0)
		m_dWorkingSet = CounterValue.doubleValue;
}

void CServerMornitor::StartServer()
{
	HWND hWnd;
	DWORD pid, tid;

	PROCESS_INFORMATION ProcessInfo;
	ZeroMemory(&ProcessInfo, sizeof(PROCESS_INFORMATION));

	STARTUPINFO StartInfo;
	ZeroMemory(&StartInfo, sizeof(STARTUPINFO));

	hWnd = FindWindow(0, _szExcute);


	if (hWnd == 0)
	{
		// 실행중이지 않을경우
		DWORD dwExitCode;

		if (GetExitCodeProcess(hServer, &dwExitCode))
		{
			if (dwExitCode == STATUS_PENDING)
				return;
		}

		StartInfo.cb = sizeof(StartInfo);
		StartInfo.dwFlags = STARTF_USEFILLATTRIBUTE | STARTF_USESHOWWINDOW;
		StartInfo.dwFillAttribute = FOREGROUND_INTENSITY | BACKGROUND_RED;
		StartInfo.wShowWindow = SW_MINIMIZE;

		bool ret = CreateProcess(_szExcute, NULL, NULL, NULL, false, CREATE_NEW_CONSOLE, NULL, NULL, &StartInfo, &ProcessInfo);

		if (!ret)
		{
			wprintf(L"Server Monitoring Start Failed ErrorCode : %d\n", GetLastError());
		}
		else
		{
			CloseHandle(ProcessInfo.hThread);
			hServer = ProcessInfo.hProcess;
		}
	}
	else
	{
		// 실행중일 경우
		tid = GetWindowThreadProcessId(hWnd, &pid);

		hServer = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
	}
}

bool CServerMornitor::CheckServer()
{
	DWORD dwExitCode;

	if (GetExitCodeProcess(hServer, &dwExitCode))
	{
		if (dwExitCode == STATUS_PENDING)
			return true;
	}

	return false;
}

void CServerMornitor::ForceStop()
{
	DWORD dwExitCode = 0;

	TerminateProcess(hServer, dwExitCode);
}


void CMornitoring::Mornitor()
{
	wprintf(L"====================================================\n");
	wprintf(L"		Mornitoring					  \n");
	wprintf(L"====================================================\n");
	for (auto iter : Monitor)
	{
		CServerMornitor *pMonitor = iter;

		wprintf(L"Process Name : %s\n", iter->_szName);
		wprintf(L"Num Of Thread : %.0f \n", iter->m_dNumOfThread);
		wprintf(L"Working Set (Use Memory): %.0f KBytes\n", iter->m_dWorkingSet / 1024);
		wprintf(L"Process Total : %f %%\n", iter->m_dProcessTotal);
		wprintf(L"Process User : %f %%\n", iter->m_dProcessUser);
		wprintf(L"Process Kernel : %f %%\n", iter->m_dProcessKernel);
		wprintf(L"====================================================\n");
	}
	wprintf(L"====================================================\n");
	wprintf(L"Available Memory : %.2f MBytes\n", m_dAvaibleMemory);
	wprintf(L"Non Paged Memory : %.2f MBytes\n", m_dNonPagedMemory / 1024 / 1024);
	wprintf(L"Network RecvBytes : %.2f KBytes\n", _pdh_Value_Network_RecvBytes / 1024);
	wprintf(L"Network SendBytes : %.2f KBytes\n", _pdh_Value_Newtork_SendBytes / 1024);
	wprintf(L"====================================================\n");
	wprintf(L"Processor Total : %f %%\n", m_ProcessTotal);
}
