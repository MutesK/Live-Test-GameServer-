#include "DBConnectorTLS.h"

CDBConnectorTLS::CDBConnectorTLS(WCHAR *szDBIP, WCHAR *szUser, WCHAR *szPassword, WCHAR *szDBName, int iDBPort, int MinimumExecuteTime)
{
	lstrcpyW(_szDBIP, szDBIP);
	lstrcpyW(_szUser, szUser);
	lstrcpyW(_szPass, szPassword);
	lstrcpyW(_szDBName, szDBName);
	_MinimumExecuteTime = MinimumExecuteTime;

	_DBPort = iDBPort;

	_TLSIndex = TlsAlloc();
	if (_TLSIndex == TLS_OUT_OF_INDEXES)
	{
		int *p = nullptr;
		*p = 0;
	}
}

CDBConnectorTLS::~CDBConnectorTLS()
{
	TlsFree(_TLSIndex);

	CDBConnector *pConnector;
	while (_ConnectStack.Pop(&pConnector))
	{
		pConnector->Disconnect();
	}
}

bool	CDBConnectorTLS::Query(WCHAR *szStringFormat, ...)
{
	CDBConnector *pConnector = (CDBConnector *)TlsGetValue(_TLSIndex);

	if (pConnector == nullptr)
	{
		pConnector = new CDBConnector(_szDBIP, _szUser, _szPass, _szDBName, _DBPort, _MinimumExecuteTime);
		TlsSetValue(_TLSIndex, pConnector);
		_ConnectStack.Push(pConnector);

		while (!pConnector->Connect());
	}

	WCHAR QueryTemp[2048];

	va_list va;
	va_start(va, szStringFormat);
	HRESULT hResult = StringCchVPrintf(QueryTemp, 2048, szStringFormat, va);
	va_end(va);

	pConnector->Query(QueryTemp);
	return true;
}


bool CDBConnectorTLS::Query_Save(WCHAR *szStringFormat, ...)
{
	CDBConnector *pConnector = (CDBConnector *)TlsGetValue(_TLSIndex);

	if (pConnector == nullptr)
	{
		pConnector = new CDBConnector(_szDBIP, _szUser, _szPass, _szDBName, _DBPort, _MinimumExecuteTime);
		TlsSetValue(_TLSIndex, pConnector);
		_ConnectStack.Push(pConnector);

		while (!pConnector->Connect());
	}

	WCHAR QueryTemp[2048];
	va_list va;
	va_start(va, szStringFormat);
	HRESULT hResult = StringCchVPrintf(QueryTemp, 2048, szStringFormat, va);
	va_end(va);

	pConnector->Query_Save(QueryTemp);

	return true;
}

MYSQL_ROW	CDBConnectorTLS::FetchRow(void)
{
	CDBConnector *pConnector = (CDBConnector *)TlsGetValue(_TLSIndex);

	if (pConnector == nullptr)
		return 0;

	return pConnector->FetchRow();
}

void	CDBConnectorTLS::FreeResult(void)
{
	CDBConnector *pConnector = (CDBConnector *)TlsGetValue(_TLSIndex);

	if (pConnector == nullptr)
		return;

	return pConnector->FreeResult();
}

int		CDBConnectorTLS::GetLastError(void)
{
	CDBConnector *pConnector = (CDBConnector *)TlsGetValue(_TLSIndex);

	if (pConnector == nullptr)
		return 0;

	return pConnector->GetLastError();
}
WCHAR* CDBConnectorTLS::GetLastErrorMsg(void)
{
	CDBConnector *pConnector = (CDBConnector *)TlsGetValue(_TLSIndex);

	if (pConnector == nullptr)
		return nullptr;

	return pConnector->GetLastErrorMsg();
}