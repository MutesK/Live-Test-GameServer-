#pragma once

#include "UpdateTime.h"
#include "MemoryPoolTLS.h"

/*---------------------------------------------------------------
Made By Mute

��Ʈ��ũ ��Ŷ�� Ŭ����. (����ȭ ����)
�����ϰ� ��Ŷ�� ������� ����Ÿ�� In, Out �Ѵ�.

- ����.

CPacketBuffer cPacket;

�ֱ�.
clPacket << 40030;	or	clPacket << iValue;	(int �ֱ�)
clPacket << 3;		or	clPacket << byValue;	(BYTE �ֱ�)
clPacket << 1.4;	or	clPacket << fValue;	(float �ֱ�)

����.
clPacket >> iValue;	(int ����)
clPacket >> byValue;	(BYTE ����)
clPacket >> fValue;	(float ����)

!.	���ԵǴ� ����Ÿ FIFO ������ �����ȴ�.
ť�� �ƴϹǷ�, �ֱ�(<<).����(>>) �� ȥ���ؼ� ����ϸ� �ȵȴ�.


==================================================================

���� CPacket �� ������� eHEADER_SIZE �� �⺻���� �����ϰ� ������ ������ ��� ������ �������� ������� ����
�̴� Encode, Decode �Լ��� ȣ���� �� �����.

Encode, Decode �� ȣ������ �ʴ´ٸ� ������ ����.

CommonSize = NetServer Class ����
ShortSize = LanServer Class ����
�ش� Header Alloc �ҽÿ� �������� �ѹ� ������ ��ü��
�ٽ� ���Ҽ� ����. ���ҷ��� �ϸ� �����߻�

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
		eBUFFER_DEFAULT = 2920,		// ��Ŷ�� �⺻ ���� ������.
		eHEADER_SIZE = 5
	};

	//////////////////////////////////////////////////////////////////////////
	// ��Ŷ  �ı�.
	//
	// Parameters: ����.
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	void	Release(void);

	//////////////////////////////////////////////////////////////////////////
	// ���� ������ ���.
	//
	// Parameters: ����.
	// Return: (int)��Ŷ ���� ������ ���.
	//////////////////////////////////////////////////////////////////////////
	int		GetBufferSize(void) { return m_iBufferSize; }


	int		GetHeaderDataUseSize(void) {
		return m_iDataSize + m_iHeaderSize;
	}



	//////////////////////////////////////////////////////////////////////////
	// ��� �� ���� ���� �Լ�
	//////////////////////////////////////////////////////////////////////////
	void CommonSizeHeaderAlloc(st_PACKET_HEADER* pOutHeader, 
		BYTE PacketCode, BYTE PacketKey1, BYTE PacketKey2); // NetServer Class ����
	void ShortSizeHeaderAlloc();		// LanServer Class ����

	void SettingBuffer(int Size = eBUFFER_DEFAULT);

	BYTE MakeCheckSum();

	bool Decode();
	void Encode(st_PACKET_HEADER *pHeader);

private:
	unsigned __int64  BufferInit;
	//------------------------------------------------------------
	// ��Ŷ���� / ���� ������.
	//------------------------------------------------------------
	char*	m_chpBuffer;
	int		m_iBufferSize;

	//------------------------------------------------------------
	// ������ ���� ��ġ, ���� ��ġ.
	//------------------------------------------------------------
	char	*m_chpReadPos;
	char	*m_chpWritePos;

	//------------------------------------------------------------
	// ���� ���ۿ� ������� ������.
	//------------------------------------------------------------
	int		m_iDataSize;


	//------------------------------------------------------------
	// ����ȭ ���ۿ� ���۷��� ī��Ʈ
	//------------------------------------------------------------
	LONG64	m_ReferenceCount;


	//------------------------------------------------------------
	// ��Ŷ ��� ���ÿ� ����
	//------------------------------------------------------------
	LONG	EncodeFlag;
	LONG	HeaderFlag;
	LONG	DecodeFlag;


	 BYTE m_PacketCode;
	 BYTE m_PacketKey1;
	 BYTE m_PacketKey2;
	 BYTE m_iHeaderSize;




	bool	m_bAlloced;  // ����׿�
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
	// ������, �ı���.
	//
	// Return:
	//////////////////////////////////////////////////////////////////////////
	CPacketBuffer();
	CPacketBuffer(int iBufferSize);
	~CPacketBuffer();

	//////////////////////////////////////////////////////////////////////////
	// ���۷��� ī��Ʈ ����
	//////////////////////////////////////////////////////////////////////////
	void AddRefCount();

	//////////////////////////////////////////////////////////////////////////
	// ���۷��� ī��Ʈ �����ϰ� ���� 0�̵Ǹ� �����ϴ� �Լ�
	//////////////////////////////////////////////////////////////////////////
	void Free();

	//////////////////////////////////////////////////////////////////////////
	// ���� ������ ���.
	//
	// Parameters: ����.
	// Return: (char *)���� ������.
	//////////////////////////////////////////////////////////////////////////
	char	*GetBufferPtr(void) { return m_chpBuffer; }


	char	*GetReadBufferPtr(void) { return m_chpReadPos; }

	char	*GetWriteBufferPtr(void) { return m_chpWritePos; }

	char	*GetHeaderPtr(void) { return GetBufferPtr() + (5 - m_iHeaderSize); }


	/* ============================================================================= */
	// ������ ���۷�����.
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
	// ���� ������� ������ ���.
	//
	// Parameters: ����.
	// Return: (int)������� ����Ÿ ������.
	//////////////////////////////////////////////////////////////////////////
	int		GetUseSize(void) { return m_iDataSize; }

	//////////////////////////////////////////////////////////////////////////
	// ����Ÿ ���.
	//
	// Parameters: (char *)Dest ������. (int)Size.
	// Return: (int)������ ������.
	//////////////////////////////////////////////////////////////////////////
	int		GetData(char *chpDest, int iSize);

	//////////////////////////////////////////////////////////////////////////
	// ����Ÿ ����.
	//
	// Parameters: (char *)Src ������. (int)SrcSize.
	// Return: (int)������ ������.
	//////////////////////////////////////////////////////////////////////////
	int		PutData(char *chpSrc, int iSrcSize);


	int		PeekData(char *chrSrc, int iSrcSize);

	//////////////////////////////////////////////////////////////////////////
	// ��Ŷ û��.
	//
	// Parameters: ����.
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	void	ClearBuffer(void);

	DWORD GetLastError()
	{
		return ErrorCode;
	}



	//////////////////////////////////////////////////////////////////////////
	// ���� Pos �̵�. (�����̵��� �ȵ�)
	// GetBufferPtr �Լ��� �̿��Ͽ� �ܺο��� ������ ���� ������ ������ ��� ���. 
	//
	// Parameters: (int) �̵� ������.
	// Return: (int) �̵��� ������.
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


