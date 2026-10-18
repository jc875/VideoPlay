#pragma once
#include "Socket.h"
#include <string>
#include "EdoyunThread.h"
#include <map>
#include "CEdoyunQueue.h"
#include"RTPHelper.h"
#include"MideaFile.h"   // ⚠️ 数据面取帧器:PLAY 后 UdpWorker 从这里逐帧读 H264

// ==================== RTSP 推流回调 ====================
// 会话层(RTSPSession)只懂协议、不懂业务:收到 PLAY 后"怎么推流"要交回服务器层决定。
// C 风格函数指针 + thiz 对象指针(普通函数指针不能指向成员函数,static 成员函数才匹配,
// 再靠 thiz 参数手动"补回"this——参考版同款设计,解耦会话层与推流业务)。
class RTSPSession;
class RTSPServer;
typedef void (*RTSPPLAYCB)(RTSPServer* thiz, RTSPSession& session);
// ==================== RTSPRequest:一次客户端请求的结构化表示 ====================
// 作用:把收到的原始 RTSP 报文解析成"方法 + URL + CSeq + Session + 客户端 RTP 端口",
//      供 HandleRequest 按方法构造回复。
// 与参考版差异:参考用 EBuffer 存字段,这里用 std::string(语义等价,更省事)。
class RTSPRequest
{
public:
	RTSPRequest() : m_method(-1) {}                    // -1 = 解析失败
	RTSPRequest(int method) : m_method(method) {}
	RTSPRequest(const RTSPRequest& other);
	RTSPRequest& operator=(const RTSPRequest& other);
	~RTSPRequest() {}

	void SetMethod(const Buffer& method);   // 把 "OPTIONS" 等字符串翻译成 0~5 枚举
	void SetURL(const Buffer& url);
	void SetSeq(const Buffer& seq);         // 解析 CSeq 头(原实现为空!导致回复永远没有 CSeq)
	void SetSession(const Buffer& session);
	void SetClientRTPPort(int* port);       // Transport 头里的 client_port=偶-奇

	// 访问器:HandleRequest / PickSessionAndHandleRequest 要读这些字段
	int method() const { return m_method; }
	const std::string& url() const { return m_url; }
	const std::string& seq() const { return m_seq; }
	const std::string& session() const { return m_sessionID; }
	int clientPort(int index = 0) const { return m_client_port[index]; } // SETUP 协商的客户端 RTP 端口

private:
	int m_method;             // -1 失败 | 0 OPTIONS 1 DESCRIBE 2 SETUP 3 PLAY 4 PAUSE 5 TEARDOWN
	std::string m_url;        // 请求的资源路径,如 /live
	std::string m_seq;        // CSeq 序号,回复必须原样带回(协议要求)
	int m_client_port[2];     // 客户端想收 RTP 的端口对
	std::string m_sessionID;  // Session 头(断线重连时按它找回会话)
};

// ==================== RTSPReply:一次服务器回复 ====================
// 作用:按方法拼装 "RTSP/1.0 200 OK + CSeq + 方法特有头 + 空行" 的完整报文。
// 之前 ToBuffer() 是空实现,导致服务器收到请求却什么都发不回去。
class RTSPReply
{
public:
	RTSPReply() : m_method(-1) {}
	RTSPReply(int method) : m_method(method) {}
	RTSPReply(const RTSPReply& other);
	RTSPReply& operator=(const RTSPReply& other);
	~RTSPReply() {}

	Buffer ToBuffer();  // 把成员拼成完整报文(核心组装逻辑,参考版 toBuffer)

	void SetMethod(int method);
	void SetSeq(const std::string& seq);
	void SetOptions(const std::string& options);  // OPTIONS 通告的方法列表
	void SetSdp(const std::string& sdp);          // DESCRIBE 的 SDP 体
	void SetClientRTPPort(int* port);
	void SetServerRTPPort(int* port);
	void SetSession(const std::string& session);

private:
	int m_method;            // 对应请求方法,决定回复体长什么样
	std::string m_seq;       // CSeq 回显
	std::string m_options;   // "OPTIONS, DESCRIBE, SETUP, ..." 
	std::string m_sdp;       // DESCRIBE 回复体
	int m_client_port[2];    // 协商结果:客户端端口
	int m_server_port[2];    // 协商结果:服务器端口(参考版写死 55000/55001)
	std::string m_session;   // Session 号(要么请求带回的,要么本会话新生成的)
};

