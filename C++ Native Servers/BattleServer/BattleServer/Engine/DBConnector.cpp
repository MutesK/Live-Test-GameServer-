
#include "DBConnector.h"
#include <Strsafe.h>


CDBConnector::CDBConnector(WCHAR *szDBIP, WCHAR *szUser, WCHAR *szPassword, WCHAR *szDBName, int iDBPort, int MinimumExecuteTime)
{
	lstrcpyW(_szDBIP, szDBIP);
	lstrcpyW(_szUser, szUser);
	lstrcpyW(_szPass, szPassword);
	lstrcpyW(_szDBName, szDBName);
	_MinimumExecuteTime = MinimumExecuteTime;
	_DBPort = iDBPort;
}


CDBConnector::~CDBConnector()
{

}


bool CDBConnector::Connect(void)
{
	mysql_init(&m_conn);

	char szIP[16];
	char szUser[100];
	char szPass[100];
	char szDBName[100];

	UTF16toUTF8(szIP, _szDBIP, 16);
	UTF16toUTF8(szUser, _szUser, lstrlenW(_szUser) + 1);
	UTF16toUTF8(szPass, _szPass, lstrlenW(_szPass) + 1);
	UTF16toUTF8(szDBName, _szDBName, lstrlenW(_szDBName) + 1);

	pConn = mysql_real_connect(&m_conn, szIP, szUser, szPass, szDBName, _DBPort, (char *)NULL, 0);
	if (pConn == nullptr)
	{
		SaveLastError();
		return false;
	}
	mysql_set_character_set(pConn, "utf8");

	return true;
}

bool CDBConnector::Disconnect(void)
{
	if (&m_conn == nullptr)
		return false;

	mysql_close(pConn);

	pConn = nullptr;
}

bool CDBConnector::UTF8toUTF16(const char *szText, WCHAR *szBuff, int iBuffLen)
{
	int iRe = MultiByteToWideChar(CP_UTF8, 0, szText, strlen(szText), szBuff, iBuffLen);
	if (iRe < iBuffLen)
		szBuff[iRe] = L'\0';
	return true;
}

bool CDBConnector::UTF16toUTF8(char *szText, const WCHAR *szBuff, int iBuffLen)
{
	int iRe = WideCharToMultiByte(CP_UTF8, 0, szBuff, lstrlenW(szBuff), static_cast<LPSTR>(szText), iBuffLen, NULL, NULL);
	if (iRe < iBuffLen)
		szText[iRe] = '\0';
	return true;
}

bool CDBConnector::Query(WCHAR *szStringFormat, ...)
{
	va_list va;
	va_start(va, _szQuery);
	HRESULT hResult = StringCchVPrintf(_szQuery, eQUERY_MAX_LEN, szStringFormat, va);
	va_end(va);

	if (FAILED(va))
	{
		// 로그
		WCHAR QueryTemp[5000];
		StringCchVPrintf(QueryTemp, 5000, szStringFormat, va);
		SetLogAndShutdownServer(true, QueryTemp);
	}

	UTF16toUTF8(_szQueryUTF8, _szQuery, eQUERY_MAX_LEN);
	int ret = 1;
	int retry = 5;

	ULONG64 ProcessTick;
	while (ret != 0)
	{
		ProcessTick = CUpdateTime::GetTickCount();
		ret = mysql_query(pConn, _szQueryUTF8);

		if (ret != 0)
		{
			// 오류
			SaveLastError();

			if (_LastError == CR_SOCKET_CREATE_ERROR ||
				_LastError == CR_CONNECTION_ERROR ||
				_LastError == CR_CONN_HOST_ERROR ||
				_LastError == CR_SERVER_GONE_ERROR ||
				_LastError == CR_TCP_CONNECTION ||
				_LastError == CR_SERVER_HANDSHAKE_ERR ||
				_LastError == CR_SERVER_LOST ||
				_LastError == CR_INVALID_CONN_HANDLE)
			{
				// 접속오류라면 재접속 처리후 쿼리 시도한다.
				if (retry == 0)
				{
					// 꺼버린다.
					SetLogAndShutdownServer(false, _szQuery);
				}
				
				Disconnect();
				Connect();
				retry--;
			}
			else
			{
				// 로그를  남기고 꺼버린다.
				SetLogAndShutdownServer(false, _szQuery);
			}
		}
	}

	if (CUpdateTime::GetTickCount() - ProcessTick >= _MinimumExecuteTime)
		SYSLOG(L"Query", LOG_SYSTEM, L"Query : %s, Using Time : %lld", _szQuery, CUpdateTime::GetTickCount() - ProcessTick);
	
	// 결과값 저장
	_pSqlResult = mysql_store_result(pConn);
	return true;
}

