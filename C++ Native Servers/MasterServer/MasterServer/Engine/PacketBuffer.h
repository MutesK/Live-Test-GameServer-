#pragma once

#include "UpdateTime.h"
#include "MemoryPoolTLS.h"

/*---------------------------------------------------------------
Made By Mute

네트워크 패킷용 클래스. (직렬화 버퍼)
간편하게 패킷에 순서대로 데이타를 In, Out 한다.

- 사용법.

CPacketBuffer cPacket;

넣기.
clPacket << 40030;	or	clPacket << iValue;	(int 넣기)
clPacket << 3;		or	clPacket << byValue;	(BYTE 넣기)
clPacket << 1.4;	or	clPacket << fValue;	(float 넣기)

빼기.
clPacket >> iValue;	(int 빼기)
clPacket >> byValue;	(BYTE 빼기)
clPacket >> fValue;	(float 빼기)

!.	삽입되는 데이타 FIFO 순서로 관리된다.
큐가 아니므로, 넣기(<<).빼기(>>) 를 혼합해서 사용하면 안된다.


==================================================================

현재 CPacket 은 헤더영역 eHEADER_SIZE 를 기본으로 내장하고 있으며 다음의 헤더 구조를 가지도록 만들어져 있음
이는 Encode, Decode 함수를 호출할 때 진행됨.

Encode, Decode 를 호출하지 않는다면 사용되지 않음.

CommonSize = NetServer Class 기준
ShortSize = LanServer Class 기준
해당 Header Alloc 할시에 정해지고 한번 정해진 객체는
다시 정할수 없다. 정할려고 하면 에러발생

----------------------------------------------------------------*/

#pragma pack(push, 1)
struct st_PACKET_HEADER
{
	BYTE	byCode;
	WORD	shLen;
	BYTE	RandKey;
	BYTE	CheckSum;
};
#pragma pack(pop)

class CPacketBuffer
{
private:

	/*---------------------------------------------------------------

	----------------------------------------------------------------*/
	enum en_PACKET
	{
		eBUFFER_DEFAULT = 2920,		// 패킷의 기본 버퍼 사이즈.
		eHEADER_SIZE = 5
	};

	//////////////////////////////////////////////////////////////////////////
	// 패킷  파괴.
	//
	// Parameters: 없음.
	// Return: 없음.
	//////////////////////////////////////////////////////////////////////////
	void	Release(void);

	//////////////////////////////////////////////////////////////////////////
	// 버퍼 사이즈 얻기.
	//
	// Parameters: 없음.
	// Return: (int)패킷 버퍼 사이즈 얻기.
	//////////////////////////////////////////////////////////////////////////
	int		GetBufferSize(void) { return m_iBufferSize; }


	int		GetHeaderDataUseSize(void) {
		return m_iDataSize + m_iHeaderSize;
	}



	//////////////////////////////////////////////////////////////////////////
	// 헤더 및 버퍼 셋팅 함수
	//////////////////////////////////////////////////////////////////////////
	void CommonSizeHeaderAlloc(st_PACKET_HEADER* pOutHeader, 
		BYTE PacketCode, BYTE PacketKey1, BYTE PacketKey2); // NetServer Class 전용
	void ShortSizeHeaderAlloc();		// LanServer Class 전용

	void SettingBuffer(int Size = eBUFFER_DEFAULT);

	BYTE MakeCheckSum();

	bool Decode();
	void Encode(st_PACKET_HEADER *pHeader);

private:
	unsigned __int64  BufferInit;
	//------------------------------------------------------------
	// 패킷버퍼 / 버퍼 사이즈.
	//------------------------------------------------------------
	char*	m_chpBuffer;
	int		m_iBufferSize;

	//------------------------------------------------------------
	// 버퍼의 읽을 위치, 넣을 위치.
	//------------------------------------------------------------
	char	*m_chpReadPos;
	char	*m_chpWritePos;

	//------------------------------------------------------------
	// 현재 버퍼에 사용중인 사이즈.
	//------------------------------------------------------------
	int		m_iDataSize;


	//------------------------------------------------------------
	// 직렬화 버퍼용 레퍼런스 카운트
	//------------------------------------------------------------
	LONG64	m_ReferenceCount;


	//------------------------------------------------------------
	// 패킷 헤더 셋팅용 변수
	//------------------------------------------------------------
	LONG	EncodeFlag;
	LONG	HeaderFlag;
	LONG	DecodeFlag;


