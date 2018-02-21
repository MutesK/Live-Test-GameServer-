#pragma once

#include <Windows.h>
#include <new>

///////////////////
#include <vector>
using namespace std;
/*
Object Pool (FreeList형식) 의 Memory Pool

움직이는 알고리즘 자체는 Stack과 비슷함.
Lock-Free 도입
*/

#define VALIDCODE (DWORD)(0x77777777)

template <class T>
class CLinkedList
{
public:
	struct Node
	{
		T _Data;
		Node *prev;
		Node *next;
	};

	class iterator
	{
	private:
		Node * pos;

	public:
		iterator(Node *_node = nullptr)
		{
			pos = _node;
		}

		iterator& operator++(int)
		{
			pos = pos->next;
			return *this;
		}

		iterator& operator--()
		{
			pos = pos->prev;
			return *this;
		}

		bool operator!=(iterator& iter)
		{
			if (this->pos != iter.pos)
				return true;
			return false;
		}

		bool operator==(iterator& iter)
		{
			if (this->pos == iter.pos)
				return true;
			return false;
		}

		T& operator*()
		{
			if (pos != nullptr)
			{
				return pos->_Data;
			}
		}
	};

	CLinkedList()
	{
		_size = 0;

		head.next = nullptr;
		head.next = &tail;
		tail.next = nullptr;
		tail.prev = &head;
	}

public:
	iterator begin()
	{
		iterator iter(head.next);
		return iter;
	}

	iterator end()
	{
		iterator iter(&tail);
		return iter;
	}

	/*
	- 이터레이터의 그 노드를 지움.
	- 그리고 지운 노드의 다음 노드를 카리키는 이터레이터 리턴
	아직 이 함수는 디버그를 하지 못함.
	*/
	iterator& erase(iterator &iter)
	{
		Delete(*iter);
		iter++;
		return iter;
	}


	void push_back(T Data)
	{
		Node *newNode = new Node;
		newNode->_Data = Data;

		newNode->prev = tail.prev;
		newNode->next = &tail;

		newNode->prev->next = newNode;
		tail.prev = newNode;

		_size++;
	}

	void push_front(T Data)
	{
		Node *newNode = (Node *)malloc(sizeof(Node));
		newNode->_Data = Data;

		newNode->prev = &head;
		newNode->next = head.next;

		newNode->next->prev = newNode;
		Head.prev = newNode;

		_size++;
	}

	T& GetLastData()
	{
		return tail.prev->_Data;
	}

	T pop_back()
	{
		T ret = tail.prev->_Data;
		Delete(tail.prev);
		return ret;
	}
	T pop_front()
	{
		T ret = head.next->_Data;
		Delete(head->next);
		return ret;
	}

	int size()
	{
		return _size;
	}
	void clear()
	{
		Node *pStart = head.next;

		while (pStart != &tail)
		{
			Node *delNode = pStart;

			pStart->prev->next = pStart->next;
			pStart->next->prev = pStart->prev;

			_size--;

			pStart = pStart->next;
			delete delNode;
		}
	}

	void Delete(T Data)
	{
		Node *pStart = head.next;

		while (pStart->next != nullptr) // pStart != &Tail
		{
			if (pStart->_Data == Data)
			{
				Node *delNode = pStart;

				pStart->prev->next = pStart->next;
				pStart->next->prev = pStart->prev;
				_size--;
				delete delNode;
				return;
			}
			else
				pStart = pStart->next;

		}
	}

private:
	int _size;
	Node head;
	Node tail;

};

template <class DATA>
class CMemoryPool
{
private:
#pragma pack(push, 1)
	struct st_BLOCK_NODE
	{
		DWORD ValidCode;
		DATA Data;
		LONG  Alloc;
		st_BLOCK_NODE *pNextBlock;
	};

	struct st_Pop
	{
		st_BLOCK_NODE *_TopNode;
		__int64		   UniqueCount;
	};

#pragma pack(pop)
public:
	//////////////////////////////////
	// 생성자
	// int - 블럭 갯수
	// bool - 블록 생성자 호출여부(기본값 = FALSE)
	//////////////////////////////////
	CMemoryPool(int blockSize = 1, bool bConst = false);
	virtual ~CMemoryPool();


