#pragma once

#include <Windows.h>

class CParser
{
private:
	WCHAR *buffer;
	int fileSize;

	bool SkipNoneCommand(WCHAR **_buff);
	bool GetNextWord(WCHAR **_buff_index, int *ipLength);
	bool isFloat(WCHAR *_buff, int len);
	bool isInt(WCHAR *_buff, int len);
public:
	CParser(WCHAR *fileName);
	~CParser();


	bool GetValue(WCHAR *szName, int *ipValue, WCHAR *part);
	bool GetValue(WCHAR *szName, WCHAR *ipValue, WCHAR *part);
	bool GetValue(WCHAR *szName, float *ipValue, WCHAR *part);

};

