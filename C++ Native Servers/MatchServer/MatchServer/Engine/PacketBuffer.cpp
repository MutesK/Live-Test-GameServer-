
#include "PacketBuffer.h"
#include "CSystemLog.h"

CMemoryPoolTLS<CPacketBuffer> CPacketBuffer::PacketPool;


LONG64 CPacketBuffer::AllocCount;

CPacketBuffer::CPacketBuffer()
{
	m_chpBuffer = new char[eBUFFER_DEFAULT];
	ZeroMemory(m_chpBuffer, eBUFFER_DEFAULT);
	m_iDataSize = 0;
	m_iBufferSize = eBUFFER_DEFAULT;

	m_chpReadPos = m_chpBuffer;
	m_chpWritePos = m_chpBuffer;
	m_ReferenceCount = 1;
}

CPacketBuffer::CPacketBuffer(int iBufferSize)
{
	m_chpBuffer = new char[iBufferSize];
	ZeroMemory(m_chpBuffer, iBufferSize);
	m_iDataSize = 0;
	m_iBufferSize = iBufferSize;


	m_chpReadPos = m_chpBuffer;
	m_chpWritePos = m_chpBuffer;
	m_ReferenceCount = 1;
}

CPacketBuffer::~CPacketBuffer()
{
	Release();
}


void CPacketBuffer::Release(void)
{
	ClearBuffer();
}
void	CPacketBuffer::ClearBuffer(void)
{
	m_iDataSize = 0;

	m_chpReadPos = m_chpBuffer + eHEADER_SIZE;
	m_chpWritePos = m_chpBuffer + eHEADER_SIZE;
	EncodeFlag = 0;
	HeaderFlag = 0;
	DecodeFlag = 0;
	ErrorCode = 0;
	m_iHeaderSize = 2;
	m_ReferenceCount = 1;

}

int	CPacketBuffer::MoveWritePos(int iSize)
{
	if (iSize < 0)
		return 0;

	int Usage = m_iBufferSize - (m_chpWritePos - m_chpReadPos);
	int CopiedSize = max(min(iSize, Usage), 0);
	m_chpWritePos += CopiedSize;
	m_iDataSize += CopiedSize;

	return CopiedSize;
}

int	CPacketBuffer::MoveReadPos(int iSize)
{
	if (iSize < 0)
		return 0;

	if (iSize > m_iDataSize)
		return 0;

	m_chpReadPos += iSize;
	m_iDataSize -= iSize;

	if (m_iDataSize == 0)
		ClearBuffer();

	return iSize;
}

int	CPacketBuffer::PeekData(char *chrSrc, int iSrcSize)
{
	int Usage = m_chpWritePos - m_chpReadPos;
	if (Usage == 0)
		return 0;

	int GetDataSize = max(min(iSrcSize, Usage), 0);

	memcpy_s(chrSrc, GetDataSize, m_chpReadPos, GetDataSize);

	return GetDataSize;
}
int	CPacketBuffer::GetData(char *chpDest, int iSize)
{
	if (iSize > m_iDataSize)
	{
		ErrorCode = 5;
		return 0;
	}
	memcpy(chpDest, m_chpReadPos, iSize);
	m_chpReadPos += iSize;
	m_iDataSize -= iSize;


	return iSize;
}

int	CPacketBuffer::PutData(char *chpSrc, int iSrcSize)
{
	if (m_chpWritePos + iSrcSize > m_chpBuffer + m_iBufferSize)
	{
		ErrorCode = 6;
		return 0;
	}

	memcpy(m_chpWritePos, chpSrc, iSrcSize);
	m_chpWritePos += iSrcSize;
	m_iDataSize += iSrcSize;

	return iSrcSize;
}

CPacketBuffer& CPacketBuffer::operator=(CPacketBuffer &clSrCPacketBuffer)
{
	if (m_chpBuffer != nullptr)
	{
		delete[] m_chpBuffer;
		m_chpBuffer = nullptr;
	}

	this->m_iDataSize = clSrCPacketBuffer.m_iDataSize;
	this->m_iBufferSize = clSrCPacketBuffer.m_iBufferSize;

	this->m_chpBuffer = new char[m_iBufferSize];

	memcpy(this->m_chpBuffer, clSrCPacketBuffer.m_chpBuffer, m_iBufferSize);

	int ReadIndex = clSrCPacketBuffer.m_chpReadPos - clSrCPacketBuffer.m_chpBuffer;
	this->m_chpReadPos = m_chpBuffer + ReadIndex;

	int WriteIndex = clSrCPacketBuffer.m_chpWritePos - clSrCPacketBuffer.m_chpBuffer;
	this->m_chpWritePos = m_chpBuffer + WriteIndex;

	return *this;
}

void CPacketBuffer::AddRefCount()
{
	if (m_ReferenceCount <= 0)
	{
		ErrorCode = 1;
		return;
	}

	InterlockedIncrement64(&m_ReferenceCount);
}

void CPacketBuffer::Free()
{
	if (m_ReferenceCount <= 0)
	{
		ErrorCode = 2;
		return;
	}

	if (InterlockedDecrement64(&m_ReferenceCount) == 0)
	{
		AllocCount--;
		m_bAlloced = false;
		PacketPool.Free(this);
	}
}


