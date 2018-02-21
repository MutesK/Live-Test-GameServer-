#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include "Parser.h"



CParser::CParser(WCHAR *fileName)
{
	FILE *fp;
	_wfopen_s(&fp, fileName, L"rb");

	if (fp != nullptr)
	{
		// ���� ũ�� ������.
		fseek(fp, 0, SEEK_END);
		fileSize = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		buffer = new WCHAR[fileSize + 1];
		fread(buffer, fileSize + 1, 1, fp);
		buffer[fileSize] = L'\0';

		fclose(fp);
	}

}

CParser::~CParser()
{
}

bool CParser::SkipNoneCommand(WCHAR **_buff)
{
	WCHAR *index = *_buff;

	while (true)
	{
		// �ּ� ��ŵ
		if (*index == L'/' && *(index + 1) == L'/')
		{
			while (*index != 0x0d)
				index++;
			index++;
		}

		if (*index == L'/' && *(index + 1) == L'*')
		{
			while ((*index != L'*') && (*(index + 1) != L'/'))
				index++;
			index++;
		}

		// �ݺ��� Ż�� ����
		if (*index == L'\t' || *index == L'"' || *index == 0x20 || *index == L'\"' || *index == L':')
			break;

		if (index - buffer >= fileSize)
			return false;

		index++;
	}

	

	if (index - buffer >= fileSize)
		return false;
	else
	{
		*_buff = index + 1;
		return true;
	}
}

bool CParser::GetNextWord(WCHAR ** _buff, int * ipLength)
{
	if (SkipNoneCommand(_buff))
	{
		WCHAR *bufIndex = *_buff;

		int count = 0;
		if (bufIndex == L'\0')
			return false;

		while (*bufIndex != L'}')
		{
			if (*bufIndex == L'\t' || *bufIndex == 0x20 || *bufIndex == L'\r' || *bufIndex == L'\n')
				break;

			bufIndex++;
			count+=2;
		}

		if (bufIndex - buffer >= fileSize)
			return false;

		*ipLength = count;
		return true;
	}

	return false;

}

bool CParser::isFloat(WCHAR *_buff, int len)
{
	int i = 0;
	int dotcount = 0;
	int numCount = 0;

	if (_buff[0] == L'0')
		return false;

	while (i < len)
	{
		if (_buff[i] >= L'0' && _buff[i] <= L'9')
		{
			i++;
			numCount++;
			continue;
		}
		else if (_buff[i] == L'.')
		{
			i++;
			dotcount++;
			continue;
		}
		else {
			return false;
		}

	}

	if (len - numCount == 1)
		return true;
	else 
		return false;
}

bool CParser::isInt(WCHAR *_buff, int len)
{
	int i = 0;

	if (_buff[0] == L'0')
		return false;

	while (i < len)
	{
		if (_buff[i] >= L'0' && _buff[i] <= L'9')
		{
			i++;
			continue;
		}
		else {
			return false;
		}
	}

	return true;
}

