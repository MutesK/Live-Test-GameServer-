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
	// ������ ���� �ڿ� ��� �̾ƿ���.
	//
	// ����� ���ٸ� NULL ����.
	//////////////////////////////////////////////////////////////////////
	MYSQL_ROW	FetchRow(void);

	//////////////////////////////////////////////////////////////////////
	// �� ������ ���� ��� ��� ��� �� ����.
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