	//////////////////////////////////
	// 블록 하나를 할당해주는 함수 -> new 선언해줘야 한다면 한다.
	// 리턴 : 특정 블록의 공간 포인터 리턴
	//////////////////////////////////
	DATA* Alloc(void);

	//////////////////////////////////
	// 사용중인 블록을 반환하는 함수
	// 파라미터 : 사용중인 데이터 주소값-> 소멸자 호출해야 된다면 하고 안한다면 그냥 반환
	// 리턴 : 성공여부
	//////////////////////////////////
	bool Free(DATA *pData); // 그렇다면 외부에서 이 함수를 통해 반환하고, 나중에 이 주소값을 사용하려고 한다면? -> 주의


							//////////////////////////////////
							// 총 확보된 블록의 갯수 리턴
							//////////////////////////////////
	int GetBlockCount(void);



	//////////////////////////////////////////////////////////////////////////
	// 현재 사용중인 블럭 개수를 얻는다.
	//
	// 파라미터: 사용중인 블럭 개수.
	//////////////////////////////////////////////////////////////////////////
	int GetAllocCount(void);


private:
	// 생성시 할당량
	LONG m_iBlockSize;

	bool m_bUseConstruct;
	bool m_Fixed;

	LONG m_iAllocCount;
	LONG64 m_iUnique;

	//st_BLOCK_NODE *pTop;
	st_Pop *pPop;
	st_BLOCK_NODE *pTail;