void CPacketBuffer::CommonSizeHeaderAlloc(st_PACKET_HEADER* pOutHeader,
	BYTE PacketCode, BYTE PacketKey1, BYTE PacketKey2)
{
	if (m_chpBuffer == nullptr)
		return;

	m_PacketCode = PacketCode;
	m_PacketKey1 = PacketKey1;
	m_PacketKey2 = PacketKey2;
	m_iHeaderSize = 5;

	if (InterlockedAdd(&HeaderFlag, 1) == 1)
	{
		if (InterlockedAdd(&HeaderFlag, 1) == 2)
		{

			pOutHeader->byCode = PacketCode;
			pOutHeader->shLen = m_iDataSize;

			// 랜덤 XOR Code 생성
			srand((int)CUpdateTime::GetTickCount());
			pOutHeader->RandKey = rand() % 255 + 1;

			// CheckSum 계산
			BYTE checkSum = MakeCheckSum();
			pOutHeader->CheckSum = checkSum;


			if (HeaderFlag == 2)
				memcpy(m_chpBuffer, pOutHeader, sizeof(st_PACKET_HEADER));
			else
				pOutHeader = nullptr;

			return;
		}
		else
			InterlockedAdd(&HeaderFlag, -1);

	}
	else
		InterlockedAdd(&HeaderFlag, -1);

	pOutHeader = nullptr;

	return;
}
void CPacketBuffer::ShortSizeHeaderAlloc()
{
	if (m_chpBuffer == nullptr)
		return;
	
	m_iHeaderSize = 2;

	if (InterlockedAdd(&HeaderFlag, 1) == 1)
	{
		if (InterlockedAdd(&HeaderFlag, 1) == 2)
		{
			char *pIndex = GetHeaderPtr();
			short DataSize = m_iDataSize;

			if (HeaderFlag == 2)
				memcpy(pIndex, reinterpret_cast<char *>(&DataSize), sizeof(short));
		}
	}
}


BYTE CPacketBuffer::MakeCheckSum()
{
	char *pIndex = m_chpReadPos;
	unsigned long long checkSum = 0;

	for (int i = 0; i < m_iDataSize; i++)
	{
		checkSum += *pIndex;
		pIndex++;
	}

	checkSum %= 256;

	return checkSum;
}
void CPacketBuffer::Encode(st_PACKET_HEADER *pHeader)
{
	char *p = m_chpBuffer;

	if (pHeader == nullptr)
		return;

	if (*p != m_PacketCode)
	{
		ErrorCode = 3;
		return;
	}

	if (m_iHeaderSize != 5)
	{
		ErrorCode = 4;
		return;
	}

	if (InterlockedAdd(&EncodeFlag, 1) == 1)
	{
		// 이미 헤더를 위에서 넣었다. 구조체로 빼서 하던가(복사가 일어남) 
		// 바로 headerPos로 작업하거나
		if (InterlockedAdd(&EncodeFlag, 1) == 2)
		{
			////////////////////////////////////////////////////////////////////////////
			// 1. Rand XOR Code로 CheckSum, Payload를 XOR한다.

			BYTE RandKey = *(m_chpBuffer + 3);
			char *pIndex = m_chpBuffer + 4;
			for (int i = 4; i < m_iDataSize + 5; i++)
			{
				*pIndex ^= RandKey;
				pIndex++;
			}

			// 2. 고정 Packet Key1로 Rand XOR Code, CheckSum, Payload를 XOR한다.
			pIndex = m_chpBuffer + 3;
			for (int i = 3; i < m_iDataSize + 5; i++)
			{
				*pIndex ^= m_PacketKey1;
				pIndex++;
			}

			// 3. 고정 Packet Key2로 Rand XOR Code, CheckSum, Payload를 XOR한다.
			pIndex = m_chpBuffer + 3;
			for (int i = 3; i < m_iDataSize + 5; i++)
			{
				*pIndex ^= m_PacketKey2;
				pIndex++;
			}
			return;

		}
		else
			InterlockedAdd(&EncodeFlag, -1);

	}
	else
		InterlockedAdd(&EncodeFlag, -1);

	return;
}

bool CPacketBuffer::Decode()
{
	BYTE CheckSum;
	BYTE CompCheckSum;
	if (InterlockedAdd(&DecodeFlag, 1) == 1)
	{
		if (InterlockedAdd(&DecodeFlag, 1) == 2)
		{
			// 이미 패킷에 다 들어있다.
			// 1. Packey Key2로 RAND XOR Code, CheckSum, Payload를 XOR한다.
			char *pIndex = m_chpBuffer + 3;
			for (int i = 3; i < m_iDataSize + 5; i++)
			{
				*pIndex ^= m_PacketKey2;
				pIndex++;
			}
			// 2. Packet Key1로 RAND XOR Code, CheckSum, Payload를 XOR한다.
			pIndex = m_chpBuffer + 3;
			for (int i = 3; i < m_iDataSize + 5; i++)
			{
				*pIndex ^= m_PacketKey1;
				pIndex++;
			}

			BYTE RandKey = *(m_chpBuffer + 3);
			pIndex = m_chpBuffer + 4;
			for (int i = 4; i < m_iDataSize + 5; i++)
			{
				*pIndex ^= RandKey;
				pIndex++;
			}
			// 검증 작업
			// CheckSum 계산후 패킷의 CheckSum과 비교한다.
			CheckSum = MakeCheckSum();
			CompCheckSum = *(m_chpBuffer + 4);

			if (CheckSum == CompCheckSum)
				return true;
		}

	}

	return false;
}

void CPacketBuffer::SettingBuffer(int Size)
{
	m_bAlloced = true;

	if (BufferInit == 0xAAAABBBBDDDDFFFF)  // P1. Not Initialize Variable
	{
		ClearBuffer();
		return;
	}

	BufferInit = 0xAAAABBBBDDDDFFFF;
	m_chpBuffer = new char[Size];
	ZeroMemory(m_chpBuffer, Size);

	m_iBufferSize = Size;
	ClearBuffer();
}


