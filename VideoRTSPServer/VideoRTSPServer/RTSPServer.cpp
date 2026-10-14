// RTSPServer.cpp —— RTSP 服务器会话层实现
// 参考工程:桌面 音视频系统代码\VideoRTSPServer\RTSPServer.cpp
#define _CRT_SECURE_NO_WARNINGS              // 关闭 sscanf 等旧 CRT 函数安全告警(教学代码,参考工程同款)
#define _WINSOCK_DEPRECATED_NO_WARNINGS      // 关闭 inet_addr 等旧 Winsock API 弃用告警(必须在 include Socket.h 之前定义,VC2026 会把它当错误)
// 本轮补齐(对比你之前的骨架):
//   1. Start() 补 m_threadPool.Invoke()     —— 之前不 Invoke,工作线程没启动,会话永远不处理(硬 bug)
//   2. 补 static SocketIniter 定义          —— 之前只声明没定义,链接会报 unresolved external(硬 bug)
//   3. RTSPSession 生成唯一会话 ID(UUID)    —— 之前 m_sessionID 永远为空
//   4. PickLine 实现                        —— 之前空实现,请求行永远解析不出来
//   5. ParseRequest 修 "Seq:" → "CSeq:"    —— 协议头是 CSeq,之前永远匹配失败(硬 bug)
//   6. ParseRequest 补 PAUSE 分支           —— 枚举有 4:PAUSE,解析却没处理
//   7. HandleRequest 实现(六方法回复)       —— 之前空实现,什么都不回
//   8. RTSPReply::ToBuffer 实现             —— 之前空实现,拼不出报文
//   9. SetSeq 实现                          —— 之前空实现,回复没有 CSeq
//   10. RTSPServer(int nPort) 补定义        —— 之前只声明没定义
// 说明:数据面(H264 解析 + RTP 发送,参考的 MideaFile/RTPHelper/UdpWorker)本轮不做,留 TODO。

#include "RTSPServer.h"
#include <rpc.h>        // UuidCreate:生成唯一会话 ID
#include <cstdio>       // snprintf
#include <cstdlib>      // atoi
#pragma comment(lib, "rpcrt4.lib")

// 静态成员定义:程序启动时自动 WSAStartup,进程退出时 WSACleanup。
// ⚠️ 头文件里只是声明,必须在这里给一次定义,否则 LNK2001 链接错误。
SocketIniter RTSPServer::socketIniter;

// ---------- RTSPServer ----------

// Init:填地址 → bind 占端口 → listen 进入监听。
int RTSPServer::Init(const std::string& strIP, short nPort)
{
	m_address.Update(strIP, nPort);   // 把 IP/端口写进 sockaddr_in
	m_listenSocket.Bind(m_address);   // bind:把 socket 和端口绑定
	m_listenSocket.Listen(5);         // listen:开始监听,内核排队最多 5 个
	return 0;
}

// 带端口构造:方便 main 里一句 RTSPServer server(554);
RTSPServer::RTSPServer(int nPort)
{
	Init("0.0.0.0", nPort);
}

RTSPServer::~RTSPServer()
{
	Stop();
}

// Start:启动 accept 主线程 + 工作线程池。
// ⚠️ 必须调 m_threadPool.Invoke():它逐个 Start 池里的 EdoyunThread;
//    不启动的话,线程循环(m_bStatus=false)根本不跑,DispatchWorker 挂上去的任务无人执行。
int RTSPServer::Start()
{
	m_threadMain.Start();      // 启动主线程(跑 threadWorker:accept)
	m_threadPool.Invoke();     // 启动 10 个工作线程(等待被派发会话任务)
	return 0;
}

// Stop:关监听 socket → 停主线程 → 停工作线程池。
void RTSPServer::Stop()
{
	m_listenSocket.Close();    // 关监听 socket(不再 accept 新连接)
	m_threadMain.Stop();       // 停 accept 主线程
	m_threadPool.Stop();       // 停工作线程池
}

// 主线程循环体(threadWorker):accept 到新连接 → 包成 RTSPSession → 入队 → 派一个工作线程处理。
int RTSPServer::threadWorker()
{
	EAddress clientAddr;
	ESocket clientSocket = m_listenSocket.Accept(clientAddr);   // 阻塞等新连接
	if (clientSocket != INVALID_SOCKET)
	{
		RTSPSession session(clientSocket);   // 构造会话(内部生成唯一 Session ID)
		m_lstSession.PushBack(session);      // 入队(生产者)
		// 派一个空闲工作线程去处理这条会话
		m_threadPool.DispatchWorker(::ThreadWorker(this, (FUNCTYPE)&RTSPServer::ThreadSessionWorker));
	}
	return 0;
}

