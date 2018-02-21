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
	// 큐 내부를 깨끗하게 지운다.
	///////////////////////////////////////////////////
	void ClearQueue();
	///////////////////////////////////////////////////
	// 현재 큐에서 사용한 크기를 가져온다.
	///////////////////////////////////////////////////
	LONG  GetUseSize();
	///////////////////////////////////////////////////
	// 현재 큐 블록 사이즈를 가져온다.
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
				// Tail->Next가 Nullptr이라면, 생성한 노드로 바꾼다. == Nullptr의 리턴값이 나왔다면(성공)  
				// 데이터를 넣는 행위
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
	// 1. 사이즈 체크한다.
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
		// Tail이 필요한 이유는 멀티스레드 환경에서 마지막 데이터 위치를 판단하기 위해서다.
		front.pNode = _Front->pNode;
		front.UniqueCount = _Front->UniqueCount;

		rear.pNode = _Rear->pNode;
		rear.UniqueCount = _Rear->UniqueCount;

		pNext = front.pNode->pNextNode;

		if (pNext == nullptr)
			return false;		// 사실상 -1은 데이터가 없다. 데이터가 있는데도 여기에서 리턴이 나왔다면 엿된거다 ㅠㅠ...

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
					return false;		// 사실상 -1은 데이터가 없다. 데이터가 있는데도 여기에서 리턴이 나왔다면 엿된거다 ㅠㅠ...

				if (m_iUseSize == 0)
					return false;
			}
			else
			{
				// 데이터를 뺀다.
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

	// 사이즈를 감소시킨다.

	return true;
}
