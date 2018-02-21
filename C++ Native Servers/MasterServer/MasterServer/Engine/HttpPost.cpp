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
// HTTP ����
/////////////////////////////////////////////////////
SOCKET Connect(WCHAR *szHost, int iPort)
{
	SOCKADDR_IN SockAddr;
	DWORD dwResult;

	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	// TCP ���� ����
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

	// ������ ����
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
// Send ����
/////////////////////////////////////////////////////
int Send(SOCKET Socket, const char *szQuery, const char *szPostData)
{
	int iTotalSend = 0;
	int iSend = 0;
	int iResult = 0;
	int iLen = strlen(szQuery);


	// HTTP ��� ����
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

	// POST ������ ����
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
	int iBufferSize = iMaxSize + 500; // �ִ� ����� ������ HTTP ��� ����ũ�� ����
	char *pRecvBuff = new char[iBufferSize + 1]; // ���۴� ���ڿ��� �ǹǷ� ��ü�� ���� ���� ����Ͽ� �ϳ� �����
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

		// HTTP ��� ó��
		if (bHeader)
		{
			// ù 0x20�� ã�Ƽ� ����ڵ带 ��´�.  HTTP/1.1 200 xxxx
			Content = strchr(pRecvBuff, 0x20);

			if (Content != nullptr)
			{
				Code = strchr(Content + 1, 0x20);

				if (Code != nullptr)
				{
					// ��� �ڵ常 ���ڿ��� ����� ���� �ڿ� null�� �־��ٰ� �ٽ� 0x20�� �־� �����Ѵ�.
					*Code = NULL;
					iCode = atoi(Content + 1);
					*Code = 0x20;
				}
			}

			// Content-Length �� ã�Ƽ� ������ ���̸� ���Ѵ�.
			Content = strstr(pRecvBuff, "Content-Length:");
			if (Content != nullptr)
			{
				Code = strchr(Content + 15, 0x0d);

				if (Code != nullptr)
				{
					// ��� �ڵ常 ���ڿ��� ����� ���� �ڿ� null�� �־��ٰ� �ٽ� 0x20�� �־� �����Ѵ�.
					*Code = NULL;
					iContentLen = atoi(Content + 15);
					*Code = 0x0d;
				}
			}


			Content = strstr(pRecvBuff, "\r\n\r\n");
			if (Content != nullptr)
			{
				Content += 4;

				// ��������� ���
				iHeaderLen = Content - pRecvBuff;

				// ��óó�� ��
				bHeader = false;
			}
		}



		// ���������� �� �޾Ѵٸ� ����.
		if (iRecv >= iHeaderLen + iContentLen)
			break;
	}

	// ���������� szRecvData�� �����Ѵ�.
	memcpy(szRecvData, Content, iContentLen);
	delete[] pRecvBuff;

	return iCode;
}

//////////////////////////////////////////////////////////////////////
// HTTP Post ��û
//
// POST DATA���� ANSICODE �Ǵ� UTF-8 �����Ͱ� �;� ��.
//////////////////////////////////////////////////////////////////////
int HTTP_POST(WCHAR *szHost, WCHAR *szPath, const char *szPostData, char *szRecvData)
{
	char szHTTPQuery[1024] = { 0, };

	// http://�� ���� �޴´�.
	if (NULL != wcsstr(szHost, L"http://"))
		return -1;

	// ȣ��Ʈ�� �н��� char �����ͷ� ��ȯ
	int iHostLen = (int)wcslen(szHost);
	char *pHost = new char[iHostLen * 2 + 1];
	memset(pHost, 0, iHostLen * 2 + 1);
	WideCharToMultiByte(CP_UTF8, 0, szHost, iHostLen, pHost, iHostLen * 2 + 1, NULL, NULL);

	int iPathLen = (int)wcslen(szPath);
	char *pPath = new char[iPathLen * 2 + 1];
	memset(pPath, 0, iPathLen * 2 + 1);
	WideCharToMultiByte(CP_UTF8, 0, szPath, iPathLen, pPath, iPathLen * 2 + 1, NULL, NULL);

	// HTTP Post ����� �����.
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