	CLinkedList<st_BLOCK_NODE *> List;
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class DATA>
CMemoryPool<DATA>::CMemoryPool(int blockSize, bool bConst)
	: m_bUseConstruct(bConst)
{
	//InitializeCriticalSection(&m_CrticalSection);

	m_iAllocCount = 0;
	m_iBlockSize = 0;
	st_BLOCK_NODE *pNewNode = nullptr;
	st_BLOCK_NODE *pOldNode = nullptr;

	if (blockSize > 1)
		m_Fixed = true;
	else
	{
		blockSize = 1;
		m_Fixed = false;
	}

	pNewNode = (st_BLOCK_NODE *)malloc(sizeof(st_BLOCK_NODE));
	memset(pNewNode, 0, sizeof(st_BLOCK_NODE));
	pNewNode->ValidCode = VALIDCODE;
	pNewNode->Alloc = false;
	memset(&pNewNode->Data, 0, sizeof(DATA));
	List.push_back(pNewNode);

	pPop = (st_Pop *)_aligned_malloc(128, 16);
	pPop->_TopNode = pNewNode;
	pPop->UniqueCount = 0;
	pOldNode = pNewNode;


	for (int i = 1; i <= blockSize; i++)
	{
		pNewNode = (st_BLOCK_NODE *)malloc(sizeof(st_BLOCK_NODE));
		pOldNode->pNextBlock = pNewNode;
		pNewNode->pNextBlock = nullptr;
		pNewNode->ValidCode = VALIDCODE;
		memset(&pNewNode->Data, 0, sizeof(DATA));
		pNewNode->Alloc = false;

		List.push_back(pNewNode);

		pOldNode = pNewNode;
		m_iBlockSize++;
	}

	pTail = pNewNode;
}

template <class DATA>
CMemoryPool<DATA>::~CMemoryPool()
{
	//DeleteCriticalSection(&m_CrticalSection);

	auto iter = List.begin();

	for (; iter != List.end();)
	{
		st_BLOCK_NODE *pNode = (*iter);
		List.erase(iter);
		free(pNode);
	}

	_aligned_free(pPop);

}
// 삽입은 ABA문제에 거의 문제가 안생긴다.
// 
template <class DATA>
DATA* CMemoryPool<DATA>::Alloc(void)
{
	st_Pop OldPop;
	st_BLOCK_NODE *pNewNode = nullptr;
	DATA *ret = nullptr;
	LONG64 Unique = InterlockedIncrement64(&m_iUnique) % MAXLONG64;

	InterlockedIncrement(&m_iAllocCount);

RETRY:
	if (pPop->_TopNode->pNextBlock == nullptr)
	{
		if (m_Fixed)
			return nullptr;

		st_BLOCK_NODE *pNewNode = (st_BLOCK_NODE *)malloc(sizeof(st_BLOCK_NODE));
		pNewNode->ValidCode = VALIDCODE;
		pNewNode->pNextBlock = nullptr;
		pNewNode->Alloc = false;
		memset(&pNewNode->Data, 0, sizeof(DATA));
		List.push_back(pNewNode);


		while (1)
		{
			OldPop._TopNode = pPop->_TopNode;
			OldPop.UniqueCount = pPop->UniqueCount;

			if (OldPop._TopNode == pPop->_TopNode)
			{
				OldPop._TopNode->pNextBlock = pNewNode;

				InterlockedAdd(&m_iBlockSize, 1);
				break;
			}

			if (pPop->_TopNode->pNextBlock != nullptr)
				break;

			Sleep(100);
		}
	}

	while (1)
	{
		OldPop = { 0 };

		// 자료구조를 변경한다.
		OldPop._TopNode = pPop->_TopNode;
		OldPop.UniqueCount = pPop->UniqueCount;

		/////////////////////////////////////////////////////////////////////
		// 노드생성
		// AllocCount와 블록카운트 값 비교
		// 
		/////////////////////////////////////////////////////////////////////
		if (OldPop._TopNode->ValidCode != VALIDCODE)
			continue;

		if (OldPop._TopNode->pNextBlock == nullptr)
			goto RETRY;

		pNewNode = OldPop._TopNode->pNextBlock;

		if (OldPop._TopNode->Alloc)
		{
			Sleep(0);
			continue;
		}

		if (InterlockedCompareExchange128((LONG64 *)pPop, (LONG64)Unique, (LONG64)pNewNode, (LONG64 *)&OldPop))
		{
			ret = &OldPop._TopNode->Data;
			InterlockedExchange(&OldPop._TopNode->Alloc, 1);
			break;
		}
		Sleep(0);

	}
	/////////////////////////////////////////////////////////////////////////////////////////////

	return ret;
}
// DATA pData와 st_BLOCK_NODE pTop으로 처리해야된다.
// 1. pData가 not null일때
// 2. pTop의 next을 pData로
// 3. pData의 next를 pTop의 next로
// 4. 치환
template <class DATA>
bool CMemoryPool<DATA>::Free(DATA *pData)
{
	st_BLOCK_NODE *pDel = (st_BLOCK_NODE *)((DWORD *)pData - 1);

	if (InterlockedAdd(&m_iAllocCount, -1) < 0)
	{
		InterlockedAdd(&m_iAllocCount, 1);
		return false;
	}
	if (pDel->ValidCode != VALIDCODE)
		return false;

	// 이 구간은 스레드별 한번만 수행할수 있게 처리해야 된다.

	st_Pop OldPop;
	LONG64 Unique = InterlockedIncrement64(&m_iUnique) % MAXLONG64;
	while (1)
	{
		OldPop = { 0 };

		OldPop._TopNode = pPop->_TopNode;
		OldPop.UniqueCount = pPop->UniqueCount;

		if (OldPop._TopNode != pDel->pNextBlock && OldPop._TopNode != pDel)
			pDel->pNextBlock = OldPop._TopNode;

		if (InterlockedCompareExchange128((LONG64 *)pPop, (LONG64)Unique, (LONG64)pDel, (LONG64 *)&OldPop))
			break;

		Sleep(0);
	}

	InterlockedExchange(&pDel->Alloc, 0);




	return true;
}

template <class DATA>
int CMemoryPool<DATA>::GetBlockCount(void)
{
	return m_iBlockSize;
}

template <class DATA>
int CMemoryPool<DATA>::GetAllocCount(void)
{
	return m_iAllocCount;
}