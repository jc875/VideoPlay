#pragma once
#include<WinSock2.h>
#include <string>

class Buffer :public std::string
{
public:
	Buffer() :std::string() {}
	Buffer(size_t size) :std::string() { resize(size); }
	Buffer(const std::string& str) :std::string(str) {}
	Buffer(const char* str) :std::string(str) {}
	Buffer(const char* str, size_t length)
		:std::string() {
		resize(length);
		memcpy((char*)c_str(), str, length);
	}
	Buffer(const char* begin, const char* end) :std::string() {
		long int len = end - begin;
		if (len > 0) {
			resize(len);
			memcpy((char*)c_str(), begin, len);
		}
	}
	operator void* () { return (char*)c_str(); }
	Buffer& operator=(const Buffer& other) {
		if (this != &other) {
			*(std::string*)this = other;
		}
		return *this;
	}
	operator char* () { return (char*)c_str(); }
	operator unsigned char* () { return (unsigned char*)c_str(); }
	operator char* () const { return (char*)c_str(); }
	operator const char* () const { return c_str(); }
	operator const void* () const { return c_str(); }
	void Update(const char* str, size_t length) {
		resize(length);
		memcpy((char*)c_str(), str, length);
	}
	void Zero() {
		memset((char*)c_str(), 0, size());
	}
};
class Socket
{
public:
	//nType 0: TCP 1: UDP
	Socket(int nType) {
		m_socket = INVALID_SOCKET;
		if (nType == 0) {
			m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		}
		else if (nType == 1) {
			m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		}
	}
	void Close() {
		if (m_socket != INVALID_SOCKET) {
			closesocket(m_socket);
			m_socket = INVALID_SOCKET;
		}
	}
	operator SOCKET() { return m_socket; }
private:
	SOCKET m_socket;
};

class SocketIniter {
public:
	SocketIniter() {
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2, 2), &wsaData);
	}
	~SocketIniter() {
		WSACleanup();
	}
};

class EAddress
{
public:
	EAddress() {
		m_nPort = 0;
		memset(&m_addr, 0, sizeof(m_addr));
		m_addr.sin_family = AF_INET;
		m_addr.sin_port = 0;
		m_addr.sin_addr.s_addr = INADDR_ANY;

	}
	EAddress(const std::string& strIP, int nPort) : m_strIP(strIP), m_nPort(nPort) {}
	EAddress(const EAddress& other) {
		if (this != &other) {
			m_strIP = other.m_strIP;
			m_nPort = other.m_nPort;
			memcpy(&m_addr, &other.m_addr, sizeof(m_addr));
		}
	}
	EAddress& operator=(const EAddress& other) {
		if (this != &other) {
			m_strIP = other.m_strIP;
			m_nPort = other.m_nPort;
			memcpy(&m_addr, &other.m_addr, sizeof(m_addr));
		}
		return *this;
	}
	~EAddress() {}
	void Update(const std::string& strIP, short nPort) {
		m_strIP = strIP;
		m_nPort = nPort;
		m_addr.sin_family = AF_INET;
		m_addr.sin_port = htons(m_nPort);
		m_addr.sin_addr.s_addr = inet_addr(m_strIP.c_str());
	}
	operator sockaddr_in*()  {
		
		return &m_addr;
	}

private:
	std::string m_strIP;
	short m_nPort;
	sockaddr_in m_addr;
};

class ESocket {
public:
	ESocket(bool nType=true): m_socket(std::make_shared<Socket>(nType)) {
		m_isTCP = nType;
	}
	ESocket(const ESocket& sock) : m_socket(sock.m_socket), m_isTCP(sock.m_isTCP)
	{}  // 拷贝:只是把计数器 +1,不复制句柄
	ESocket(SOCKET sock, bool nType) : m_socket(std::make_shared<Socket>(nType)), m_isTCP(nType)
	{
		*m_socket = sock;
	} // 直接用 SOCKET 初始化 ESocket
	ESocket& operator=(const ESocket& sock) 
	{ 
		if (this != &sock) 
	{ 
		m_socket = sock.m_socket; 
		m_isTCP = sock.m_isTCP;
	} 
	return *this; 
	} // 赋值:只是把计数器 +1,不复制句柄)
	~ESocket() { m_socket.reset(); }               // 析构:计数器 -1,count 没到 0 就绝不关闭
	operator SOCKET() { return *m_socket; }  // 让 ESocket 可以直接当 SOCKET 使用
	int Bind(const EAddress& addr) {
		if (m_socket == nullptr) {
			m_socket.reset(new Socket(m_isTCP ? 0 : 1));
			}
		return bind(*m_socket, (sockaddr*)&addr, sizeof(addr));
	}
	int Listen(int nBacklog=5) {
		return listen(*m_socket, nBacklog);
	}
	int Connect(const EAddress& addr) {
		return connect(*m_socket, (sockaddr*)&addr, sizeof(addr));
	}
	ESocket Accept(EAddress& addr) {
		
		int addrLen = sizeof(addr);
		SOCKET s=  accept(*m_socket, (sockaddr*)&addr, &addrLen);
		return ESocket(s, m_isTCP);
	}
	void Close() {
		m_socket.reset();
		
	}
	int Recv(Buffer& buffer) {
		return recv(*m_socket, buffer, buffer.size(), 0);
	}
	int Send(const Buffer& buffer) {
		return send(*m_socket, buffer, buffer.size(), 0);
	}
private:
	std::shared_ptr<Socket> m_socket;
	bool m_isTCP;
};