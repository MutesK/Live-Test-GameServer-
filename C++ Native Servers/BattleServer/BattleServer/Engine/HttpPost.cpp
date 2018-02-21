#include <conio.h>
#include <ws2tcpip.h>
#include "HttpPost.h"
#include "rapidjson\document.h"
#include "rapidjson\writer.h"
#include "rapidjson\stringbuffer.h"

bool DomainToIP(WCHAR *szDomain, WCHAR *outszIP)
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	ADDRINFOW *pAddrInfo;
	SOCKADDR_IN *pSockAddr;
	DWORD size = 100;

	int ret = GetAddrInfo(szDomain, L"0", NULL, &pAddrInfo);
	if (ret != 0)
		return FALSE;

	pSockAddr = (SOCKADDR_IN *)pAddrInfo->ai_addr;
	IN_ADDR *pAddr = (IN_ADDR *)&pSockAddr->sin_addr;

	if (WSAAddressToString((SOCKADDR *)pSockAddr, sizeof(SOCKADDR_IN), NULL, outszIP, &size)
		== SOCKET_ERROR)
	{
		int Error = WSAGetLastError();

		return FALSE;
	}
	return TRUE;
}

/////////////////////////////////////////////////////
// HTTP 접속
/////////////////////////////////////////////////////
SOCKET Connect(WCHAR *szHost, int iPort)
{
	SOCKADDR_IN SockAddr;
	DWORD dwResult;

	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	// TCP 소켓 생성
	SOCKET Socket = socket(AF_INET, SOCK_STREAM, 0);

	if (Socket == INVALID_SOCKET)
		return INVALID_SOCKET;

	char cFlag = 1;
	DWORD dwValue = 3000; // TimeOut;

	if (SOCKET_ERROR == setsockopt(Socket, IPPROTO_TCP, TCP_NODELAY, &cFlag, sizeof(char)))
	{
		dwValue = WSAGetLastError();
		return INVALID_SOCKET;
	}

	if (SOCKET_ERROR == setsockopt(Socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&dwValue, sizeof(DWORD)))
	{
		dwValue = WSAGetLastError();
		return INVALID_SOCKET;
	}

	if (SOCKET_ERROR == setsockopt(Socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&dwValue, sizeof(DWORD)))
	{
		dwValue = WSAGetLastError();
		return INVALID_SOCKET;
	}

	// 서버에 접속
	memset(&SockAddr, 0, sizeof(SockAddr));

	SockAddr.sin_family = AF_INET;
	SockAddr.sin_port = htons(iPort);
	InetPton(AF_INET, szHost, &SockAddr.sin_addr);
	//DomainToIP(szHost, &SockAddr.sin_addr);

	dwResult = WSAConnect(Socket, (sockaddr *)&SockAddr, sizeof(SOCKADDR_IN), NULL, NULL, NULL, NULL);

	if (SOCKET_ERROR == dwResult)
	{
		DWORD Error = WSAGetLastError();
		return INVALID_SOCKET;
	}

	return Socket;

}

/////////////////////////////////////////////////////
// Send 보냄
/////////////////////////////////////////////////////
int Send(SOCKET Socket, const char *szQuery, const char *szPostData)
{
	int iTotalSend = 0;
	int iSend = 0;
	int iResult = 0;
	int iLen = strlen(szQuery);


	// HTTP 헤더 전송
	while (iSend < iLen)
	{
		if (iResult == -1)
			break;

		iResult = send(Socket, szQuery + iSend, iLen - iSend, 0);
		iSend += iResult;
	}

	iTotalSend += iSend;

	iSend = 0;
	iLen = strlen(szPostData);

	// POST 데이터 보냄
	while (iSend < iLen)
	{
		if (iResult == -1)
			break;

		iResult = send(Socket, szPostData, iLen, 0);
		iSend += iResult;
	}

	iTotalSend += iSend;

	return iTotalSend;
}

