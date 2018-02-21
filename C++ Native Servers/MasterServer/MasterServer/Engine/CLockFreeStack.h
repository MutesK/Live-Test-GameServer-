#pragma once

#include "MemoryPoolTLS.h"


template <class DATA>
class CLockFreeStack
{
private:
#define STACKVALIDCODE (DWORD)(0x77778888)
#pragma pack(push, 1)
	struct st_NODE
	{
		DWORD ValidCode;
		DATA Data;

		st_NODE *pNextNode;
	};

	struct st_Top
	{
		st_NODE *_TopNode;
		__int64	 UniqueCount;
	};
#pragma pack(pop)
public:
	CLockFreeStack(int iSize = 0);
	virtual ~CLockFreeStack();

	void Push(DATA Data);
	bool Pop(DATA *pData);
	///////////////////////////////////////////////////
	// 스택 내부를 깨끗하게 지운다.
	///////////////////////////////////////////////////
	void ClearStack();
	///////////////////////////////////////////////////
	// 현재 스택에서 사용한 크기를 가져온다.
	///////////////////////////////////////////////////
	int  GetUseSize();
	///////////////////////////////////////////////////
	// 현재 스택 블록 사이즈를 가져온다.
	///////////////////////////////////////////////////
	int GetStackSize();

private:
	CMemoryPool<st_NODE> *pMemoryPool;

	bool	m_bFixed;

	LONG m_iUseSize;
	LONG64 m_iUnique;

	st_Top *pTop;
};

template <class DATA>
CLockFreeStack<DATA>::CLockFreeStack(int iSize)
{
	pMemoryPool = new CMemoryPool<st_NODE>(iSize);

	m_iUseSize = 0;
	m_iUnique = 0;

	pTop = (st_Top *)_aligned_malloc(128, 16);
	pTop->_TopNode = nullptr;
	pTop->UniqueCount = 0;
}

template <class DATA>
CLockFreeStack<DATA>::~CLockFreeStack()
{
	ClearStack();
	_aligned_free(pTop);
}

template <class DATA>
void CLockFreeStack<DATA>::ClearStack()
{
	while (pTop->_TopNode != nullptr)
	{
		pMemoryPool->Free(pTop->_TopNode);
		pTop->_TopNode = pTop->_TopNode->pNextNode;
	}
	m_iUseSize = 0;
}
template <class DATA>
void CLockFreeStack<DATA>::Push(DATA Data)
{
	st_NODE *pNewNode = pMemoryPool->Alloc();

	pNewNode->ValidCode = STACKVALIDCODE;
	pNewNode->Data = Data;
	pNewNode->pNextNode = nullptr;

	LONG64 Unique = InterlockedIncrement64(&m_iUnique);
	st_Top OldTop;

	do
	{
		OldTop = { 0 };

		OldTop._TopNode = pTop->_TopNode;
		OldTop.UniqueCount = pTop->UniqueCount;

		if (OldTop._TopNode != nullptr)
			pNewNode->pNextNode = OldTop._TopNode;

	} while (!InterlockedCompareExchange128((LONG64 *)pTop, (LONG64)Unique, (LONG64)pNewNode, (LONG64 *)&OldTop));


	InterlockedAdd(&m_iUseSize, 1);
}
template <class DATA>
bool CLockFreeStack<DATA>::Pop(DATA *pData)
{
	if (m_iUseSize == 0)
		return false;

	LONG64 Unique = InterlockedIncrement64(&m_iUnique);
	st_NODE *pNewNode = nullptr;
	st_Top OldTop;

	do
	{
		OldTop = { 0 };

		OldTop._TopNode = pTop->_TopNode;
		OldTop.UniqueCount = pTop->UniqueCount;

		*pData = OldTop._TopNode->Data;

		pNewNode = OldTop._TopNode->pNextNode;

	} while (!InterlockedCompareExchange128((LONG64 *)pTop, (LONG64)Unique, (LONG64)pNewNode, (LONG64 *)&OldTop));


	//InterlockedDecrement64(&m_iUseSize);
	InterlockedAdd(&m_iUseSize, -1);


	pMemoryPool->Free(OldTop._TopNode);



	return true;
}

template <class DATA>
int CLockFreeStack<DATA>::GetUseSize()
{
	return m_iUseSize;
}
template <class DATA>
int CLockFreeStack<DATA>::GetStackSize()
{
	return pMemoryPool->GetBlockCount();
}