// 工作线程任务:从队列取一个会话,交给会话主循环处理(消费者)。
int RTSPServer::ThreadSessionWorker()
{
	RTSPSession session;
	if (m_lstSession.PopFront(session)) {
		return session.PickSessionAndHandleRequest();   // 阻塞处理完这条连接才返回
	}
	return -1;
}

// ---------- RTSPSession ----------

// 默认构造:生成唯一会话 ID。
// RTSP 协议要求 SETUP 后服务器返回 Session 头;客户端后续请求(PLAY/PAUSE/TEARDOWN)带回它,
// 服务器据此认出"这是同一条会话"(断线重连也能找回)。
// 参考版用 UuidCreate 生成,这里沿用。
RTSPSession::RTSPSession() : m_port(-1)
{
	UUID uuid;
	UuidCreate(&uuid);
	char szId[32] = { 0 };
	snprintf(szId, sizeof(szId), "%08X%08X", uuid.Data1, uuid.Data2);  // 拼成十六进制串当 ID
	m_sessionID = szId;
}

// 接受连接构造:同上,只是额外保存客户端 socket。
RTSPSession::RTSPSession(ESocket clientSocket) : m_clientSocket(clientSocket), m_port(-1)
{
	UUID uuid;
	UuidCreate(&uuid);
	char szId[32] = { 0 };
	snprintf(szId, sizeof(szId), "%08X%08X", uuid.Data1, uuid.Data2);
	m_sessionID = szId;
}

// 拷贝/赋值:ESocket 内部是 shared_ptr,拷贝=引用计数+1,安全;
// std::string/短整型直接拷。队列 PushBack 时走这里。
RTSPSession::RTSPSession(const RTSPSession& other)
{
	m_sessionID = other.m_sessionID;
	m_clientSocket = other.m_clientSocket;
	m_port = other.m_port;
}

RTSPSession& RTSPSession::operator=(const RTSPSession& other)
{
	if (this != &other) {
		m_sessionID = other.m_sessionID;
		m_clientSocket = other.m_clientSocket;
		m_port = other.m_port;
	}
	return *this;
}

// 会话主入口:循环【收请求 → 解析 → 构造回复 → 发回】,直到 send 失败或断连。
// 参考版是 do-while:一个连接可以连续发 OPTIONS→DESCRIBE→SETUP→PLAY 一串请求。
// SETUP 时顺手记下客户端 RTP 端口(m_port),PLAY 发数据要用。
int RTSPSession::PickSessionAndHandleRequest()
{
	int ret = -1;
	do {
		Buffer buffer = Pick();                       // 收一个完整请求(到 \r\n\r\n)
		if (buffer.size() <= 0) return -1;            // 空 = 客户端断开或出错
		RTSPRequest request = ParseRequest(buffer);   // 解析成结构化请求
		if (request.method() < 0) {
			TRACE("ParseRequest failed: %s\r\n", (char*)buffer);
			return -2;                                // 解析失败,不回了
		}
		RTSPReply reply = HandleRequest(request);     // 按方法构造回复
		ret = m_clientSocket.Send(reply.ToBuffer());  // 发回给客户端
		if (request.method() == 2) {                  // SETUP 完成 → 记住客户端 RTP 端口(PLAY 发数据要用)
			m_port = (short)request.clientPort(0);    // 客户端 RTP 端口对里的偶数端口
		}
	} while (ret >= 0);                              // send 成功就继续等下一条请求
	return ret;
}

// PickLine:从 buffer 里取出一行(到 '\n',含换行),剩余部分回填 buffer。
// 参考版 PickOneLine。之前空实现 → ParseRequest 拿不到请求行,全链路断。
// 实现:逐字节 append 到 '\n';i+1 之后是下一行开头。
Buffer RTSPSession::PickLine(Buffer& buffer)
{
	if (buffer.size() <= 0) return Buffer();
	Buffer result;
	int i = 0;
	for (; i < (int)buffer.size(); i++) {
		result.push_back(buffer.at(i));   // at() 避开 Buffer 的 operator char*() 引起的 [] 重载歧义(参考版同款)
		if (buffer.at(i) == '\n') break;  // 到行尾
	}
	Buffer temp = buffer.substr(i + 1); // 剩下的(不含已取走的 i+1 字节)
	buffer = temp;
	return result;
}

