#pragma once

#include "CommonInclude.h"
#pragma comment(lib, "Engine\\include\\mysqlclient.lib")
#include "include\mysql.h"
#include "include\errmsg.h"
/////////////////////////////////////////////////////////
// MySQL DB 연결 클래스
//
// 단순하게 MySQL Connector 를 통한 DB 연결만 관리한다.
//
// 스레드에 안전하지 않으므로 주의 해야 함.
// 여러 스레드에서 동시에 이를 사용한다면 개판이 됨.
//
/////////////////////////////////////////////////////////

class CDBConnector
{
public:

	enum
	{
		eQUERY_MAX_LEN = 4096
	};

	CDBConnector(WCHAR *szDBIP, WCHAR *szUser, WCHAR *szPassword, WCHAR *szDBName, int iDBPort, int MinimumExecuteTime);
	virtual ~CDBConnector();

	//////////////////////////////////////////////////////////////////////
	// MySQL DB 연결
	//////////////////////////////////////////////////////////////////////
	bool		Connect(void);

	//////////////////////////////////////////////////////////////////////
	// MySQL DB 끊기
	//////////////////////////////////////////////////////////////////////
	bool		Disconnect(void);


	//////////////////////////////////////////////////////////////////////
	// 쿼리 날리고 결과셋 임시 보관
	//
	//////////////////////////////////////////////////////////////////////
	bool		Query(WCHAR *szStringFormat, ...);
	bool		Query_Save(WCHAR *szStringFormat, ...);	// DBWriter 스레드의 Save 쿼리 전용

	//////////////////////////////////////////////////////////////////////
	// 쿼리를 날린 뒤에 결과 뽑아오기.
	//
	// 결과가 없다면 NULL 리턴.
	//////////////////////////////////////////////////////////////////////
	MYSQL_ROW	FetchRow(void);

	//////////////////////////////////////////////////////////////////////
	// 한 쿼리에 대한 결과 모두 사용 후 정리.
	//////////////////////////////////////////////////////////////////////
	void		FreeResult(void);

	//////////////////////////////////////////////////////////////////////
	// Error 얻기.한 쿼리에 대한 결과 모두 사용 후 정리.
	//////////////////////////////////////////////////////////////////////
	int			GetLastError(void) { return _LastError; };
	WCHAR		*GetLastErrorMsg(void) { return _szLastErrorMsg; }

	bool UTF8toUTF16(const char *szText, WCHAR *szBuff, int iBuffLen);
	bool UTF16toUTF8(char *szText, const WCHAR *szBuff, int iBuffLen);
private:


	//////////////////////////////////////////////////////////////////////
	// mysql 의 LastError 를 맴버변수로 저장한다.
	//////////////////////////////////////////////////////////////////////
	void		SaveLastError(void);
	void		SetLogAndShutdownServer(bool sizeError, WCHAR *pQuery);
private:
	//-------------------------------------------------------------
	// MySQL 연결객체 본체
	//-------------------------------------------------------------
	MYSQL	m_conn;
	//-------------------------------------------------------------
	// MySQL 연결객체 포인터. 위 변수의 포인터임. 
	// 이 포인터의 null 여부로 연결상태 확인.
	//-------------------------------------------------------------
	MYSQL	*pConn;

	WCHAR	_szDBIP[16];
	WCHAR	_szUser[64];
	WCHAR	_szPass[64];
	WCHAR	_szDBName[64];
	int		_DBPort;

	int		_LastError;
	WCHAR	_szLastErrorMsg[200];

	int		_MinimumExecuteTime;
	//-------------------------------------------------------------
	// 쿼리를 날린 뒤 Result 저장소.
	//
	//-------------------------------------------------------------
	MYSQL_RES	*_pSqlResult;

	WCHAR		_szQuery[eQUERY_MAX_LEN];
	char		_szQueryUTF8[eQUERY_MAX_LEN];
};

