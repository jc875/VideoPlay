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
	void SetMethod(const Buffer& method);
	void SetURL(const Buffer& url);
	void SetSeq(const Buffer& url);
	void SetSession(const Buffer& url);
	void SetClientRTPPort(int *port);

private:
	int m_method; // 0:OPTIONS 1:DESCRIBE 2:SETUP 3:PLAY 4:PAUSE 5:TEARDOWN
	std::string m_url;
	int m_seq;
	int m_client_port[2];
	std::string m_sessionID;
	
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
	std::string m_sdp;
	short m_serverRTPPort;
	short m_clientRTPPort;
};

class RTSPSession
{	
public:
	RTSPSession() {}
	RTSPSession(const RTSPSession& other) {
		if (this != &other) {
			
		}
	}
	RTSPSession& operator=(const RTSPSession& other) {
		if (this != &other) {
			
		}
		return *this;
	}
	RTSPSession(ESocket clientSocket) : m_clientSocket(clientSocket) {}
	~RTSPSession() {}
	int PickSessionAndHandleRequest();
	Buffer PickLine( Buffer& buffer);
	Buffer Pick();
	RTSPRequest ParseRequest(const Buffer& buffer);
	RTSPReply HandleRequest(const RTSPRequest& request);
private:
	
	std::string m_sessionID;
	ESocket m_clientSocket;
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
	
	int ThreadSessionWorker();
private:
	ESocket m_listenSocket;
	EAddress m_address;
	int m_status; // 0:未启动 1:启动中 2:运行中 3:停止中
	EdoyunThread m_threadMain;
	EdoyunThreadPool m_threadPool;
	static SocketIniter socketIniter;
	CEdoyunQueue<RTSPSession> m_lstSession;
};

