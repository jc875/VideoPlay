#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS   // inet_addr 等旧 Winsock API 弃用告警:VC2026 把它当错误(Socket.h 里有,必须在 include 前定义)
#define _CRT_SECURE_NO_WARNINGS           // fopen/fread 等旧 CRT 函数安全告警(与 RTSPServer.cpp 顶部同款)
#include "Socket.h"   // Buffer 定义所在(参考版 base.h 定义 EBuffer;用户工程 Buffer 在 Socket.h,base.h 为空)

// MideaFile:逐帧读取 H264 裸流文件(NALU),供 RTSP 数据面(RTPHelper 分片打包)取帧使用。
// 对照参考工程 MideaFile.h/.cpp 实现,EBuffer → Buffer。
// 生活类比:一个"读录像带机器"——每次 ReadOneFrame() 吐出一段完整画面(一个 NALU),
// 读到带尾返回空,Reset() 倒带重播。
class MideaFile
{
public:
	MideaFile();
	~MideaFile();
	// 打开 H264 裸流文件;nType=96 表示 H264/90000(与 SDP 的 rtpmap:96 H264/90000 对应)。
	// 失败返回 -1(文件不存在/打不开)。
	int Open(const Buffer& filepath, int nType = 96);
	// 读一帧 NALU(包含起始码);返回 Buffer 的 size()==0 表示没有帧了(读到底)。
	Buffer ReadOneFrame();
	// 关闭文件(析构函数也会调;重复调用安全)。
	void Close();
	// 回到文件开头,配合 ReadOneFrame 从头再播(断流重推时用)。
	void Reset();
private:
	// 从当前文件位置向后找 NALU 起始码(00 00 01 三字节 或 00 00 00 01 四字节)。
	// 返回起始码第一个字节的文件偏移;找不到返回 -1;headsize 输出起始码字节数(3 或 4)。
	long FindH264Head(int& headsize);
	Buffer ReadH264Frame();   // 按 H264 格式读一帧(含起始码)
private:
	long m_size;       // 文件总字节数(Open 时用 ftell 拿到,读最后一帧没有下一个头时兜底)
	FILE* m_file;
	Buffer m_filepath;
	int m_type;        // 负载类型:96 = H264/90000(参考版默认;其它类型暂不支持)
};