// Pick:循环 Recv,把数据攒起来,直到末尾是 "\r\n\r\n"(空行=请求头结束)。
// 参考版逻辑一致;⚠️ 不足:用 *(UINT*) 读结尾 4 字节判断,依赖小端字节序(Windows 全是小端,可接受)。
Buffer RTSPSession::Pick()
{
	Buffer result;
	int ret = 1;
	Buffer buffer;
	while (ret > 0) {
		buffer.resize(1024 * 10);       // 每次最多读 10KB
		buffer.Zero();                  // 清零(不然 Recv 后缓冲区末尾有旧数据)
		ret = m_clientSocket.Recv(buffer);
		if (ret > 0) {
			buffer.resize(ret);         // 只保留真正收到的字节
			result += buffer;
			if (result.size() >= 4) {
				// 看结尾 4 字节是不是 \r\n\r\n = 0x0D0A0D0A
				UINT val = *(UINT*)((char*)result + result.size() - 4);
				if (val == *(UINT*)"\r\n\r\n") break;   // 头收完了
			}
		}
		else {
			m_clientSocket.Close();     // 断连/出错,关连接
			break;
		}
	}
	return result;
}

// ParseRequest:把原始报文解析成 RTSPRequest。
// 结构:第一行 "方法 URL RTSP版本",第二行 "CSeq: n",然后按方法读特有头:
//   OPTIONS/DESCRIBE:够了
//   SETUP:找 Transport 行里的 client_port=a-b
//   PLAY/PAUSE/TEARDOWN:找 Session: xxx
RTSPRequest RTSPSession::ParseRequest(const Buffer& buffer)
{
	Buffer data = buffer;
	RTSPRequest request;
	Buffer line = PickLine(data);
	Buffer method(32), url(256), version(32), seq(32);

	// 请求行:"OPTIONS rtsp://... RTSP/1.0"
	// ⚠️ 注意:Buffer 继承 std::string,operator char*() 返回 c_str() 指针,
	//    sscanf 往里写是"教学写法"(参考版同款),严格说是未定义行为;正式代码应改用可写缓冲。
	// ⚠️ 可变参数(va_args)不会对类做隐式转换:method/url/version 必须显式 (char*) 强转,
	//    否则 sscanf 会把 Buffer 对象本身当缓冲区写(原代码漏了强转,是 UB)。
	if (sscanf(line.c_str(), "%s %s %s", (char*)method, (char*)url, (char*)version) < 3) {
		TRACE("ParseRequest: invalid request line: %s\n", line.c_str());
		return request;   // m_method 保持 -1,上层识别为解析失败
	}

	// CSeq 行:协议头是 "CSeq:",不是 "Seq:"。
	// ⚠️ 原代码写 "Seq: %s" 永远匹配失败(sscanf 从行首精确匹配,实际行以 'C' 开头),已修。
	line = PickLine(data);
	if (sscanf(line.c_str(), "CSeq: %s", (char*)seq) < 1) {
		TRACE("ParseRequest: invalid CSeq line: %s\n", line.c_str());
		return request;
	}

	request.SetMethod(method);
	request.SetURL(url);
	request.SetSeq(seq);    // 原实现为空!不存 CSeq,回复就没法回显

	if ((strcmp(method, "OPTIONS") == 0) || (strcmp(method, "DESCRIBE") == 0)) {
		return request;     // 这两个方法到此为止
	}
	else if (strcmp(method, "SETUP") == 0) {
		// 找 Transport 行:Transport: RTP/AVP;unicast;client_port=50000-50001
		do {
			line = PickLine(data);
			if (strstr((const char*)line, "client_port=") == NULL) continue;
			break;
		} while (line.size() > 0);
		int port[2] = { 0, 0 };
		if (sscanf(line, "Transport: RTP/AVP;unicast;client_port=%d-%d\r\n", port, port + 1) == 2) {
			request.SetClientRTPPort(port);
			return request;
		}
	}
	else if ((strcmp(method, "PLAY") == 0) || (strcmp(method, "PAUSE") == 0) || (strcmp(method, "TEARDOWN") == 0)) {
		// 这三个方法都带 Session 头(断线重连也要靠它)
		// ⚠️ 原代码只有 PLAY/TEARDOWN,漏了 PAUSE(枚举里明明有 4:PAUSE),已补
		line = PickLine(data);
		Buffer session(64);
		if (sscanf(line, "Session: %s\r\n", (char*)session) == 1) {
			request.SetSession(session);
			return request;
		}
	}
	return request;
}

