#pragma once

#include "MemoryPoolTLS.h"

template <class DATA>
class CLockFreeQueue
{
private:
	enum
	{
		enBLOCKCHECK = 0xCCDDEEFFAABBCCDD
	};
#pragma pack(push ,1)
	struct st_NODE
	{
		__int64 BlockCheck;
		DATA Data;
		st_NODE *pNextNode;
	};

	struct st_DUMMY
	{
		st_NODE *pNode;
		__int64	 UniqueCount;
	};
#pragma pack(pop)
public:
	CLockFreeQueue(int iSize=0);
	virtual ~CLockFreeQueue();

	void Enqueue(const DATA *data);
	bool Dequeue(DATA *pOutData);


	///////////////////////////////////////////////////
	// ť ���θ� �����ϰ� �����.
	///////////////////////////////////////////////////
	void ClearQueue();
	///////////////////////////////////////////////////
	// ���� ť���� ����� ũ�⸦ �����´�.
	///////////////////////////////////////////////////
	LONG  GetUseSize();
	///////////////////////////////////////////////////
	// ���� ť ��� ����� �����´�.
	///////////////////////////////////////////////////
	int GetQueueSize();

private:
	CMemoryPool<st_NODE> *pMemoryPool;
	//CLinkedList<st_NODE *> List;

	st_DUMMY *_Rear;
	st_DUMMY *_Front;

	LONG m_iUseSize;
	LONG64 m_iUnique;

	bool	PeekMode;
};

template <class DATA>
CLockFreeQueue<DATA>::CLockFreeQueue(int iSize)
{
	pMemoryPool = new CMemoryPool<st_NODE>(iSize);

	m_iUseSize = m_iUnique = 0;
	PeekMode = false;

	_Rear = (st_DUMMY *)_aligned_malloc(128, 16);
	_Rear->pNode = nullptr;
	_Rear->UniqueCount = 0;

	_Front = (st_DUMMY *)_aligned_malloc(128, 16);
	_Front->pNode = nullptr;
	_Front->UniqueCount = 0;

	_Rear->pNode = pMemoryPool->Alloc();
	//List.push_back(_Rear->pNode);

	st_NODE *pNewNode = _Rear->pNode;
	pNewNode->pNextNode = nullptr;
	pNewNode->BlockCheck = 0xCCDDEEFFAABBCCDD;

	_Front->pNode = pNewNode;
}

template <class DATA>
CLockFreeQueue<DATA>::~CLockFreeQueue()
{
	_aligned_free(_Rear);
	_aligned_free(_Front);

	ClearQueue();
}
template <class DATA>
void CLockFreeQueue<DATA>::ClearQueue()
{
	while (_Front->pNode != nullptr)
	{
		pMemoryPool->Free(_Front->pNode);
		_Front->pNode = _Front->pNode->pNextNode;
	}

	_Rear->pNode = pMemoryPool->Alloc();
	//List.push_back(_Rear->pNode);

	st_NODE *pNewNode = _Rear->pNode;
	pNewNode->pNextNode = nullptr;
	pNewNode->BlockCheck = 0xCCDDEEFFAABBCCDD;

	_Front->pNode = pNewNode;

	m_iUseSize = 0;
}
template <class DATA>
LONG CLockFreeQueue<DATA>::GetUseSize()
{
	return max(m_iUseSize, 0);
}
template <class DATA>
int CLockFreeQueue<DATA>::GetQueueSize()
{
	return pMemoryPool->GetBlockCount();
}
template <class DATA>
void CLockFreeQueue<DATA>::Enqueue(const DATA *data)
{
	st_NODE *pNewNode = pMemoryPool->Alloc();



	pNewNode->BlockCheck = 0xCCDDEEFFAABBCCDD;
	pNewNode->Data = *data;
	pNewNode->pNextNode = nullptr;

	st_DUMMY rear;
	st_NODE *pNext;
	LONG64 Unique = InterlockedIncrement64(&m_iUnique) % MAXDWORD64;

	while (1)
	{
		rear.pNode = _Rear->pNode;
		rear.UniqueCount = _Rear->UniqueCount;

		pNext = rear.pNode->pNextNode;

		if (pNewNode == rear.pNode)
		{
			if(pNext != nullptr)
				InterlockedCompareExchangePointer((PVOID *)&_Rear->pNode, pNext, rear.pNode);
		}
		
		if (rear.pNode == _Rear->pNode)
		{

			if (pNext == nullptr)
			{
				// Tail->Next�� Nullptr�̶��, ������ ���� �ٲ۴�. == Nullptr�� ���ϰ��� ���Դٸ�(����)  
				// �����͸� �ִ� ����
				if (InterlockedCompareExchangePointer((PVOID *)&rear.pNode->pNextNode, pNewNode, nullptr) == nullptr)
				{
					InterlockedCompareExchange128((LONG64 *)_Rear, (LONG64)Unique, (LONG64)pNewNode, (LONG64 *)&rear);
					break;
				}
			}
			else
			{
				InterlockedCompareExchangePointer((PVOID *)&_Rear->pNode, pNext, rear.pNode);
			}
		}

		rear = { 0 };
		pNext = nullptr;
		Sleep(0);
	}

	InterlockedIncrement(&m_iUseSize);
}

template <class DATA>
bool CLockFreeQueue<DATA>::Dequeue(DATA *pOutData)
{
	// 1. ������ üũ�Ѵ�.
	if (InterlockedDecrement(&m_iUseSize) < 0)
	{
		InterlockedIncrement(&m_iUseSize);
		return false;
	}

	LONG64 Unique = InterlockedIncrement64(&m_iUnique) % MAXDWORD64;
	st_DUMMY front;
	st_DUMMY rear;
	st_NODE *pNext;

	while (1)
	{
		// Tail�� �ʿ��� ������ ��Ƽ������ ȯ�濡�� ������ ������ ��ġ�� �Ǵ��ϱ� ���ؼ���.
		front.pNode = _Front->pNode;
		front.UniqueCount = _Front->UniqueCount;

		rear.pNode = _Rear->pNode;
		rear.UniqueCount = _Rear->UniqueCount;

		pNext = front.pNode->pNextNode;

		if (pNext == nullptr)
			return false;		// ��ǻ� -1�� �����Ͱ� ����. �����Ͱ� �ִµ��� ���⿡�� ������ ���Դٸ� ���ȰŴ� �Ф�...

		st_NODE *pRearNext = rear.pNode->pNextNode;

		if (pRearNext != nullptr)
		{
			InterlockedCompareExchangePointer((PVOID *)&_Rear->pNode, pRearNext, rear.pNode);
			Sleep(0);
			continue;
		}

		if (front.pNode == _Front->pNode)
		{
			if (front.pNode == rear.pNode || 
				front.pNode == _Rear->pNode)
 			{

				if (pNext == nullptr)
					return false;		// ��ǻ� -1�� �����Ͱ� ����. �����Ͱ� �ִµ��� ���⿡�� ������ ���Դٸ� ���ȰŴ� �Ф�...

				if (m_iUseSize == 0)
					return false;
			}
			else
			{
				// �����͸� ����.
				*pOutData = pNext->Data;
				
				if (front.pNode == rear.pNode || front.pNode == _Rear->pNode)
					continue;

				if (InterlockedCompareExchange128((LONG64 *)_Front, (LONG64)Unique, (LONG64)pNext, (LONG64 *)&front))
				{
					pMemoryPool->Free(front.pNode);
					break;
				}
			}

		}

		Sleep(0);
	}

	// ����� ���ҽ�Ų��.

	return true;
}
