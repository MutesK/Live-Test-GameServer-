#pragma once

#include <Windows.h>
template <class DATA>
class CQueue
{
private:
	struct st_NODE
	{
		DATA Data;
		st_NODE *pNext;
	};

public:
	CQueue()
	{
		pRear = new st_NODE;
		pFront = new st_NODE;

		UseSize = 0;

		pFront->pNext = pRear;
		pRear->pNext = nullptr;

		InitializeCriticalSection(&m_QueueSection);

	}
	virtual ~CQueue()
	{
		DeleteCriticalSection(&m_QueueSection);
		ClearBuffer();
	}

	bool Enqueue(DATA *rData)
	{
		pRear->Data = *rData;

		st_NODE *pNewNode = new st_NODE;
		pNewNode->pNext = nullptr;

		pRear->pNext = pNewNode;
		pRear = pNewNode;
		
		UseSize++;

		return true;
	}
	bool Dequeue(DATA *pOutData)
	{
		if (UseSize == 0)
			return false;
		
		st_NODE *pDelNode = pFront;
		pFront = pFront->pNext;
		*pOutData = pFront->Data;

		delete pDelNode;
		UseSize--;

		return true;
	}

	int GetUseSize()
	{
		return UseSize;
	}

	bool Peek(DATA *pOut, int Pos)
	{
		st_NODE *pNode = pFront->pNext;

		for (int i = 0; i < Pos; i++)
		{
			if (pNode == nullptr)
				return false;

			pNode = pNode->pNext;
		}

		*pOut = pNode->Data;
		return true;
	}
	void ClearBuffer()
	{
		st_NODE *pNode = pFront;
		st_NODE *delNode;
		while (pNode != nullptr)
		{
			delNode = pNode;

			pNode = pNode->pNext;
			delete delNode;
		}

		pRear = new st_NODE;
		pFront = new st_NODE;

		UseSize = 0;

		pFront->pNext = pRear;
		pRear->pNext = nullptr;
	}
	void Lock()
	{
		EnterCriticalSection(&m_QueueSection);
	}
	void UnLock()
	{
		LeaveCriticalSection(&m_QueueSection);
	}

private:

	int UseSize;
	st_NODE *pRear, *pFront;

	CRITICAL_SECTION m_QueueSection;
};