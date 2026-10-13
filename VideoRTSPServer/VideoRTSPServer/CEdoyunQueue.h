#pragma once

#include<mutex>
#include<list>      
#include<atomic> 
#include "EdoyunThread.h"
#ifndef SENDLOG
#define SENDLOG(...) do { FILE* _f = NULL; fopen_s(&_f, "D:\\code\\remote-control\\remote-control\\sendlog.txt", "a"); if (_f) { fprintf(_f, __VA_ARGS__); fclose(_f); } } while (0)
#endif

template<class T>
class CEdoyunQueue
{//线程安全的队列（利用IOCP实现）
public:
	enum {
		EQNone,
		EQPush,
		EQPop,
		EQSize,
		EQClear,
	};
	typedef struct IocpParam {
		int nOperator; //操作
		T Data; //数据
		
		HANDLE hEvent;
		IocpParam(int op, const T& data,HANDLE hEve=NULL) {
			nOperator = op;
			Data = data;
			hEvent = hEve;
		}
		IocpParam() {
			nOperator = EQNone;
		}
	} PPARAM;//Post Parameter用于投递消息的结构体

	
	CEdoyunQueue() {
		m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1);
		SENDLOG("[Queue] ctor port=%p\n", m_hCompletionPort);
		m_hThread = INVALID_HANDLE_VALUE;
		m_lock = false;
		if (m_hCompletionPort != NULL) {
			m_hThread = (HANDLE)_beginthread(
				&CEdoyunQueue<T>::threadEntry,
				0, this
			);
		}
		SENDLOG("[Queue] ctor thread=%p\n", m_hThread);
	}
	virtual ~CEdoyunQueue() {
		if (m_lock)return;
		m_lock=true;
		
		PostQueuedCompletionStatus(m_hCompletionPort, 0, NULL, NULL); // 此时m_hCompletionPort还是有效值
		WaitForSingleObject(m_hThread, INFINITE); // 等待线程彻底退出
		if (m_hCompletionPort != NULL) {
			HANDLE hTemp = m_hCompletionPort;       // 先把当前有效句柄复制到局部栈变量
			m_hCompletionPort = NULL; // 成员置空，此时对象成员已经可以随便写
			CloseHandle(hTemp); // 使用栈上保存的副本关闭内核对象，不受成员变量置空影响
		}		
	}

	bool PushBack(const T& data) {
		
		IocpParam* pParam = new IocpParam(EQPush, data);
		if (m_lock == true) {
			delete pParam;
			return false;
		}
		bool ret = PostQueuedCompletionStatus(m_hCompletionPort, sizeof(PPARAM),
			(ULONG_PTR)pParam, NULL);
		if (ret == false) delete pParam;
		return ret;
	}
	virtual bool PopFront(T& data) {
		
		HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		IocpParam Param(EQPop, data, hEvent);
		if (m_lock) {
			if (hEvent)CloseHandle(hEvent);
			return false;
		}
		bool ret = PostQueuedCompletionStatus(m_hCompletionPort, sizeof(PPARAM),
			(ULONG_PTR)&Param, NULL);
		if (ret == false) {
			CloseHandle(hEvent);
			return false;
		}
		ret = WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0;
		if (ret) {
			data = Param.Data;
		}
		return ret;
	}

	size_t Size() {
		HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		IocpParam Param(EQSize, T(), hEvent);
		if (m_lock) {
			if (hEvent)CloseHandle(hEvent);
			return -1;
		}
		bool ret = PostQueuedCompletionStatus(m_hCompletionPort, sizeof(PPARAM),
			(ULONG_PTR)&Param, NULL);
		if (ret == false) {
			CloseHandle(hEvent);
			return -1;
		}
		ret = WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0;
		if (ret) {
			return Param.nOperator;
		}
		return -1;
	}

	bool Clear() {
		if (m_lock == true)return false;
		IocpParam* pParam = new IocpParam(EQClear,T() );

		bool ret = PostQueuedCompletionStatus(m_hCompletionPort, sizeof(PPARAM),
			(ULONG_PTR)pParam, NULL);
		if (ret == false) delete pParam;
		return ret;
	}

