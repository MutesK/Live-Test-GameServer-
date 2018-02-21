#pragma once

///////////////////////////////////////////////////////////////////////////////
/*
  Made by Mute

	CSystemLog

	시스템 로그 클래스

	1. GetInstance 하면 생성된다.
	2. 기본적으로 로그레벨은 LOG_DEBUG
	3. 인스턴스 생성했다면 디렉토리 설정은 필수적으로 해야된다.

Tip)
	이후 만지게 될경우 문자열 초과가 의심된다면 매크로 변수 dfLOGMSGLEN_MAX를 바꿔준다.

	로그레벨의 종류
	LOG_DEBUG
	LOG_WARNING,
	LOG_ERROR,
	LOG_SYSTEM,

*/
///////////////////////////////////////////////////////////////////////////////

#include <Windows.h>
#include <iostream>
#include <direct.h>
#include <Strsafe.h>
#include "UpdateTime.h"



enum en_LOG_LEVEL
{
	LOG_DEBUG = 0,
	LOG_WARNING,
	LOG_ERROR,
	LOG_SYSTEM,
};

class CSystemLog;

#define dfLOGMSGLEN_MAX 10000

#define SYSLOG_SET(LOGLEVEL) CSystemLog::GetInstance()->SetLogLevel(LOGLEVEL)

extern WCHAR szDirectory[dfLOGMSGLEN_MAX];

#define SYSLOG_DIRECTROYSET(fmt, ...)										\
do{																			\
		wsprintf(szDirectory, fmt, ##__VA_ARGS__);							\
		CSystemLog::GetInstance()->SetLogDirectory(szDirectory);			\
}while(0)																	\

/*
로그 레벨
LOG_DEBUG = 0,
LOG_WARNING,
LOG_ERROR,
LOG_SYSTEM,
*/
#define SYSLOG(TYPE, LOGLEVEL, fmt, ... )		CSystemLog::GetInstance()->Log((TYPE), (LOGLEVEL), fmt, ##__VA_ARGS__ )
#define SYSLOGHEX(TYPE, LOGLEVEL, SZLOG, DATA, DATALEN)   CSystemLog::GetInstance()->LogHex((TYPE), (LOGLEVEL), (SZLOG), (DATA), (DATALEN))

class CSystemLog
{
private:
	CSystemLog(en_LOG_LEVEL LogLevel);
	~CSystemLog();
	
public:

	//------------------------------------------------------
	// 싱글톤 클래스, 
	//------------------------------------------------------
	static CSystemLog *GetInstance(en_LOG_LEVEL LogLevel = LOG_DEBUG)
	{
		static CSystemLog Log(LogLevel);
		return &Log;
	}

	//------------------------------------------------------
	// 외부에서 로그레벨 제어
	//------------------------------------------------------
	void SetLogLevel(en_LOG_LEVEL LogLevel) { _SaveLogLevel = LogLevel; }

	//------------------------------------------------------
	// 로그 경로 지정.
	//------------------------------------------------------
	void SetLogDirectory(WCHAR *szDirectory)
	{
		_wmkdir(szDirectory);
		wsprintf(_SaveDirectory, L"%s\\", szDirectory);
	}

	//------------------------------------------------------
	// 실제 로그 남기는 함수.
	// szStringFormat에 시간 및 각종 의미는 넣지말고 디버그 메세지 넣을껏만 넣는다.
	// 개행문자는 안넣는걸 추천한다. 내부에서 개행문자 처리중
	// ex) Log(L"ERROR", LOG_ERROR, L"LOG ERROR BUFFER SIZE OVER ");
	//------------------------------------------------------
	void Log(WCHAR *szType, en_LOG_LEVEL LogLevel, WCHAR *szStringFormat, ...);

	//------------------------------------------------------
	// BYTE 바이너리를 헥사로 로그 출력
	// 디버그에서 메모리 보는것과 비슷하게 출력해주는 툴
	//------------------------------------------------------
	void LogHex(WCHAR *szType, en_LOG_LEVEL LogLevel, WCHAR *szLog, BYTE *pByte, int iByteLen);



private:

	unsigned long	_LogNo;
	SRWLOCK			_srwLock;

	en_LOG_LEVEL	_SaveLogLevel;
	WCHAR			_SaveDirectory[25];
};

