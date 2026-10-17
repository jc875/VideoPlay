#pragma once
#include"base.h"
class RTPHeader
{
public://位域先填充低位,字节为单位,RTP包不要超过1400字节
	unsigned short cc : 4;           // 信道计数
	
	unsigned short extension : 1;    // 扩展位
	unsigned short padding : 1;      // 填充位
	unsigned short version : 2;      // 版本号,固定为 2
	
	
	unsigned short payloadType : 7;  // 负载类型
	unsigned short marker : 1;       // 标记位
	
	uint16_t sequenceNumber;   // 序列号
	uint32_t timestamp;        // 时间戳
	uint32_t ssrc;             // 同步源标识符
	uint32_t csrc[15];          // 贡献源标识符列表,最多 15 个

public:
	RTPHeader();
	~RTPHeader();
	operator Buffer() {
		return Buffer((const char*)this, sizeof(RTPHeader));
	}

};

class RTPHelper
{
public:
	RTPHelper();
	~RTPHelper()
	{

	}
private:
	Buffer m_head;
	Buffer m_payload;
};