protected:  // 原为 private：派生类 EdoyunSendQueue 要重写 DealParam/threadMain，必须 protected
	virtual void DealParam(PPARAM* pParam) {
		switch (pParam->nOperator)
		{
		case EQPush:
			m_lstData.push_back(pParam->Data);
				SENDLOG("[Queue] EQPush size=%zu\n", m_lstData.size());
			delete pParam;
			break;
		case EQPop:
			if (m_lstData.size() > 0) {
				pParam->Data = m_lstData.front();
				m_lstData.pop_front();
			}
			if (pParam->hEvent != NULL)
				SetEvent(pParam->hEvent);
			break;
		case EQSize:
			pParam->nOperator = m_lstData.size();
			if (pParam->hEvent != NULL)
				SetEvent(pParam->hEvent);
			break;
		case EQClear:
			m_lstData.clear();
			delete pParam;
			break;
		default:
			OutputDebugStringA("unknown operator!\r\n");
			break;
		}
	}
	static void threadEntry(void* arg) {
		CEdoyunQueue<T>* thiz = (CEdoyunQueue<T>*)arg;
		thiz->threadMain();
		_endthread();
	}
	void threadMain() {
		DWORD dwTransferred = 0;
		PPARAM* pParam = NULL;
		ULONG_PTR CompletionKey = 0;
		OVERLAPPED* pOverlapped = NULL;
		while (GetQueuedCompletionStatus(
			m_hCompletionPort,
			&dwTransferred,
			&CompletionKey,
			&pOverlapped, INFINITE))
		{
			if ((dwTransferred == 0) || (CompletionKey == NULL))
			{
				printf("thread is prepare to exit!\r\n");
				break;
			}

			pParam = (PPARAM*)CompletionKey;
			SENDLOG("[Queue] threadMain got op=%d\n", pParam->nOperator);
			DealParam(pParam);
		}
		

		HANDLE hTemp = m_hCompletionPort;       // 先把当前有效句柄复制到局部栈变量
		
		m_hCompletionPort = NULL; // 成员置空，此时对象成员已经可以随便写
		CloseHandle(hTemp);

	}

protected:
	std::list<T> m_lstData;
	HANDLE m_hCompletionPort;
	HANDLE m_hThread;
	std::atomic<bool> m_lock;//队列正在析构
};

typedef int (ThreadFuncBase::* EDYCALLBACK)();

template<class T>
class EdoyunSendQueue :public CEdoyunQueue<T>, public ThreadFuncBase
{
public:
	typedef int (ThreadFuncBase::* EDYCALLBACK)(T& data);
	EdoyunSendQueue(ThreadFuncBase* obj, EDYCALLBACK callback)
		:CEdoyunQueue<T>(), m_base(obj), m_callback(callback)
	{
		m_thread.Start();
		m_thread.UpdateWorker(::ThreadWorker(this, (FUNCTYPE)&EdoyunSendQueue<T>::threadTick));
	}
	virtual ~EdoyunSendQueue()
	{
		m_base = NULL;
		m_callback = NULL;
		m_thread.Stop();
	}

protected:
	virtual bool PopFront(T& data)
	{
		return false;
	}
	bool PopFront() {
		typename CEdoyunQueue<T>::IocpParam* Param = new typename CEdoyunQueue<T>::IocpParam(CEdoyunQueue<T>::EQPop, T());
		if (CEdoyunQueue<T>::m_lock) {
			delete Param;
			return false;
		}
		bool ret = PostQueuedCompletionStatus(CEdoyunQueue<T>::m_hCompletionPort, sizeof(*Param), (ULONG_PTR)Param, NULL);
			SENDLOG("[Queue] PopFront post EQPop ret=%d\n", ret);
		if (ret == false) {
			delete Param;
			return false;
		}
		return ret;
	}
	int threadTick() {
		if (WaitForSingleObject(CEdoyunQueue<T>::m_hThread, 0) != WAIT_TIMEOUT)
			return 0;
		if (CEdoyunQueue<T>::m_lstData.size() > 0) {
			SENDLOG("[Queue] threadTick has data size=%zu\n", CEdoyunQueue<T>::m_lstData.size());
			PopFront();
		}
		Sleep(1);
		return 0;
	}
	virtual void DealParam(typename CEdoyunQueue<T>::PPARAM* pParam) {
		switch (pParam->nOperator)
		{
		case CEdoyunQueue<T>::EQPush:
			CEdoyunQueue<T>::m_lstData.push_back(pParam->Data);
			delete pParam;
			//printf("delete %08p\r\n", (void*)pParam);
			break;
		case CEdoyunQueue<T>::EQPop:
			if (CEdoyunQueue<T>::m_lstData.size() > 0) {
				pParam->Data = CEdoyunQueue<T>::m_lstData.front();
				CEdoyunQueue<T>::m_lstData.pop_front();
			}
			SENDLOG("[DealParam] EQPop before cb\n");
			if (m_base && m_callback) (m_base->*m_callback)(pParam->Data);
			SENDLOG("[DealParam] EQPop after cb\n");
			if (pParam->hEvent != NULL)SetEvent(pParam->hEvent);
			delete pParam; // 无参 PopFront new 出来的信封，处理完释放（防泄漏）
			break;
		case CEdoyunQueue<T>::EQSize:
			pParam->nOperator = CEdoyunQueue<T>::m_lstData.size();
			if (pParam->hEvent != NULL)
				SetEvent(pParam->hEvent);
			break;
		case CEdoyunQueue<T>::EQClear:
			CEdoyunQueue<T>::m_lstData.clear();
			delete pParam;
			//printf("delete %08p\r\n", (void*)pParam);
			break;
		default:
			OutputDebugStringA("unknown operator!\r\n");
			break;
		}
	}
private:
	ThreadFuncBase* m_base;
	EDYCALLBACK m_callback;
	EdoyunThread m_thread;
};


typedef EdoyunSendQueue<std::vector<char>>::EDYCALLBACK SENDCALLBACK;