	 BYTE m_PacketCode;
	 BYTE m_PacketKey1;
	 BYTE m_PacketKey2;
	 BYTE m_iHeaderSize;




	bool	m_bAlloced;  // 디버그용
	DWORD   ErrorCode;

	friend class CNetServer;
	friend class CLanServer;
	friend class CLanClient;
	friend class CMMOServer;
	friend class CNetSession;

	/////////////////////////////////////////////////////////////////////////////////////////
public:
	static LONG64 AllocCount;
	//////////////////////////////////////////////////////////////////////////
	// 생성자, 파괴자.
	//
	// Return:
	//////////////////////////////////////////////////////////////////////////
	CPacketBuffer();
	CPacketBuffer(int iBufferSize);
	~CPacketBuffer();

	//////////////////////////////////////////////////////////////////////////
	// 레퍼런스 카운트 증가
	//////////////////////////////////////////////////////////////////////////
	void AddRefCount();

	//////////////////////////////////////////////////////////////////////////
	// 레퍼런스 카운트 감소하고 만약 0이되면 해제하는 함수
	//////////////////////////////////////////////////////////////////////////
	void Free();

	//////////////////////////////////////////////////////////////////////////
	// 버퍼 포인터 얻기.
	//
	// Parameters: 없음.
	// Return: (char *)버퍼 포인터.
	//////////////////////////////////////////////////////////////////////////
	char	*GetBufferPtr(void) { return m_chpBuffer; }


	char	*GetReadBufferPtr(void) { return m_chpReadPos; }

	char	*GetWriteBufferPtr(void) { return m_chpWritePos; }

	char	*GetHeaderPtr(void) { return GetBufferPtr() + (5 - m_iHeaderSize); }


	/* ============================================================================= */
	// 연산자 오퍼레이터.
	/* ============================================================================= */
	CPacketBuffer	&operator = (CPacketBuffer &clSrCPacketBuffer);

	template<typename T>
	CPacketBuffer& operator<<(T& Value)
	{
		PutData(reinterpret_cast<char *>(&Value), sizeof(T));
		return *this;
	}


	template<typename T>
	CPacketBuffer& operator >> (T& Value)
	{
		GetData(reinterpret_cast<char *>(&Value), sizeof(T));
		return *this;
	}

	//////////////////////////////////////////////////////////////////////////
	// 현재 사용중인 사이즈 얻기.
	//
	// Parameters: 없음.
	// Return: (int)사용중인 데이타 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int		GetUseSize(void) { return m_iDataSize; }

	//////////////////////////////////////////////////////////////////////////
	// 데이타 얻기.
	//
	// Parameters: (char *)Dest 포인터. (int)Size.
	// Return: (int)복사한 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int		GetData(char *chpDest, int iSize);

	//////////////////////////////////////////////////////////////////////////
	// 데이타 삽입.
	//
	// Parameters: (char *)Src 포인터. (int)SrcSize.
	// Return: (int)복사한 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int		PutData(char *chpSrc, int iSrcSize);


	int		PeekData(char *chrSrc, int iSrcSize);

	//////////////////////////////////////////////////////////////////////////
	// 패킷 청소.
	//
	// Parameters: 없음.
	// Return: 없음.
	//////////////////////////////////////////////////////////////////////////
	void	ClearBuffer(void);

	DWORD GetLastError()
	{
		return ErrorCode;
	}



	//////////////////////////////////////////////////////////////////////////
	// 버퍼 Pos 이동. (음수이동은 안됨)
	// GetBufferPtr 함수를 이용하여 외부에서 강제로 버퍼 내용을 수정할 경우 사용. 
	//
	// Parameters: (int) 이동 사이즈.
	// Return: (int) 이동된 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int		MoveWritePos(int iSize);
	int		MoveReadPos(int iSize);


	static CMemoryPoolTLS<CPacketBuffer> PacketPool;

	static CPacketBuffer* Alloc()
	{
		CPacketBuffer * pRet = PacketPool.Alloc();
		pRet->SettingBuffer();

		return pRet;
	}

	static void Free(CPacketBuffer *pBuffer)
	{
		pBuffer->Free();
	}

	static LONG64 GetAllocCount()
	{
		return AllocCount;
	}
};


