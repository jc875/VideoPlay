#pragma once
#include"Socket.h"
#include <string>
#include"EdoyunThread.h"
#include<map>
#include"CEdoyunQueue.h"
class RTSPRequest
{
public:
	RTSPRequest() {}
	RTSPRequest(int method) : m_method(method) {}
	RTSPRequest(const RTSPRequest& other)  {
		if (this != &other) {
			m_method = other.m_method;
		}
	}
	RTSPRequest& operator=(const RTSPRequest& other) {
		if (this != &other) {
			m_method = other.m_method;
		}
		return *this;
	}
	~RTSPRequest() {}
private:
	int m_method; // 0:OPTIONS 1:DESCRIBE 2:SETUP 3:PLAY 4:PAUSE 5:TEARDOWN
};

class RTSPReply
{
public:
	RTSPReply() {}
	RTSPReply(int method) : m_method(method) {}
	RTSPReply(const RTSPReply& other) {
		if (this != &other) {
			m_method = other.m_method;
		}
	}
	RTSPReply& operator=(const RTSPReply& other) {
		if (this != &other) {
			m_method = other.m_method;
		}
		return *this;
	}
	~RTSPReply() {}
	Buffer ToBuffer();
private:
	int m_method; // 0:OPTIONS 1:DESCRIBE 2:SETUP 3:PLAY 4:PAUSE 5:TEARDOWN
};

class RTSPSession
{	
public:
	RTSPSession() {}
	RTSPSession(const RTSPSession& other) {
		if (this != &other) {
			m_request = other.m_request;
			m_reply = other.m_reply;
		}
	}
	RTSPSession& operator=(const RTSPSession& other) {
		if (this != &other) {
			m_request = other.m_request;
			m_reply = other.m_reply;
		}
		return *this;
	}
	~RTSPSession() {}
private:
	RTSPRequest m_request;
	RTSPReply m_reply;
};


class RTSPServer : public ThreadFuncBase
{
public:
	RTSPServer(): m_listenSocket(0), m_address(), m_status(0),m_threadPool(10)  {
		m_threadMain.UpdateWorker(::ThreadWorker(this, (FUNCTYPE)&RTSPServer::threadWorker));
	}
	int Init(const std::string& strIP="0.0.0.0", short nPort=554);
	//nPort: 554
	RTSPServer(int nPort);
	~RTSPServer();
	//启动服务
	int Start();
	//停止服务
	void Stop();
	
protected:
	int threadWorker();
	RTSPRequest AnalyzeRequest(const std::string& request);
	RTSPReply HandleRequest(const RTSPRequest& request);
	int ThreadSessionWorker();
private:
	ESocket m_listenSocket;
	EAddress m_address;
	int m_status; // 0:未启动 1:启动中 2:运行中 3:停止中
	EdoyunThread m_threadMain;
	EdoyunThreadPool m_threadPool;
	std::map<std::string, RTSPSession> m_sessions;
	static SocketIniter socketIniter;
	CEdoyunQueue<ESocket> m_clients;
};

