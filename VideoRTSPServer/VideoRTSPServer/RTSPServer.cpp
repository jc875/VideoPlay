#include "RTSPServer.h"

int RTSPServer::Init(const std::string& strIP, short nPort)
{
	m_address.Update(strIP, nPort);
	m_listenSocket.Bind(m_address);
	m_listenSocket.Listen(5);
    return 0;
}

RTSPServer::~RTSPServer()
{
	Stop();
}

int RTSPServer::Start()
{
	m_threadMain.Start();
	return 0;
}

void RTSPServer::Stop()
{
	m_listenSocket.Close();
	m_threadMain.Stop();
	m_threadPool.Stop();
}

int RTSPServer::threadWorker()
{
	EAddress clientAddr;
	ESocket clientSocket = m_listenSocket.Accept(clientAddr);
	if (clientSocket != INVALID_SOCKET)
	{
		m_clients.PushBack(clientSocket);
		m_threadPool.DispatchWorker(::ThreadWorker(this, (FUNCTYPE)&RTSPServer::ThreadSessionWorker));
	}
	return 0;
}

int RTSPServer::ThreadSessionWorker()
{
	// 处理客户端请求 接收数据 
	ESocket client;
	Buffer buffer(1024*16);		
	int len = client.Recv(buffer);
	if (len < 0)
	{
		client.Close();
		return -1;
	}
	buffer.resize(len);
	RTSPRequest request = AnalyzeRequest(buffer);
	RTSPReply reply = HandleRequest(request);

	client.Send(reply.ToBuffer());
	return 0;
}
