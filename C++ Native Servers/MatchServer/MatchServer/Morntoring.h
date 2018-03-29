#pragma comment(lib,"Pdh.lib")
#include <Windows.h>
#include <Pdh.h>
#include <strsafe.h>
#include "CpuUsage.h"
#include <vector>
#include <thread>
#include <process.h>
#include "Parser.h"
using namespace std;
#define df_PDH_ETHERNET_MAX		8

class CServerMornitor
{
public:
	HANDLE	hServer = INVALID_HANDLE_VALUE;

	wstring	  _szName;

	double m_dProcessTotal;
	double m_dProcessUser;
	double m_dProcessKernel;

	CCpuUsage		*pCPUUsage;

	void Update();
};

class CMornitoring
{
public:
	CMornitoring();

///////////////////////////////////////////////////////////////////////////////
//	모니터링(Agent) 요소 업데이트
///////////////////////////////////////////////////////////////////////////////
	static UINT WINAPI UpdateThread(LPVOID arg);
	void Update();

	void TurnOff();

	double GetCpuUsage()
	{
		return (pMonitor->m_dProcessKernel + pMonitor->m_dProcessUser);
	}
	double GetMemoryUsage()
	{
		return m_dPrivateMemory;
	}


private:
	HANDLE hUpdateThread;
	PDH_HQUERY	_pdh_Query;
	///////////////////////////////////////////////////////////////////////////////

	PDH_HCOUNTER	_pdh_Counter_Handle_PrivateMBytes;
	// 맴버 보관용

	double			 m_dPrivateMemory;							// 사용가능한 메모리

	bool	_bToggleSwitch;
	shared_ptr<CServerMornitor> pMonitor;

	friend class MatchServer;
};

