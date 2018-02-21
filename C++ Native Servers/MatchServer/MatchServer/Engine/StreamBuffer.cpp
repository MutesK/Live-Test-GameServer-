#include <iostream>
#include "StreamBuffer.h"


CStreamBuffer::CStreamBuffer()
	:m_front(0), m_rear(0), m_bufferSize(enBUFFER_SIZE)
{
	m_buffer = new char[enBUFFER_SIZE];
	memset(m_buffer, 0, enBUFFER_SIZE);

	InitializeCriticalSection(&RQueue);
}

CStreamBuffer::CStreamBuffer(int iSize)
	:m_bufferSize(iSize), m_front(0), m_rear(0)
{
	m_buffer = new char[iSize];
	memset(m_buffer, 0, iSize);

	InitializeCriticalSection(&RQueue);
}


CStreamBuffer::~CStreamBuffer()
{
	delete[] m_buffer;
	DeleteCriticalSection(&RQueue);
}

int	CStreamBuffer::GetBufferSize(void)
{
	return m_bufferSize - enBUFFER_BLANK;
}

/////////////////////////////////////////////////////////////////////////
// 현재 사용중인 용량 얻기.
//
// Parameters: 없음.
// Return: (int)사용중인 용량.
/////////////////////////////////////////////////////////////////////////
int	CStreamBuffer::GetUseSize(void)
{
	if (m_rear >= m_front)
		return m_rear - m_front;
	else
		return m_bufferSize - m_front + m_rear;
}

/////////////////////////////////////////////////////////////////////////
// 현재 버퍼에 남은 용량 얻기.
//
// Parameters: 없음.
// Return: (int)남은용량.
/////////////////////////////////////////////////////////////////////////
int	CStreamBuffer::GetFreeSize(void)
{
	return m_bufferSize - GetUseSize() - enBUFFER_BLANK;
}

/////////////////////////////////////////////////////////////////////////
// 버퍼 포인터로 외부에서 한방에 읽고, 쓸 수 있는 길이.
// (끊기지 않은 길이)
//
// Parameters: 없음.
// Return: (int)사용가능 용량.
////////////////////////////////////////////////////////////////////////
int	CStreamBuffer::GetNotBrokenGetSize(void)
{
	if (m_front <= m_rear)
	{
		return m_rear - m_front;
	}
	else
		return m_bufferSize - m_front;
}
int	CStreamBuffer::GetNotBrokenPutSize(void)
{
	if (m_rear < m_front)
	{
		return m_front - m_rear - enBUFFER_BLANK;
	}
	else
	{
		if (m_front < enBUFFER_BLANK)
			return m_bufferSize - m_rear - enBUFFER_BLANK + m_front;
		else
			return m_bufferSize - m_rear;
	}
}

/////////////////////////////////////////////////////////////////////////
// WritePos 에 데이타 넣음.
//
// Parameters: (char *)데이타 포인터. (int)크기. 
// Return: (int)넣은 크기.
/////////////////////////////////////////////////////////////////////////
int	CStreamBuffer::PutData(char *chpData, int iSize)
{
	int inputSize = 0;

	if (iSize > GetFreeSize())
		return 0;

	if (iSize <= 0)
		return 0;

	if (m_front <= m_rear)
	{
		int WriteSize = m_bufferSize - m_rear;

		if (WriteSize >= iSize)
		{
			memcpy(m_buffer + m_rear, chpData, iSize);
			m_rear += iSize;
		}
		else
		{
			memcpy(m_buffer + m_rear, chpData, WriteSize);
			memcpy(m_buffer, chpData + WriteSize, iSize - WriteSize);
			m_rear = iSize - WriteSize;
		}

	}
	else
	{
		memcpy(m_buffer + m_rear, chpData, iSize);
		m_rear += iSize;
	}

	if (m_rear == m_bufferSize)
		m_rear = 0;
}

