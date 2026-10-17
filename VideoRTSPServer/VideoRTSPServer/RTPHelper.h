// RTPHelper.h —— RTP 数据面:把 H264 NALU 封装成 RTP 包发送
// 对照参考工程 RTPHelper.h/.cpp 实现,EBuffer→Buffer、BYTE→uint8_t,
// 并修掉参考版的三个坑(见 RTPHelper.cpp 注释):
//   ① FU Indicator 的 NRI 写死 3 → 改为取原 NALU 头 bit6-5
//   ② CSRC 数组不转网络序 → 每个元素 htonl
//   ③ operator Buffer() 用 sizeof(RTPHeader) → 改 12+4*cc 动态长度
// 依赖:Buffer(base.h)、EAddress/ESocket(Socket.h)
#pragma once
#include "Socket.h"
#include <cstdint>   // uint16_t/uint32_t

// RTP 头(固定 12 字节 + CSRC 列表)。位域块 16bit = RTP 头前 2 字节。
// MSVC 小端位域从 bit0 开始分配,声明顺序(cc→ext→pad→version)让 version 落在
// 字节0 的 bit6-7(字节最高2位)= RTP 规范 V 的位置 → 拼出的字节无需字节序转换;
// sequenceNumber/timestamp/ssrc 是数值字段,operator Buffer() 里 htons/htonl。
// 生活类比:位域块=拼好的"快递单前两格",数值字段=要按网络规矩排的数字。
class RTPHeader
{
public:
	// ── 位域块(RTP 头字节0-1)──
	unsigned short cc : 4;           // CSRC 数量(0-15),本项目恒 0
	unsigned short extension : 1;    // 扩展位,恒 0
	unsigned short padding : 1;      // 填充位,恒 0
	unsigned short version : 2;      // RTP 版本,固定 2
	unsigned short payloadType : 7;  // 负载类型,96 = H264(与 SDP rtpmap 一致)
	unsigned short marker : 1;       // 标记位(参考版未用,恒 0)
	// ── 多字节数值字段(必须转网络序)──
	uint16_t sequenceNumber;         // 16 位序号:每发一包 +1,接收端排序/去重
	uint32_t timestamp;              // 32 位时间戳:90kHz 采样时钟,每帧 + 90000/帧率
	uint32_t ssrc;                   // 32 位同步源标识:标识"这是同一路流"
	uint32_t csrc[15];               // CSRC 列表(cc>0 时有效),本实现恒空

public:
	RTPHeader();
	// 转成可发送的字节序列:数值字段转网络序,长度 = 12 + 4*cc(不是 sizeof!)
	operator Buffer();
};

// 一帧 RTP 数据 = RTP 头 + 载荷(H264 数据)
class RTPFrame
{
public:
	RTPHeader m_head;
	Buffer m_pyload;                 // 载荷:小帧=NALU 完整数据;分片=FU-A 单片数据
	operator Buffer();               // 头 + 载荷拼接成完整 RTP 包字节流
};

// 把 MideaFile 读出的 H264 帧(NALU,带起始码)封装成 RTP 包,发到客户端
class RTPHelper
{
public:
	RTPHelper();                     // 创建 UDP socket 并绑定源端口 55000
	~RTPHelper() {}
	// 发送一帧:
	//   - 帧 ≤1300B:完整 NALU(去起始码)直接放进载荷,一个包
	//   - 帧 >1300B:按 FU-A 分片成多个包(每片加 FU Indicator + FU Header)
	//   自动累加 serial/timestamp,并按 Sleep 限速(参考版同款 30fps 节流)
	int SendMediaFrame(RTPFrame& rtpframe, Buffer& frame, const EAddress& client);
private:
	int GetFrameSepSize(Buffer& frame);           // 起始码 4 字节(00 00 00 01)还是 3 字节
	int SendFrame(const Buffer& frame, const EAddress& client);  // sendto 发包
	ESocket m_udp;                                 // UDP socket(发 RTP 用,true=UDP)
};
