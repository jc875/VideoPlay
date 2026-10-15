#pragma once
#include<WinSock2.h>
#pragma comment(lib, "ws2_32.lib")   // ⚠️ 补:Winsock API 导入库;vcxproj 没配 AdditionalDependencies,不链会 LNK2019
#include <string>
#include <memory>   // ⚠️ 补:ESocket 用了 std::shared_ptr,之前漏 include,在部分编译单元里会 C2065 m_socket 未声明

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
	// ⚠️ 补(参考版同款):包装一个已存在的 SOCKET 句柄(accept 返回的)。
	//    没有它,ESocket(SOCKET,bool) 里 *m_socket = sock 会把句柄当"类型标志"走 Socket(int) 构造,
	//    真实句柄(如 264)≠0/1 → m_socket 变 INVALID_SOCKET → Recv/Send 全失败、不回包。
	Socket(SOCKET s) {
		m_socket = s;
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
	// ⚠️ 以下两个 + size() 是对齐参考版补的:
	//   - operator const sockaddr* () const:const EAddress 传给 bind/connect(第二参是 const sockaddr*)必需,原来没有导致 C2440
	//   - operator sockaddr* ():accept 第二参是 sockaddr*(非 const),原来只有 sockaddr_in* 版本
	//   - size():bind/connect 第三参长度(原来用 sizeof(addr)=整个对象大小,错;应为 sockaddr_in 大小)
	operator const sockaddr* () const { return (sockaddr*)&m_addr; }
	operator sockaddr* () { return (sockaddr*)&m_addr; }
	int size() const { return sizeof(sockaddr_in); }

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
		// ⚠️ 修复:原来写 (sockaddr*)&addr 传 EAddress 对象地址(开头是 std::string),
		//    bind 读到 sin_family 是 string 的字节 → WSAEAFNOSUPPORT(10047) → 端口永远监听不上。
		//    参考版写法:addr 隐式转 const sockaddr*(指向 &m_addr),长度用 addr.size()。
		return bind(*m_socket, addr, addr.size());
	}
	int Listen(int nBacklog=5) {
		return listen(*m_socket, nBacklog);
	}
	int Connect(const EAddress& addr) {
		// ⚠️ 同 Bind 修复:取 m_addr 地址,不是对象地址
		return connect(*m_socket, addr, addr.size());
	}
	ESocket Accept(EAddress& addr) {
		// ⚠️ 同 Bind 修复:accept 第二参必须是 sockaddr 地址(→&m_addr),第三参是它的长度
		int addrLen = addr.size();
		SOCKET s=  accept(*m_socket, addr, &addrLen);
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