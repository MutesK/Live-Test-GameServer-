#pragma once

#include <Windows.h>
#include <list>
using namespace std;

#define PROFILE_CHECK
#define PROFILE_NUMMAX 30
#define PROFILE_SAMPLE_MAX 50
#define PROFILE_THREAD_MAX 500

class CSystemLog;

struct PROFILE_DATA
{
	long flag;

	WCHAR szName[64];

	LARGE_INTEGER startTime;
	__int64 iTotalTime;
	__int64 iMin[2];
	__int64 iMax[2];

	__int64 iCall;
};

//////////////////////////////////////////////////////////////////////
// 한 스레드가 가져야 할 프로파일링 필수 TLS 자료들
//////////////////////////////////////////////////////////////////////
class CProfileThread
{
public:
	DWORD dwThreadID;
	PROFILE_DATA Data[PROFILE_SAMPLE_MAX];

	CProfileThread();
};
//////////////////////////////////////////////////////////////////////
class CProfile
{
private:
	CProfile();
	~CProfile();
	DWORD tlsIndex;
	LARGE_INTEGER lFreqency;
	double fMicroFreqeuncy;

	list <CProfileThread *> ProfileList;

	CRITICAL_SECTION WriteSection;
public:
	static CProfile *GetInstance()
	{
		static CProfile Profile;
		return &Profile;
	}

	// 스레드를 찾아내고, 이름을 찾아낸다.
	void ProfileBegin(WCHAR *_fName);
	void ProfileEnd(WCHAR *_fName);
	// 모든 스레드의 데이터를 출력한다.
	void ProfileOutText(WCHAR *szFileName);
	void ProfileReset();

private:
	void ListExistAddCounter(CProfileThread *pThread, WCHAR *_fName);

};