bool CDBConnector::Query_Save(WCHAR *szStringFormat, ...)
{
	va_list va;
	va_start(va, _szQuery);
	HRESULT hResult = StringCchVPrintf(_szQuery, eQUERY_MAX_LEN, szStringFormat, va);
	va_end(va);

	if (FAILED(va))
	{
		// 로그
		WCHAR QueryTemp[5000];
		StringCchVPrintf(QueryTemp, 5000, szStringFormat, va);
		SetLogAndShutdownServer(true, QueryTemp);
	}


	UTF16toUTF8(_szQueryUTF8, _szQuery, eQUERY_MAX_LEN);
	int ret = 1;
	int retry = 5;
	ULONG64 ProcessTick;

	while (ret != 0)
	{
		ProcessTick = CUpdateTime::GetTickCount();
		ret = mysql_query(pConn, _szQueryUTF8);

		if (ret != 0)
		{
			// 오류
			SaveLastError();

			if (_LastError == CR_SOCKET_CREATE_ERROR ||
				_LastError == CR_CONNECTION_ERROR ||
				_LastError == CR_CONN_HOST_ERROR ||
				_LastError == CR_SERVER_GONE_ERROR ||
				_LastError == CR_TCP_CONNECTION ||
				_LastError == CR_SERVER_HANDSHAKE_ERR ||
				_LastError == CR_SERVER_LOST ||
				_LastError == CR_INVALID_CONN_HANDLE)
			{
				// 접속오류라면 재접속 처리후 쿼리 시도한다.
				if (retry == 0)
				{
					// 꺼버린다.
					SetLogAndShutdownServer(false, _szQuery);
				}

				Disconnect();
				Connect();
				retry--;
			}
			else
			{
				// 로그를  남기고 꺼버린다.
				SetLogAndShutdownServer(false, _szQuery);
			}
		}
	}

	if (CUpdateTime::GetTickCount() - ProcessTick >= _MinimumExecuteTime)
		SYSLOG(L"Query", LOG_SYSTEM, L"Query : %s, Using Time : %lld", _szQuery, CUpdateTime::GetTickCount() - ProcessTick);

	_pSqlResult = mysql_store_result(pConn);

	return true;
}

void	CDBConnector::SetLogAndShutdownServer(bool sizeError, WCHAR *pQuery)
{
	SaveLastError();

	if (!sizeError)
		SYSLOG(L"ERROR", LOG_ERROR, L"MySQL Error : %d, Msg : %s", _LastError, _szLastErrorMsg);

	SYSLOG(L"ERROR", LOG_ERROR, L"MySQL Error : Query : %s", pQuery);

	CCrashDump::Crash();
}

MYSQL_ROW	CDBConnector::FetchRow(void)
{
	if (_pSqlResult == nullptr)
		return nullptr;

	MYSQL_ROW ret = mysql_fetch_row(_pSqlResult);

	return ret;
}

void	CDBConnector::FreeResult(void)
{
	mysql_free_result(_pSqlResult);
}
void	CDBConnector::SaveLastError(void)
{
	_LastError = mysql_errno(pConn);

	char ErrorMsg[200];
	strcpy_s(ErrorMsg, mysql_error(pConn));

	UTF8toUTF16(ErrorMsg, _szLastErrorMsg, strlen(ErrorMsg));
}