// HandleRequest:按方法构造回复(核心业务逻辑)。
// 之前空实现 → 服务器收到请求但什么都不回。参考版 Reply 逻辑:
//   OPTIONS   → Public: 通告支持的方法
//   DESCRIBE  → 返回 SDP 描述(视频格式/端口/控制 URL)
//   SETUP     → 协商传输:回 client_port/server_port + Session
//   PLAY/PAUSE/TEARDOWN → 回 Session(实际数据收发在数据面)
RTSPReply RTSPSession::HandleRequest(const RTSPRequest& request)
{
	RTSPReply reply;
	reply.SetMethod(request.method());   // 告诉 ToBuffer 按哪个方法拼报文
	reply.SetSeq(request.seq());         // CSeq 原样回显(协议要求)
	// Session:请求带了就原样回(断线重连场景),否则回本会话新生成的 ID
	if (request.session().size() > 0) {
		reply.SetSession(request.session());
	}
	else {
		reply.SetSession(m_sessionID);
	}

	switch (request.method())
	{
	case 0: // OPTIONS:通告服务器支持哪些方法
		reply.SetOptions("OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, TEARDOWN");
		break;
	case 1: // DESCRIBE:返回 SDP(媒体描述,客户端据此知道流是什么格式)
	{
		std::string sdp;
		sdp += "v=0\r\n";                                   // SDP 版本
		sdp += "o=- " + m_sessionID + " 1 IN IP4 127.0.0.1\r\n"; // 会话发起者(用会话 ID 当名字,保证唯一)
		sdp += "t=0 0\r\n";                                 // 时间:一直有效
		sdp += "a=control:*\r\n";                           // 全局控制 URL
		sdp += "m=video 0 RTP/AVP 96\r\n";                  // 视频媒体,payload 类型 96
		sdp += "a=framerate:24\r\n";                        // 帧率 24
		sdp += "a=rtpmap:96 H264/90000\r\n";                // 96 号 payload = H264,时钟 90000Hz
		sdp += "a=control:track0\r\n";                      // 该媒体的控制 URL(track0)
		reply.SetSdp(sdp);
		break;
	}
	case 2: // SETUP:协商传输端口
	{
		int cp[2] = { request.clientPort(0), request.clientPort(1) };  // 客户端要的端口
		reply.SetClientRTPPort(cp);
		int sp[2] = { 55000, 55001 };                                 // 服务器端口(参考版写死)
		reply.SetServerRTPPort(sp);
		// Session 已在函数开头设置
		break;
	}
	case 3: // PLAY
	case 4: // PAUSE
	case 5: // TEARDOWN
		// 这三个只要 Session + 200 OK 就够。
		// TODO(数据面,本轮不做):PLAY 还要触发 RTP 数据发送——
		//   参考版 PlayCallBack → UdpWorker → MideaFile 读 H264 帧 → RTPHelper 打包 → UDP 发给客户端
		break;
	default:
		TRACE("HandleRequest: unknown method %d\r\n", request.method());
		break;
	}
	return reply;
}

// ---------- RTSPRequest 实现 ----------

// SetMethod:把方法字符串翻译成 0~5 枚举(和头文件注释一致)
void RTSPRequest::SetMethod(const Buffer& method) {
	if (method == "OPTIONS") m_method = 0;
	else if (method == "DESCRIBE") m_method = 1;
	else if (method == "SETUP") m_method = 2;
	else if (method == "PLAY") m_method = 3;
	else if (method == "PAUSE") m_method = 4;
	else if (method == "TEARDOWN") m_method = 5;
	else m_method = -1;   // 未知方法,标记失败
}

void RTSPRequest::SetURL(const Buffer& url) {
	m_url = url;          // Buffer → std::string 隐式转换
}

void RTSPRequest::SetSeq(const Buffer& seq) {
	m_seq = seq;          // 之前空实现!CSeq 必须存下来,回复要回显
}

void RTSPRequest::SetSession(const Buffer& session) {
	m_sessionID = session;
}

void RTSPRequest::SetClientRTPPort(int* port) {
	if (port) {
		m_client_port[0] = port[0];
		m_client_port[1] = port[1];
	}
}

// ---------- RTSPReply 实现 ----------

