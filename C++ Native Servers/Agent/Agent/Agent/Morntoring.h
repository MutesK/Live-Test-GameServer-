#pragma comment(lib,"Pdh.lib")
#include <Windows.h>
#include <Pdh.h>
#include <strsafe.h>
#include "CpuUsage.h"
#include <vector>
#include "Parser.h"
using namespace std;
#define df_PDH_ETHERNET_MAX		8

class CServerMornitor
{
public:
	HANDLE	hServer;
	WCHAR   _szName[128];
	WCHAR	_szExcute[500];

	double m_dNumOfThread;
	double m_dWorkingSet;

	double m_dProcessTotal;
	double m_dProcessUser;
	double m_dProcessKernel;

	//// PDH Handle
	PDH_HCOUNTER	_pdh_Counter_Handle_NumOfThread;
	PDH_HCOUNTER	_pdh_Counter_Handle_WorkingSet;

	CCpuUsage		*pCPUUsage;

	void StartServer();
	bool CheckServer();

	void RequestStop();
	void ForceStop();

	void Update();
};

class CMornitoring
{
private:
	//--------------------------------------------------------------
	// 이더넷 하나에 대한 Send,Recv PDH 쿼리 정보.
	//--------------------------------------------------------------
	struct st_Ethernet
	{
		bool	_bUse;
		WCHAR   _szName[128];

		PDH_HCOUNTER _pdh_Counter_Network_RecvBytes;
		PDH_HCOUNTER _pdh_Counter_Network_SendBytes;
	};

public:
	CMornitoring(WCHAR *fileName);

///////////////////////////////////////////////////////////////////////////////
//	모니터링(Agent) 서버 끄고 켜기 기능
///////////////////////////////////////////////////////////////////////////////
	void StartServer();
	//void RequestStopServer();
	//void ForceStopServer();
///////////////////////////////////////////////////////////////////////////////
//	모니터링(Agent) 요소 업데이트
///////////////////////////////////////////////////////////////////////////////
	void Update();

	void Mornitor();
private:
	CParser *pParser;

	PDH_HQUERY	_pdh_Query;
	///////////////////////////////////////////////////////////////////////////////

	PDH_HCOUNTER	_pdh_Counter_Handle_AvailableMBytes;
	PDH_HCOUNTER	_pdh_Counter_Handle_NonPagedPool;

	PDH_HCOUNTER	_pdh_Counter_Handle_ProcessorTotal;

	vector<CServerMornitor *> Monitor;
	st_Ethernet _Ehternet[df_PDH_ETHERNET_MAX];				// 랜카드별 PDH 정보
	// 맴버 보관용


	double			_pdh_Value_Network_RecvBytes;				// 총 RecvBytes 모든 이더넷 Recv 수치
	double			_pdh_Value_Newtork_SendBytes;				// 총 SendBytes 모든 이더넷 Send 수치

	double			 m_dAvaibleMemory;							// 사용가능한 메모리
	double			 m_dNonPagedMemory;

	double			m_ProcessTotal;
	double			m_ProcessTota2l;


	PDH_HCOUNTER _pdh_Counter_Handle_NumOfThread;
};

