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
		RTSPSession session(clientSocket);
		
		m_lstSession.PushBack(session);
		m_threadPool.DispatchWorker(::ThreadWorker(this, (FUNCTYPE)&RTSPServer::ThreadSessionWorker));
	}
	return 0;
}

int RTSPServer::ThreadSessionWorker()
{
	// 处理客户端请求 接收数据 
	RTSPSession session ;
	if (m_lstSession.PopFront(session)) {
		return session.PickSessionAndHandleRequest();
	}
	return -1;
}

int RTSPSession::PickSessionAndHandleRequest()
{
	Buffer buffer = Pick();
	if (buffer.empty()) {
		return -1;
	}
	RTSPRequest request = ParseRequest(buffer);
	RTSPReply reply = HandleRequest(request);
	m_clientSocket.Send(reply.ToBuffer());
	return 0;
}

Buffer RTSPSession::PickLine( Buffer& buffer)
{
	return Buffer();
}

Buffer RTSPSession::Pick()
{
	Buffer result;
	int ret = 1;
	Buffer buffer;
	while (ret > 0) {
		buffer.resize(1024 * 10);
		buffer.Zero();// 清空缓冲区
		ret = m_clientSocket.Recv(buffer);
		if (ret > 0) {
			buffer.resize(ret);
			result += buffer;
			UINT val = *(UINT*)(result.size() - 4 + (char*)result);
			if (val == 0x0D0A0D0A) {
				// 接收完毕
				break;
			}
		}
		else if (ret == 0) {
			// 客户端关闭连接
			m_clientSocket.Close();
			return result;
		}
		else {
			// 接收错误
			m_clientSocket.Close();
			return result;
		}
	}
	return result;
}

RTSPRequest RTSPSession::ParseRequest( const Buffer& buffer)
{
	Buffer data=buffer;
	RTSPRequest request;
	Buffer line = PickLine(data);
	Buffer method(32), url(256), version(32),seq(32);
	if (sscanf(line.c_str(), "%s %s %s", method, url, version) < 3) {
		TRACE("ParseRequest: invalid request line: %s\n", line.c_str());
		return request;
	}
	line = PickLine(data);
	if (sscanf(line.c_str(), "Seq: %s", seq) < 1) {
		TRACE("ParseRequest: invalid seq line: %s\n", line.c_str());
		return request;
	}
	request.SetMethod(method);
	request.SetURL(url);
	request.SetSeq(seq);
	if ((strcmp(method, "OPTIONS") == 0) || (strcmp(method, "DESCRIBE") == 0)) {
		return request;
	}
	else if (strcmp(method, "SETUP") == 0) {
		do {
			line = PickLine(data);
			if (strstr((const char*)line, "client_port=") == NULL)continue;
			break;
		} while (line.size() > 0);
		int port[2] = { 0,0 };
		if (sscanf(line, "Transport: RTP/AVP;unicast;client_port=%d-%d\r\n", port, port + 1) == 2) {
			request.SetClientRTPPort(port);
			return request;
		}
	}
	else if ((strcmp(method, "PLAY") == 0) || (strcmp(method, "TEARDOWN") == 0)) {
		line = PickLine(data);
		Buffer session(64);
		if (sscanf(line, "Session: %s\r\n", (char*)session) == 1) {
			request.SetSession(session);
			return request;
		}
	}
	return request;
}

RTSPReply RTSPSession::HandleRequest(const RTSPRequest& request)
{
	return RTSPReply();
}

 void RTSPRequest::SetMethod(const Buffer& method) {
	if (method == "OPTIONS") m_method = 0;
	else if (method == "DESCRIBE") m_method = 1;
	else if (method == "SETUP") m_method = 2;
	else if (method == "PLAY") m_method = 3;
	else if (method == "PAUSE") m_method = 4;
	else if (method == "TEARDOWN") m_method = 5;
}

void RTSPRequest::SetURL(const Buffer& url) {
	m_url = url;
}

void RTSPRequest::SetSeq(const Buffer& url)
{}
void RTSPRequest::SetSession(const Buffer& session)
{
	m_sessionID = session;
}

void RTSPRequest::SetClientRTPPort(int* port)
{
	if (port) {
		m_client_port[0] = port[0];
		m_client_port[1] = port[1];
	}
}