bool CParser::GetValue(WCHAR *szName, int *ipValue, WCHAR *partName)
{
	WCHAR *buff, chWord[256];
	int iLength;

	buff = buffer;

	while (GetNextWord(&buff, &iLength))
	{
		// WORD���ۿ� ã�� �ܾ �����Ѵ�.
		memset(chWord, 0, 256);
		memcpy(chWord, buff, iLength);

		if (*(buff - 1) == L':')
			if (lstrcmpW(partName, chWord) == 0)
				break;
	}

	// ã���� �ϴ´ܾ ���ö����� ���ã�����̹Ƿ�, while������ �˻�
	while (GetNextWord(&buff, &iLength))
	{
		// WORD���ۿ� ã�� �ܾ �����Ѵ�.
		memset(chWord, 0, 256);
		memcpy(chWord, buff, iLength);

		if (*(buff - 1) == L':')
		{
			if (lstrcmpW(partName, chWord) != 0)
				return false;
		}
		// ���ڷ� �Է� ���� �ܾ�� ������ �˻��Ѵ�.
 		 else if (0 == lstrcmpW(szName, chWord))
		{
			// �´ٸ� �ٷ� �ڿ� =�� ã��.
			if (GetNextWord(&buff, &iLength))
			{
				memset(chWord, 0, 256);
				memcpy(chWord, buff, iLength);

				if (0 == lstrcmpW(chWord, L"="))
				{
					// ���� �κ��� ������ �κ��� ����.
					if (GetNextWord(&buff, &iLength))
					{
						if (buff[0] != L'\"')
						{
							memset(chWord, 0, 256);
							memcpy(chWord, buff, iLength);
							if (isInt(chWord, iLength / 2))
							{
								*ipValue = _wtoi(chWord);
								return true;
							}
							return false;
						}
						return false;
					}

					return false;
				}
			}
			return false;
		}
	}
	return false;
}
bool CParser::GetValue(WCHAR *szName, WCHAR *ipValue, WCHAR *partName)
{
	WCHAR *buff, chWord[256];
	int iLength;

	buff = buffer;

	while (GetNextWord(&buff, &iLength))
	{
		// WORD���ۿ� ã�� �ܾ �����Ѵ�.
		memset(chWord, 0, 256);
		memcpy(chWord, buff, iLength);

		if (*(buff - 1) == L':')
			if (lstrcmpW(partName, chWord) == 0)
				break;
	}

	// ã���� �ϴ´ܾ ���ö����� ���ã�����̹Ƿ�, while������ �˻�
	while (GetNextWord(&buff, &iLength))
	{
		// WORD���ۿ� ã�� �ܾ �����Ѵ�.
		memset(chWord, 0, 256);
		memcpy(chWord, buff, iLength);
		
		if (*(buff - 1) == L':')
		{
			if (lstrcmpW(partName, chWord) != 0)
				return false;
		}
		// ���ڷ� �Է� ���� �ܾ�� ������ �˻��Ѵ�.
		else if (0 == lstrcmpW(szName, chWord))
		{
			// �´ٸ� �ٷ� �ڿ� =�� ã��.
			if (GetNextWord(&buff, &iLength))
			{
				memset(chWord, 0, 256);
				memcpy(chWord, buff, iLength);

				if (0 == lstrcmpW(chWord, L"="))
				{
					// ���� �κ��� ������ �κ��� ����.
					if (GetNextWord(&buff, &iLength))
					{
						if (buff[0] == L'\"')
						{
							memset(chWord, 0, 256);
							memcpy(chWord, buff + 1, iLength);
							if (!isInt(chWord, iLength / 2))
							{
								if (!isFloat(chWord, iLength / 2))
								{
									chWord[iLength / 2 - 2] = L'\0';
									lstrcpynW(ipValue, chWord, iLength / 2);
									return true;
								}
							}
							return false;
						}
						return false;
					}

					return false;
				}
			}
			return false;
		}
	}
	return false;
}
bool CParser::GetValue(WCHAR *szName, float *ipValue, WCHAR *partName)
{
	WCHAR *buff, chWord[256];
	int iLength;

	buff = buffer;

	while (GetNextWord(&buff, &iLength))
	{
		// WORD���ۿ� ã�� �ܾ �����Ѵ�.
		memset(chWord, 0, 256);
		memcpy(chWord, buff, iLength);

		if (*(buff - 1) == L':')
			if (lstrcmpW(partName, chWord) == 0)
				break;
	}

	// ã���� �ϴ´ܾ ���ö����� ���ã�����̹Ƿ�, while������ �˻�
	while (GetNextWord(&buff, &iLength))
	{
		// WORD���ۿ� ã�� �ܾ �����Ѵ�.
		memset(chWord, 0, 256);
		memcpy(chWord, buff, iLength);

		if (*(buff - 1) == L':')
		{
			if (lstrcmpW(partName, chWord) != 0)
				return false;
		}
		// ���ڷ� �Է� ���� �ܾ�� ������ �˻��Ѵ�.
		 else if (0 == lstrcpyW(szName, chWord))
		{
			// �´ٸ� �ٷ� �ڿ� =�� ã��.
			if (GetNextWord(&buff, &iLength))
			{
				memset(chWord, 0, 256);
				memcpy(chWord, buff, iLength);

				if (0 == lstrcpyW(chWord, L"="))
				{
					// ���� �κ��� ������ �κ��� ����.
					if (GetNextWord(&buff, &iLength))
					{
						if (buff[0] != L'\"')
						{

							memset(chWord, 0, 256);
							memcpy(chWord, buff, iLength);
							if (isFloat(chWord, iLength / 2))
							{
								*ipValue = _wtof(chWord);
								return true;
							}
							return false;
						}
						return false;
					}

					return false;
				}
			}
			return false;
		}
	}

	return false;
}
