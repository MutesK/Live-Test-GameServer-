#pragma once

#include <Windows.h>
#include <list>
#include "MemoryPool.h"


// dynamic TLS Alloc 을 사용하여 스레드별로 ChunkBlock 를 할당하며
// 이를통해서 메모리를 할당 받음.
//
// - CMemoryPool : 통합 메모리 관리자 (CChunkBlock 을 관리함)
// - CChunkBlock : 실제 사용자용 객체를 관리하는 스레드별 할당 블록
//
//
// 1. MemoryPool 인스턴스가 생성되면 CChunkBlock 을 일정량 확보하여 관리 준비에 들어감.
// 2. MemoryPool 생성자에서 TlsAlloc 을 통해서 메모리풀 전용 TLS 인덱스를 확보.
// 3. 이 TLS 인덱스 영역에 각 스레드용 CChunkBlock 이 담겨진다.
//    만약 한 프로세스에서 여러개의 MemoryPool 을 사용하게 된다면 각자의 TLS Index 를 가지게 됨.
// 4. 스레드에서 MemoryPool.Alloc 호출
// 5. TLS Index 에 CChunkBlock 이 있는지 확인.
// 6. 청크블록이 없다면 (NULL 이라면)  MemoryPool 에서 ChunkBlock 을 할당하여 TLS 에 등록.
// 7. TLS 에 등록된 ChunkBlock 에서 DATA Alloc 후 반환.
// 8. ChunkBlock 에 더 이상 DATA 가 없다면 TLS 를 NULL 로 바꾸어 놓고 종료.
//
// 1. MemoryPool.Free 호출시
// 2. 입력 포인터를 기준으로 ChunkBlock 에 접근.
// 3. ChunkBlock Ref 를 줄임.
// 4. ChunkBlock Ref 가 0 이라면 MemoryPool 로 ChunkBlock 반환.
// -. Free 의 경우는 스레드의 구분이 없으므로 스레드 세이프 하도록 구현.
//
// Alloc 할당은 TLS 에 할당된 CChunkBlock 에서 스레드마다 개별적으로 진행
// Free 해제는 CChunkBlock 레퍼런스 카운터만 줄인 뒤 해당 청크블록에 사용중인 개수가 0 인경우 CMemoryPool 로 반환.
//
// ChunkBlock 은 DATA 의 할당만 가능하며, 반환 후 재사용은 고려하지 않음.
// 이유는 반환의 경우 어떤 스레드에서든 가능해야 하기 때문에 동기화 문제로 반환은 없음.
//
// ChunkBlock 에 10 개의 DATA 가 있다면 이 10개의 DATA 를 모두 할당 후 반환 하게 될 때 
// ChunkBlock 자체가 통합 메모리풀로 반환되어 청크 자체의 재 사용을 기다리게 된다.
//////////////////////////////////////////////////////////////////

#define BLOCK_CHECK 0xABCDEF123456789A
#define INIT_CHECK 0x777778888899999A
template <class DATA>
class CMemoryPoolTLS
{
private:
	class CChunkBlock
	{
	private:
#pragma pack(push, 1)
		struct st_ChunkDATA
		{
			unsigned __int64 BlockCheck; // 64비트
			CChunkBlock *pThisChunk;		// 64비트
			DATA	Data;
			CMemoryPool<CChunkBlock>* pMemoryPool;
			bool    Alloced;
		};
#pragma pack(pop)
	public:
		CChunkBlock(CMemoryPool<CChunkBlock> *pMemoryPool, int BlockSize = 2000, bool mConstructor = false)
		{
			Constructor = mConstructor;

			if (pArrayChunk == nullptr)
				pArrayChunk = (st_ChunkDATA *)malloc(sizeof(st_ChunkDATA) * BlockSize);

			for (int i = 0; i < BlockSize; i++)
			{
				pArrayChunk[i].BlockCheck = BLOCK_CHECK;
				pArrayChunk[i].pThisChunk = this;
				pArrayChunk[i].Alloced = false;
				pArrayChunk[i].pMemoryPool = pMemoryPool;
			}

			m_lInit = 0;
			m_lAllocCount = 0;
			m_lReferenceCount = BlockSize;
		}

		DATA* Alloc()
		{
			if (pArrayChunk[m_lAllocCount].BlockCheck != BLOCK_CHECK)
				return nullptr;

			if (pArrayChunk[m_lAllocCount].Alloced)
				m_lAllocCount++;

			pArrayChunk[m_lAllocCount].Alloced = true;
			DATA *ret = &pArrayChunk[m_lAllocCount].Data;


			if (Constructor)
				new (ret) DATA();

			m_lAllocCount++;

			return ret;
		}
		bool Free(DATA *pData, st_ChunkDATA *pBlock)
		{
			if (pBlock->BlockCheck != BLOCK_CHECK)
				return false;

			if (!pBlock->Alloced)
				return false;

			pBlock->Alloced = false;

			if (InterlockedDecrement(&m_lReferenceCount) == 0)
			{
				pBlock->pMemoryPool->Free(pBlock->pThisChunk);
				//free(pArrayChunk);
				m_lInit = INIT_CHECK;
			}

			if (Constructor)
				pData->~DATA();

			return true;
		}

	private:
		st_ChunkDATA *pArrayChunk;

		LONG	m_lReferenceCount;
		LONG	m_lAllocCount;
		bool    Constructor;
		LONG64	m_lInit; // 재사용



		template <class DATA>
		friend class CMemoryPoolTLS;
	};

public:
	CMemoryPoolTLS(int ChunkSize = 1000, int BlockSize = 0, bool mConstructor = false)
		:BlockSize(BlockSize), b_Constructor(mConstructor), ChunkSize(ChunkSize)
	{
		m_lAllocCount = 0;

		pMemoryPool = new CMemoryPool<CChunkBlock>(BlockSize, false);

		TLSIndex = TlsAlloc();
		if (TLSIndex == TLS_OUT_OF_INDEXES)
		{
			int *p = nullptr;
			*p = 0;
		}
	}

	~CMemoryPoolTLS()
	{
		TlsFree(TLSIndex);
	}

	DATA* Alloc()
	{
		CChunkBlock *pBlock = (CChunkBlock *)TlsGetValue(TLSIndex);

		if (pBlock == nullptr)
		{
			pBlock = pMemoryPool->Alloc();
			new (pBlock) CChunkBlock(pMemoryPool, ChunkSize, b_Constructor);
			TlsSetValue(TLSIndex, pBlock);
		}

		DATA* pRet = pBlock->Alloc();
		InterlockedAdd(&m_lAllocCount, 1);


		if (pBlock->m_lAllocCount == ChunkSize || pBlock->m_lReferenceCount == 0)
			TlsSetValue(TLSIndex, nullptr);

		return pRet;
	}
	bool Free(DATA *pData)
	{
		CChunkBlock::st_ChunkDATA *pBlock = (CChunkBlock::st_ChunkDATA *)((__int64 *)pData - 2);


		if (pBlock->pThisChunk->Free(pData, pBlock))
			InterlockedAdd(&m_lAllocCount, -1);

		return true;
	}
	long GetChunkSize()
	{
		return pMemoryPool->GetAllocCount();
	}
	long GetAllocCount()
	{
		return  max(m_lAllocCount, 0);
	}
private:
	CMemoryPool<CChunkBlock>* pMemoryPool;

	DWORD TLSIndex;
	LONG	m_lAllocCount;
	int		BlockSize;
	int		ChunkSize;

	bool	b_Constructor;
};

