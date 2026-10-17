// RTPHelper.cpp —— RTP 封装与发送实现(数据面核心)
// ⚠️ 编译前提:中文注释必须 UTF-8 BOM(否则 936 代码页误解注释 → C2447/C2039);
//   include Socket.h 前定义两个宏(Socket.h 有 inet_addr,VC2026 弃用告警当错误)
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include "RTPHelper.h"
#include <Windows.h>   // Sleep(限速)
#include <cstring>     // memcmp/memcpy

#define RTP_MAX_SIZE 1300   // 载荷上限:以太网 MTU 1500 - IP头20 - UDP头8 - RTP头12 ≈ 1460,
                            // 取 1300 留余量,避免 IP 分片(分片会导致重传/丢包放大)

RTPHeader::RTPHeader()
{
	cc = 0;
	extension = 0;
	padding = 0;
	version = 2;
	payloadType = 96;      // H264 动态负载类型(与 SETUP/DESCRIBE 的 rtpmap:96 H264/90000 对应)
	marker = 0;
	sequenceNumber = 0;
	timestamp = 0;
	ssrc = 0x98765432;     // 固定 SSRC(参考版同款;多路流时应唯一,本项目单路无所谓)
	memset(csrc, 0, sizeof(csrc));
}

RTPHeader::operator Buffer()
{
	RTPHeader header = *this;                  // 拷副本,不改原对象(默认拷贝构造=逐成员拷贝)
	// ── 多字节数值字段转网络序(小端机器=高低字节交换;大端机器=no-op)──
	header.sequenceNumber = htons(header.sequenceNumber);
	header.timestamp = htonl(header.timestamp);
	header.ssrc = htonl(header.ssrc);
	// ⚠️ 修参考版坑 ②:CSRC 数组每个元素是 4 字节数值,cc>0 时必须 htonl(参考版漏转)。
	//    本项目 cc 恒 0 不触发,但正确性要补上。
	for (int i = 0; i < header.cc; i++)
		header.csrc[i] = htonl(header.csrc[i]);
	// ⚠️ 修参考版坑 ③:长度 = 12 固定头 + 4*cc。
	//    原骨架用 sizeof(RTPHeader) 会把整个 csrc[15](60B)+填充都拷出去,且不管 cc 是多少。
	int size = 12 + 4 * header.cc;
	return Buffer((const char*)&header, size);
}

RTPFrame::operator Buffer()
{
	Buffer result;
	Buffer head = m_head;                      // 调 RTPHeader::operator Buffer()(已转网络序)
	result.append((const char*)head, head.size());
	result.append((const char*)m_pyload, m_pyload.size());
	return result;
}

RTPHelper::RTPHelper() : m_udp(true)           // true → Socket(1)=UDP(见 Socket.h)
{
	// 绑定源端口 55000(与 SETUP 回复的 server_port=55000-55001 一致);
	// 0.0.0.0 = 监听任意本地地址。失败不致命,sendto 仍可能发出(取决于系统路由)。
	m_udp.Bind(EAddress("0.0.0.0", 55000));
}

int RTPHelper::GetFrameSepSize(Buffer& frame)
{
	// H264 起始码两种:00 00 00 01(4字节,标准)或 00 00 01(3字节,少见)
	uint8_t buf[] = { 0, 0, 0, 1 };
	if (memcmp(frame, buf, 4) == 0) return 4;
	return 3;
}

int RTPHelper::SendFrame(const Buffer& frame, const EAddress& client)
{
	// UDP 无连接:每次 sendto 指定目标(客户端 RTP 端口,来自 SETUP 的 client_port)
	return sendto(m_udp, frame, (int)frame.size(), 0, client, client.size());
}

int RTPHelper::SendMediaFrame(RTPFrame& rtpframe, Buffer& frame, const EAddress& client)
{
	size_t frame_size = frame.size();
	int sepsize = GetFrameSepSize(frame);          // 起始码长度(3 或 4)
	frame_size -= sepsize;
	uint8_t* pFrame = sepsize + (uint8_t*)frame;   // 跳过起始码 → pFrame[0] = NALU 头

	if (frame_size > RTP_MAX_SIZE) {
		// ═══ 大帧:FU-A 分片(每片载荷 = FU Indicator 1B + FU Header 1B + 分片数据)═══
		uint8_t nalu = pFrame[0] & 0x1F;           // NALU 头低5位 = 真实帧类型(5=IDR/1=P/7=SPS/8=PPS)
		size_t restsize = frame_size % RTP_MAX_SIZE;
		size_t count = frame_size / RTP_MAX_SIZE;
		for (size_t i = 0; i < count; i++) {
			rtpframe.m_pyload.resize(RTP_MAX_SIZE + 2);
			// ⚠️ 修参考版坑 ①:FU Indicator = F(0) | NRI(原 NALU 头 bit6-5,参考版写死 3) | Type=28(FU-A 魔数)
			((uint8_t*)rtpframe.m_pyload)[0] = (pFrame[0] & 0x60) | 28;
			// FU Header 低5位 = 原 NALU 类型;S(第一片)/E(最后一片)由 i 决定
			((uint8_t*)rtpframe.m_pyload)[1] = nalu;
			if (i == 0)
				((uint8_t*)rtpframe.m_pyload)[1] |= 0x80;          // S=1
			else if ((restsize == 0) && (i == count - 1))
				((uint8_t*)rtpframe.m_pyload)[1] |= 0x40;          // E=1(整除时最后一片在这)
			// 分片数据从 NALU 头之后开始(NALU 头信息已拆进 FU 两字节)
			memcpy(2 + (uint8_t*)rtpframe.m_pyload, pFrame + RTP_MAX_SIZE * i + 1, RTP_MAX_SIZE);
			SendFrame(rtpframe, client);
			rtpframe.m_head.sequenceNumber++;
		}
		if (restsize > 0) {
			// 尾片(不整除时):E=1,无 S
			rtpframe.m_pyload.resize(restsize + 2);
			((uint8_t*)rtpframe.m_pyload)[0] = (pFrame[0] & 0x60) | 28;
			((uint8_t*)rtpframe.m_pyload)[1] = 0x40 | nalu;        // E=1
			memcpy(2 + (uint8_t*)rtpframe.m_pyload, pFrame + RTP_MAX_SIZE * count + 1, restsize);
			SendFrame(rtpframe, client);
			rtpframe.m_head.sequenceNumber++;
		}
	}
	else {
		// ═══ 小帧:完整 NALU(头+数据)直接放载荷,不加 FU 两字节 ═══
		rtpframe.m_pyload.resize(frame.size() - sepsize);
		memcpy(rtpframe.m_pyload, pFrame, frame.size() - sepsize);
		SendFrame(rtpframe, client);
		rtpframe.m_head.sequenceNumber++;
	}

	// 时间戳累加:每帧 + 90000/24(90kHz 采样时钟 / 24fps,与 SDP framerate:24 对应)。
	// ⚠️ 参考版遗留不一致:Sleep(1000/30) 按 30fps 限速,但时间戳按 24fps 累加——
	//    实际码率会略慢于 24fps。保持参考版行为,不擅自改;真要做准应统一帧率。
	rtpframe.m_head.timestamp += 90000 / 24;
	Sleep(1000 / 30);
	return 0;
}