/////////////////////////////////////////////////////////////////////////
// ReadPos 에서 데이타 가져옴. ReadPos 이동.
//
// Parameters: (char *)데이타 포인터. (int)크기.
// Return: (int)가져온 크기.
/////////////////////////////////////////////////////////////////////////
int	CStreamBuffer::GetData(char *chpDest, int iSize)
{
	if (iSize > GetUseSize())
		return 0;

	if (iSize <= 0)
		return 0;

	if (m_front <= m_rear)
	{
		memcpy(chpDest, m_buffer + m_front, iSize);
		m_front += iSize;
	}
	else
	{
		int ReadSize = m_bufferSize - m_front;

		if (ReadSize >= iSize)
		{
			memcpy(chpDest, m_buffer + m_front, iSize);
			m_front += iSize;
		}
		else
		{
			memcpy(chpDest, m_buffer + m_front, ReadSize);
			memcpy(chpDest + ReadSize, m_buffer, iSize - ReadSize);
			m_front = iSize - ReadSize;
		}
	}

	if (m_rear == m_front)
		m_rear = m_front = 0;

	return iSize;
}

int	CStreamBuffer::Peek(char *chpDest, int iSize)
{
	if (iSize > GetUseSize())
		return 0;

	if (iSize <= 0)
		return 0;

	if (m_front <= m_rear)
	{
		memcpy(chpDest, m_buffer + m_front, iSize);
	}
	else
	{
		int ReadSize = m_bufferSize - m_front;

		if (ReadSize >= iSize)
		{
			memcpy(chpDest, m_buffer + m_front, iSize);
		}
		else
		{
			memcpy(chpDest, m_buffer + m_front, ReadSize);
			memcpy(chpDest + ReadSize, m_buffer, iSize - ReadSize);
		}
	}

	return iSize;
}

void CStreamBuffer::RemoveData(int iSize)
{
	if (iSize > GetUseSize())
		return;

	if (iSize <= 0)
		return;

	if (m_front <= m_rear)
	{
		m_front += iSize;
	}
	else
	{
		int ReadSize = m_bufferSize - m_front;

		if (ReadSize >= iSize)
		{
			m_front += iSize;
		}
		else
		{
			m_front = iSize - ReadSize;
		}
	}
	if (m_front == m_bufferSize)
		m_front = 0;
}

void CStreamBuffer::MoveWritePos(int iSize)
{
	int inputSize = 0;

	if (iSize > GetFreeSize())
		return;

	if (iSize <= 0)
		return;

	if (m_front <= m_rear)
	{
		int WriteSize = m_bufferSize - m_rear;

		if (WriteSize >= iSize)
		{
			m_rear += iSize;
		}
		else
		{
			m_rear = iSize - WriteSize;
		}

	}
	else
	{
		m_rear += iSize;
	}

	if (m_rear == m_bufferSize)
		m_rear = 0;
}

void CStreamBuffer::ClearBuffer(void)
{
	m_front = 0;
	m_rear = 0;
}

char* CStreamBuffer::GetBufferPtr(void)
{
	return m_buffer;
}

char* CStreamBuffer::GetReadBufferPtr(void)
{
	return m_buffer + m_front;
}


char* CStreamBuffer::GetWriteBufferPtr(void)
{
	return m_buffer + m_rear;
}


void CStreamBuffer::Lock()
{
	EnterCriticalSection(&RQueue);
}
void CStreamBuffer::UnLock()
{
	LeaveCriticalSection(&RQueue);
}

CStreamBuffer	&CStreamBuffer::operator = (CStreamBuffer &clSrCPacketBuffer)
{
	if (m_buffer != nullptr)
	{
		delete[] m_buffer;
		m_buffer = nullptr;
	}

	m_bufferSize = clSrCPacketBuffer.m_bufferSize;

	m_front = clSrCPacketBuffer.m_front;
	m_rear = clSrCPacketBuffer.m_rear;

	m_buffer = new char[m_bufferSize];

	memcpy(m_buffer, clSrCPacketBuffer.m_buffer, sizeof(m_bufferSize));

	return *this;
}