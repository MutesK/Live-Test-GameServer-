#pragma once

#include "CommonInclude.h"
#pragma comment(lib, "Engine\\include\\mysqlclient.lib")
#include "include\mysql.h"
#include "include\errmsg.h"
/////////////////////////////////////////////////////////
// MySQL DB ���� Ŭ����
//
// �ܼ��ϰ� MySQL Connector �� ���� DB ���Ḹ �����Ѵ�.
//
// �����忡 �������� �����Ƿ� ���� �ؾ� ��.
// ���� �����忡�� ���ÿ� �̸� ����Ѵٸ� ������ ��.
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
	// MySQL DB ����
	//////////////////////////////////////////////////////////////////////
	bool		Connect(void);

	//////////////////////////////////////////////////////////////////////
	// MySQL DB ����
	//////////////////////////////////////////////////////////////////////
	bool		Disconnect(void);


	//////////////////////////////////////////////////////////////////////
	// ���� ������ ����� �ӽ� ����
	//
	//////////////////////////////////////////////////////////////////////
	bool		Query(WCHAR *szStringFormat, ...);
	bool		Query_Save(WCHAR *szStringFormat, ...);	// DBWriter �������� Save ���� ����

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

	//////////////////////////////////////////////////////////////////////
	// Error ���.�� ������ ���� ��� ��� ��� �� ����.
	//////////////////////////////////////////////////////////////////////
	int			GetLastError(void) { return _LastError; };
	WCHAR		*GetLastErrorMsg(void) { return _szLastErrorMsg; }

	bool UTF8toUTF16(const char *szText, WCHAR *szBuff, int iBuffLen);
	bool UTF16toUTF8(char *szText, const WCHAR *szBuff, int iBuffLen);
private:


	//////////////////////////////////////////////////////////////////////
	// mysql �� LastError �� �ɹ������� �����Ѵ�.
	//////////////////////////////////////////////////////////////////////
	void		SaveLastError(void);
	void		SetLogAndShutdownServer(bool sizeError, WCHAR *pQuery);
private:
	//-------------------------------------------------------------
	// MySQL ���ᰴü ��ü
	//-------------------------------------------------------------
	MYSQL	m_conn;
	//-------------------------------------------------------------
	// MySQL ���ᰴü ������. �� ������ ��������. 
	// �� �������� null ���η� ������� Ȯ��.
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
	// ������ ���� �� Result �����.
	//
	//-------------------------------------------------------------
	MYSQL_RES	*_pSqlResult;

	WCHAR		_szQuery[eQUERY_MAX_LEN];
	char		_szQueryUTF8[eQUERY_MAX_LEN];
};