int Recv(SOCKET Socket, char *szRecvData, int iMaxSize)
{
	int iBufferSize = iMaxSize + 500; // 최대 사이즈에 적당히 HTTP 헤더 예상크기 더함
	char *pRecvBuff = new char[iBufferSize + 1]; // 버퍼는 문자열이 되므로 전체가 꽉찰 것을 대비하여 하나 비워줌
	char *Content = nullptr;
	char *Code = nullptr;

	int iRecv = 0;
	int iResult, iCode = 0;

	bool bHeader = true;
	int iHeaderLen = 0;
	int iContentLen = 0;
	int iContentRecv = 0;

	memset(pRecvBuff, 0, iBufferSize + 1);

	while (1)
	{
		iResult = recv(Socket, pRecvBuff + iRecv, iBufferSize - iRecv, 0);
		if (0 >= iResult)
			break;

		iRecv += iResult;

		// HTTP 헤더 처리
		if (bHeader)
		{
			// 첫 0x20을 찾아서 결과코드를 얻는다.  HTTP/1.1 200 xxxx
			Content = strchr(pRecvBuff, 0x20);

			if (Content != nullptr)
			{
				Code = strchr(Content + 1, 0x20);

				if (Code != nullptr)
				{
					// 잠시 코드만 문자열로 만들기 위해 뒤에 null을 넣엇다가 다시 0x20을 넣어 복구한다.
					*Code = NULL;
					iCode = atoi(Content + 1);
					*Code = 0x20;
				}
			}

			// Content-Length 를 찾아서 컨텐츠 길이를 구한다.
			Content = strstr(pRecvBuff, "Content-Length:");
			if (Content != nullptr)
			{
				Code = strchr(Content + 15, 0x0d);

				if (Code != nullptr)
				{
					// 잠시 코드만 문자열로 만들기 위해 뒤에 null을 넣엇다가 다시 0x20을 넣어 복구한다.
					*Code = NULL;
					iContentLen = atoi(Content + 15);
					*Code = 0x0d;
				}
			}


			Content = strstr(pRecvBuff, "\r\n\r\n");
			if (Content != nullptr)
			{
				Content += 4;

				// 헤더사이즈 계산
				iHeaderLen = Content - pRecvBuff;

				// 헤처처리 끝
				bHeader = false;
			}
		}



		// 컨텐츠까지 다 받앗다면 중지.
		if (iRecv >= iHeaderLen + iContentLen)
			break;
	}

	// 컨턴츠부터 szRecvData로 복사한다.
	memcpy(szRecvData, Content, iContentLen);
	delete[] pRecvBuff;

	return iCode;
}

//////////////////////////////////////////////////////////////////////
// HTTP Post 요청
//
// POST DATA에는 ANSICODE 또는 UTF-8 데이터가 와야 함.
//////////////////////////////////////////////////////////////////////
int HTTP_POST(WCHAR *szHost, WCHAR *szPath, const char *szPostData, char *szRecvData)
{
	char szHTTPQuery[1024] = { 0, };

	// http://는 빼고 받는다.
	if (NULL != wcsstr(szHost, L"http://"))
		return -1;

	// 호스트와 패스를 char 포인터로 변환
	int iHostLen = (int)wcslen(szHost);
	char *pHost = new char[iHostLen * 2 + 1];
	memset(pHost, 0, iHostLen * 2 + 1);
	WideCharToMultiByte(CP_UTF8, 0, szHost, iHostLen, pHost, iHostLen * 2 + 1, NULL, NULL);

	int iPathLen = (int)wcslen(szPath);
	char *pPath = new char[iPathLen * 2 + 1];
	memset(pPath, 0, iPathLen * 2 + 1);
	WideCharToMultiByte(CP_UTF8, 0, szPath, iPathLen, pPath, iPathLen * 2 + 1, NULL, NULL);

	// HTTP Post 헤더를 만든다.
	char szTemplate[] = "POST %s HTTP/1.1\r\nHOST: %s\r\nUser-Agent:C_Client\r\nConnection: Close\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\n\r\n";
	sprintf_s(szHTTPQuery, szTemplate, pPath, pHost, strlen(szPostData));

	delete[] pHost;
	delete[] pPath;

	int iSendSize = strlen(szHTTPQuery) + strlen(szPostData);

	SOCKET Socket = Connect(szHost, 80);

	if (Socket == INVALID_SOCKET)
		return -1;
	
	if (Send(Socket, szHTTPQuery, szPostData) != iSendSize)
	{
		closesocket(Socket);
		return -1;
	}

	int iResultCode = Recv(Socket, szRecvData, 1400);
	
	closesocket(Socket);
	return iResultCode;

}

bool UTF8toUTF16(const char *szText, WCHAR *szBuff, int iBuffLen)
{
	int iRe = MultiByteToWideChar(CP_UTF8, 0, szText, strlen(szText), szBuff, iBuffLen);
	if (iRe < iBuffLen)
		szBuff[iRe] = L'\0';
	return true;
}

bool UTF16toUTF8(char *szText, const WCHAR *szBuff, int iBuffLen)
{
	int iRe = WideCharToMultiByte(CP_UTF8, 0, szBuff, lstrlenW(szBuff), static_cast<LPSTR>(szText), iBuffLen, NULL, NULL);
	if (iRe < iBuffLen)
		szText[iRe] = '\0';
	return true;
}