// ==================== RTSPSession:一条客户端连接 ====================
// 作用:会话主循环【收完整请求 → 逐行解析 → 构造回复 → 发回】,
//      并持有本连接的会话 ID 和客户端 RTP 端口。
// 之前 PickLine/HandleRequest 都是空实现,会话链路是断的。
class RTSPSession
{
public:
	RTSPSession();                         // 生成唯一会话 ID(参考版用 UUID)
	RTSPSession(ESocket clientSocket);     // accept 到连接时构造
	RTSPSession(const RTSPSession& other); // 队列要拷贝;ESocket 是 shared_ptr 语义,浅拷贝安全
	RTSPSession& operator=(const RTSPSession& other);
	~RTSPSession() {}

	int PickSessionAndHandleRequest(RTSPPLAYCB cb, RTSPServer* thiz);  // 会话主入口:循环收/解析/回复,直到断连
	// ⚠️ 回调参数:PLAY 请求到达时,会话层调用 cb(thiz, *this) 把推流交回服务器层(参考版同款)。
	Buffer PickLine(Buffer& buffer);       // 从 buffer 取出一行(到 '\n'),剩余回填(原空实现)
	Buffer Pick();                         // 收一个完整请求(直到末尾 \r\n\r\n)
	RTSPRequest ParseRequest(const Buffer& buffer);  // 解析请求行 + CSeq + 各方法特有头
	RTSPReply HandleRequest(const RTSPRequest& request);  // 按方法构造回复(原空实现)
	EAddress GetClientUDPAddress() const;  // 推流目标地址 = TCP 对端 IP + SETUP 协商的客户端 RTP 端口

private:
	std::string m_sessionID;  // 本会话唯一 ID,回给客户端的 Session 头
	ESocket m_clientSocket;   // 客户端 socket(shared_ptr 封装,拷贝只加引用计数)
	short m_port;             // SETUP 时记录的客户端 RTP 端口(PLAY 发数据时要用)
};

// ==================== RTSPServer:服务器 ====================
// 作用:bind/listen → accept 主线程 → 会话入队 → 工作线程池取出并处理。
// ⚠️ 之前 Start() 没调 m_threadPool.Invoke():工作线程根本没启动,
//    DispatchWorker 只是把任务挂上去,线程不跑,会话永远无人处理(已修)。
class RTSPServer : public ThreadFuncBase
{
public:
	RTSPServer() : m_listenSocket(0), m_address(), m_status(0), m_threadPool(10) {
		// 主线程的任务单:accept 循环
		m_threadMain.UpdateWorker(::ThreadWorker(this, (FUNCTYPE)&RTSPServer::threadWorker));
		// 数据面取帧器:打开 H264 裸流(参考版同款,相对路径=exe 工作目录)。
		// ⚠️ 失败不致命:Open 失败后 ReadOneFrame 返回空,UdpWorker 自然不发,只留 TRACE 便于诊断。
		int nRet = m_h264File.Open("./test.h264");
		TRACE("MideaFile Open('./test.h264') = %d\n", nRet);
	}
	RTSPServer(int nPort);   // 指定端口构造(补实现,之前只声明没定义,会被链接器报错)
	int Init(const std::string& strIP = "0.0.0.0", short nPort = 554);
	~RTSPServer();
	int Start();             // 启动 accept 主线程 + 工作线程池
	void Stop();

protected:
	int threadWorker();          // 主线程循环体:accept → RTSPSession → 入队 → 派工
	int ThreadSessionWorker();   // 工作线程任务:PopFront 取会话 → 处理请求
	static void PlayCallBack(RTSPServer* thiz, RTSPSession& session);  // 会话层 PLAY 时回调(static 才能塞进函数指针)
	void UdpWorker(const EAddress& client);  // 数据面推流:循环 读帧→封 RTP→发 UDP

private:
	ESocket m_listenSocket;
	EAddress m_address;
	int m_status;                              // 0 未启动 1 启动中 2 运行中 3 停止中
	EdoyunThread m_threadMain;                 // accept 主线程
	EdoyunThreadPool m_threadPool;             // 处理会话的工作线程池(10 个)
	static SocketIniter socketIniter;          // 静态成员:程序启动自动 WSAStartup(定义在 .cpp)
	CEdoyunQueue<RTSPSession> m_lstSession;    // 会话队列(IOCP 串行化,线程安全)
	RTPHelper m_rtpHelper;                      // RTP 助手(封装和发送 RTP 包)
	MideaFile m_h264File;                       // H264 裸流取帧器(数据面源)
};
