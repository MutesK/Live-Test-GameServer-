#pragma once

///////////////////////////////////////////////////////////////////////////////
/*
  Made by Mute

	CSystemLog

	�ý��� �α� Ŭ����

	1. GetInstance �ϸ� �����ȴ�.
	2. �⺻������ �α׷����� LOG_DEBUG
	3. �ν��Ͻ� �����ߴٸ� ���丮 ������ �ʼ������� �ؾߵȴ�.

Tip)
	���� ������ �ɰ�� ���ڿ� �ʰ��� �ǽɵȴٸ� ��ũ�� ���� dfLOGMSGLEN_MAX�� �ٲ��ش�.

	�α׷����� ����
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
�α� ����
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
	// �̱��� Ŭ����, 
	//------------------------------------------------------
	static CSystemLog *GetInstance(en_LOG_LEVEL LogLevel = LOG_DEBUG)
	{
		static CSystemLog Log(LogLevel);
		return &Log;
	}

	//------------------------------------------------------
	// �ܺο��� �α׷��� ����
	//------------------------------------------------------
	void SetLogLevel(en_LOG_LEVEL LogLevel) { _SaveLogLevel = LogLevel; }

	//------------------------------------------------------
	// �α� ��� ����.
	//------------------------------------------------------
	void SetLogDirectory(WCHAR *szDirectory)
	{
		_wmkdir(szDirectory);
		wsprintf(_SaveDirectory, L"%s\\", szDirectory);
	}

	//------------------------------------------------------
	// ���� �α� ����� �Լ�.
	// szStringFormat�� �ð� �� ���� �ǹ̴� �������� ����� �޼��� �������� �ִ´�.
	// ���๮�ڴ� �ȳִ°� ��õ�Ѵ�. ���ο��� ���๮�� ó����
	// ex) Log(L"ERROR", LOG_ERROR, L"LOG ERROR BUFFER SIZE OVER ");
	//------------------------------------------------------
	void Log(WCHAR *szType, en_LOG_LEVEL LogLevel, WCHAR *szStringFormat, ...);

	//------------------------------------------------------
	// BYTE ���̳ʸ��� ���� �α� ���
	// ����׿��� �޸� ���°Ͱ� ����ϰ� ������ִ� ��
	//------------------------------------------------------
	void LogHex(WCHAR *szType, en_LOG_LEVEL LogLevel, WCHAR *szLog, BYTE *pByte, int iByteLen);



private:

	unsigned long	_LogNo;
	SRWLOCK			_srwLock;

	en_LOG_LEVEL	_SaveLogLevel;
	WCHAR			_SaveDirectory[25];
};