// ToBuffer:把成员拼成完整 RTSP 报文(之前空实现,服务器什么都发不出去)。
// 通用开头:"RTSP/1.0 200 OK\r\n" + "CSeq: n\r\n",然后按方法拼特有头,最后空行。
// 对照参考版 toBuffer;这里 PAUSE(4) 与 TEARDOWN 同样只带 Session。
Buffer RTSPReply::ToBuffer()
{
	Buffer result;
	result += "RTSP/1.0 200 OK\r\n";
	result += "CSeq: " + m_seq + "\r\n";      // CSeq 回显(协议要求,没有会被客户端丢弃)
	switch (m_method)
	{
	case 0: // OPTIONS
		result += "Public: " + m_options + "\r\n\r\n";
		break;
	case 1: // DESCRIBE
		result += "Content-Base: 127.0.0.1\r\n";
		result += "Content-Type: application/sdp\r\n";
		result += "Content-Length: " + std::to_string(m_sdp.size()) + "\r\n\r\n";  // SDP 体长度
		result += m_sdp;                       // 空行后就是 SDP 体
		break;
	case 2: // SETUP
		result += "Transport: RTP/AVP;unicast;client_port="
			+ std::to_string(m_client_port[0]) + "-" + std::to_string(m_client_port[1]);
		result += ";server_port=" + std::to_string(m_server_port[0]) + "-"
			+ std::to_string(m_server_port[1]) + "\r\n";
		result += "Session: " + m_session + "\r\n\r\n";
		break;
	case 3: // PLAY
		result += "Range: npt=0.000-\r\n";     // 从 0 开始播
		result += "Session: " + m_session + "\r\n\r\n";
		break;
	case 4: // PAUSE
	case 5: // TEARDOWN
		result += "Session: " + m_session + "\r\n\r\n";
		break;
	default:
		result += "\r\n";                      // 未知方法也给个空行收尾
		break;
	}
	return result;
}

void RTSPReply::SetMethod(int method) {
	m_method = method;
}

void RTSPReply::SetSeq(const std::string& seq) {
	m_seq = seq;
}

void RTSPReply::SetOptions(const std::string& options) {
	m_options = options;
}

void RTSPReply::SetSdp(const std::string& sdp) {
	m_sdp = sdp;
}

void RTSPReply::SetClientRTPPort(int* port) {
	if (port) {
		m_client_port[0] = port[0];
		m_client_port[1] = port[1];
	}
}

void RTSPReply::SetServerRTPPort(int* port) {
	if (port) {
		m_server_port[0] = port[0];
		m_server_port[1] = port[1];
	}
}

void RTSPReply::SetSession(const std::string& session) {
	m_session = session;
}

// ---------- RTSPRequest 拷贝/赋值 ----------

RTSPRequest::RTSPRequest(const RTSPRequest& other) {
	m_method = other.m_method;
	m_url = other.m_url;
	m_seq = other.m_seq;
	m_client_port[0] = other.m_client_port[0];
	m_client_port[1] = other.m_client_port[1];
	m_sessionID = other.m_sessionID;
}

RTSPRequest& RTSPRequest::operator=(const RTSPRequest& other) {
	if (this != &other) {
		m_method = other.m_method;
		m_url = other.m_url;
		m_seq = other.m_seq;
		m_client_port[0] = other.m_client_port[0];
		m_client_port[1] = other.m_client_port[1];
		m_sessionID = other.m_sessionID;
	}
	return *this;
}

// ---------- RTSPReply 拷贝/赋值 ----------

RTSPReply::RTSPReply(const RTSPReply& other) {
	m_method = other.m_method;
	m_seq = other.m_seq;
	m_options = other.m_options;
	m_sdp = other.m_sdp;
	m_client_port[0] = other.m_client_port[0];
	m_client_port[1] = other.m_client_port[1];
	m_server_port[0] = other.m_server_port[0];
	m_server_port[1] = other.m_server_port[1];
	m_session = other.m_session;
}

RTSPReply& RTSPReply::operator=(const RTSPReply& other) {
	if (this != &other) {
		m_method = other.m_method;
		m_seq = other.m_seq;
		m_options = other.m_options;
		m_sdp = other.m_sdp;
		m_client_port[0] = other.m_client_port[0];
		m_client_port[1] = other.m_client_port[1];
		m_server_port[0] = other.m_server_port[0];
		m_server_port[1] = other.m_server_port[1];
		m_session = other.m_session;
	}
	return *this;
}
