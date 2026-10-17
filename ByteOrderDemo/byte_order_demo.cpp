// byte_order_demo.cpp —— 字节序(大小端)与 RTP 头位域演示
// 编译:cl /EHsc byte_order_demo.cpp /Fe:byte_order_demo.exe
// 运行:byte_order_demo.exe
// 结论预览(小端 x86):
//   int 0x12345678 内存: 78 56 34 12   → 低字节在前 = 小端
//   htonl 之后:       12 34 56 78      → 高字节在前 = 网络大端序
//   RTPHeader 位域拼出的前 2 字节: 80 60 → 恰好等于 RTP 规范(无需转换)
//   多字节字段 serial/timestamp/ssrc: 必须 htons/htonl 转网络序
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

// 与参考版 RTPHelper.h 的 RTPHeader 同款布局(去掉 csrc 数组简化)
struct RTPHeader
{
	// 位域块:16 bit,正好占 2 字节。MSVC 小端上从低位开始分配:
	//   低字节 = csrccount(bit0-3) | extension(bit4) | padding(bit5) | version(bit6-7)
	//   高字节 = pytype(bit0-6) | mark(bit7)
	unsigned short csrccount : 4;   // 0-15,CSRC 数量
	unsigned short extension : 1;   // 扩展位
	unsigned short padding : 1;     // 填充位
	unsigned short version : 2;     // 版本(2)
	unsigned short pytype : 7;      // 负载类型(96=H264)
	unsigned short mark : 1;        // 标记位
	unsigned short serial;          // 16 位序号 → 需 htons
	unsigned timestamp;             // 32 位时间戳 → 需 htonl
	unsigned ssrc;                  // 32 位同步源 → 需 htonl
};

// 打印一段内存的字节(从低地址到高地址),这就是"直接 send 会发出去的字节顺序"
void dump(const char* name, const void* p, size_t n)
{
	const unsigned char* b = (const unsigned char*)p;
	printf("%-16s ", name);
	for (size_t i = 0; i < n; i++) printf("%02X ", b[i]);
	printf("\n");
}

// 打印一段内存的二进制形式(每个字节 8 bit,左边 MSB 最高位 → 右边 LSB 最低位)
// 用途:看清"位域把每个字段放在哪个 bit"(十六进制只能看字节,二进制能看 bit)
void dump_bin(const char* name, const void* p, size_t n)
{
	const unsigned char* b = (const unsigned char*)p;
	printf("%-16s ", name);
	for (size_t i = 0; i < n; i++) {
		for (int bit = 7; bit >= 0; bit--)
			printf("%d", (b[i] >> bit) & 1);
		printf(" ");
	}
	printf("\n");
}

int main()
{
	printf("===== 1. 本机字节序:小端还是大端? =====\n");
	unsigned int x = 0x12345678;
	dump("int 0x12345678", &x, 4);
	printf("→ 内存低地址开始看到 78 56 34 12:低字节(0x78)在前 = 小端(x86)\n\n");

	printf("===== 2. htonl 转换后 =====\n");
	unsigned int y = htonl(0x12345678);
	dump("htonl 之后", &y, 4);
	printf("→ 12 34 56 78:高字节(0x12)在前 = 网络大端序\n");
	printf("→ 小端机器上 htonl = 把 4 个字节倒过来;大端机器上是 no-op\n\n");

	printf("===== 3. htons(16 位) =====\n");
	unsigned short s = htons(0x1234);
	dump("htons(0x1234)", &s, 2);
	printf("→ 原内存 34 12,转换后 12 34(高低两字节交换)\n\n");

	printf("===== 4. RTPHeader 位域布局(version=2, pytype=96) =====\n");
	RTPHeader h;
	memset(&h, 0, sizeof(h));
	h.version = 2;
	h.pytype = 96;
	h.serial = 0x1234;
	h.timestamp = 0x11223344;
	h.ssrc = 0x55667788;
	dump("原始内存(hex)", &h, 12);
	dump_bin("原始内存(bin)", &h, 12);
	printf("→ 字节0 = 10000000:bit6-7=10(version=2),bit0-3=0000(csrccount=0)\n");
	printf("→ 字节1 = 01100000:bit7=0(mark),bit0-6=1100000(pytype=96)\n");
	printf("→ 位域拼出的前 2 字节 = RTP 规范字节(V=2 在最高2位, PT=96),不用转换!\n");
	printf("→ 但 serial/timestamp/ssrc 还是小端(34 12 / 44 33 22 11 / 88 77 66 55)\n\n");

	printf("===== 5. 模拟参考版 operator EBuffer:多字节字段转网络序后整体拷贝 =====\n");
	RTPHeader net = h;
	net.serial = htons(net.serial);
	net.timestamp = htonl(net.timestamp);
	net.ssrc = htonl(net.ssrc);
	dump("转网络序后(hex)", &net, 12);
	dump_bin("转网络序后(bin)", &net, 12);
	printf("→ 80 60 12 34 11 22 33 44 55 66 77 88 = 标准 RTP 头!sendto 直接发即可\n");
	printf("→ 前 2 字节(位域)没动,后 6 字节(多字节数值)高低字节交换了\n\n");

	printf("===== 6. 结论 =====\n");
	printf("位域(bit 级)≠ 字节序(byte 级):RTP 头前 2 字节是 bit 字段,位域布局碰巧对齐规范;\n");
	printf("serial/timestamp/ssrc 是 16/32 位整数,小端内存直接 send 会把低字节先发出去,\n");
	printf("而网络要求高字节先发 → 必须 htons/htonl。\n");
	return 0;
}
