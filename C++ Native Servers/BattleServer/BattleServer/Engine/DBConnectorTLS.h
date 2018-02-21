#pragma once

#include "CommonInclude.h"
#include "DBConnector.h"

class CDBConnectorTLS
{
public:
	CDBConnectorTLS(WCHAR *szDBIP, WCHAR *szUser, WCHAR *szPassword, WCHAR *szDBName, int iDBPort, int MinimumExecuteTime);
	virtual ~CDBConnectorTLS();

	bool		Query(WCHAR *szStringFormat, ...);
	bool		Query_Save(WCHAR *szStringFormat, ...);

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

	int			GetLastError(void);
	WCHAR		*GetLastErrorMsg(void);


private:
	WCHAR	_szDBIP[16];
	WCHAR	_szUser[64];
	WCHAR	_szPass[64];
	WCHAR	_szDBName[64];
	int		_DBPort;
	int		_MinimumExecuteTime;

	DWORD	_TLSIndex;

	CLockFreeStack<CDBConnector *> _ConnectStack;
};

