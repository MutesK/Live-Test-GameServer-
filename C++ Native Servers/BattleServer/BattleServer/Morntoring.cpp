
#include "Morntoring.h"
#include <memory>

CMornitoring::CMornitoring()
{
	PdhOpenQuery(NULL, NULL, &_pdh_Query);

	int iCnt = 0;
	bool bErr = false;
	WCHAR *szCur = NULL;
	WCHAR *szCounters = NULL;
	WCHAR *szInterfaces = NULL;
	DWORD dwCounterSize = 0, dwInterfaceSize = 0;
	WCHAR szQuery[1024] = { 0, };

	szCounters = new WCHAR[dwCounterSize];
	szInterfaces = new WCHAR[dwInterfaceSize];

	iCnt = 0;
	szCur = szInterfaces;

	int Server = 0;
	WCHAR ParserTagName[200];
	WCHAR Name[200];

	//CServerMornitor *pMonitor = new CServerMornitor;
	pMonitor = make_shared<CServerMornitor>();


	CServerMornitor *pData = pMonitor.get();
	pData->_szName = L"BattleServer";

	PDH_STATUS Status;

	// 사용 메모리
	StringCbPrintf(szQuery, sizeof(WCHAR) * 1024, L"\\Process(%s)\\Private Bytes", pData->_szName.c_str());
	Status = PdhAddCounter(_pdh_Query, szQuery, NULL, &_pdh_Counter_Handle_PrivateMBytes);

	_bToggleSwitch = true;

	hUpdateThread = (HANDLE)_beginthreadex(nullptr, 0, UpdateThread, this, FALSE, nullptr);

}

void CMornitoring::TurnOff()
{
	_bToggleSwitch = false;
}

void CMornitoring::Update()
{
	// 쿼리 데이터 갱신
	PdhCollectQueryData(_pdh_Query);

	PDH_STATUS Status;
	PDH_FMT_COUNTERVALUE CounterValue;

	Status = PdhGetFormattedCounterValue(_pdh_Counter_Handle_PrivateMBytes,
		PDH_FMT_DOUBLE, NULL, &CounterValue);
	if (Status == 0)
		m_dPrivateMemory = CounterValue.doubleValue / 1024 / 1024;


	auto ptr = pMonitor.get();
	ptr->Update();
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


}


UINT WINAPI CMornitoring::UpdateThread(LPVOID arg)
{
	shared_ptr<CMornitoring> ptr{static_cast<CMornitoring *>(arg)};

	while (ptr->_bToggleSwitch)
	{
		ptr->Update();

		Sleep(1000);
	}

	return 0;
}