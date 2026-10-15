#include "MideaFile.h"
#include <cstdio>

// ========== MideaFile:读 H264 裸流逐帧输出 ==========
// 参考版(桌面 音视频系统代码\VideoRTSPServer\MideaFile.cpp)同款逻辑,EBuffer → Buffer。
// H264 裸流 = 一连串 NALU 直接连在一起,每个 NALU 以"起始码"开头:
//   三字节起始码:00 00 01
//   四字节起始码:00 00 00 01(防竞争,一般出现在流开头/关键帧前)
// 本类做的就是:定位起始码 → 读 [本起始码, 下一个起始码) 之间的字节 = 一个完整 NALU。

MideaFile::MideaFile()
	: m_size(0), m_file(NULL), m_type(-1)   // m_type=-1:未打开;Open 成功后才置 96
{
}

MideaFile::~MideaFile()
{
	Close();   // 析构兜底,防文件句柄泄漏
}

int MideaFile::Open(const Buffer& filepath, int nType)
{
	// const Buffer& → fopen 第一参 const char*:走 operator const char*() const,取 c_str()
	m_file = fopen(filepath, "rb");
	if (m_file == NULL) {
		// TODO(参考版同款):这里可以加 TRACE 报错提示,参考版没做
		return -1;
	}
	m_type = nType;
	// 先跳到文件尾拿总大小:以后读最后一帧时,下一个起始码不存在 → 用 m_size 当帧尾
	fseek(m_file, 0, SEEK_END);
	m_size = ftell(m_file);
	Reset();   // 回到文件头,准备从第一帧读起
	return 0;
}

Buffer MideaFile::ReadOneFrame()
{
	switch (m_type) {
	case 96:
		return ReadH264Frame();   // 目前只支持 H264(96);参考版也只实现了这一个分支
	}
	return Buffer();   // 未知类型:返回空帧(调用方当"没有帧"处理)
}

void MideaFile::Close()
{
	m_type = -1;   // 置回未打开态
	if (m_file != NULL) {
		// 先拷出再置空再关:保证"只关一次"——fclose 后如果还留着旧指针,重复 Close 会 double-free
		FILE* file = m_file;
		m_file = NULL;
		fclose(file);
	}
}

void MideaFile::Reset()
{
	if (m_file) {
		fseek(m_file, 0, SEEK_SET);   // 倒带回文件头
	}
}

// 从当前文件位置找 NALU 起始码。核心思路:
//   起始码 = 00 00 01(或 00 00 00 01),所以先跳过所有非零字节,找到一个 00,
//   再看它后面是不是 00 01 / 00 00 01。
long MideaFile::FindH264Head(int& headsize)
{
	while (!feof(m_file)) {
		char c = 0x7F;   // 参考版写法:初始值会被下面 fgetc 覆盖,实际没用;保留以对齐参考版
		// ① 跳过非零字节,直到读到一个 0
		while (!feof(m_file)) {          // feof = file end of file,到文件尾退出
			c = fgetc(m_file);
			if (c == 0) break;           // 找到第一个 00,进入判断
		}
		if (!feof(m_file)) {             // 没到文件尾,说明刚读到的是 00 不是 EOF
			c = fgetc(m_file);
			if (c == 0) {                // 00 00 ...
				c = fgetc(m_file);
				if (c == 1) {            // 00 00 01 → 三字节起始码
					headsize = 3;
					return ftell(m_file) - 3;   // ftell 已指到 01 之后,减 3 回到起始码开头
				}
				else if (c == 0) {       // 00 00 00 ...
					c = fgetc(m_file);
					if (c == 1) {        // 00 00 00 01 → 四字节起始码
						headsize = 4;
						return ftell(m_file) - 4;
					}
				}
			}
			// 到这里说明 00 后面不是 01/00 01,不是有效起始码,外层继续找下一个 00
		}
	}
	return -1;   // 整个文件都没找到起始码
}

Buffer MideaFile::ReadH264Frame()
{
	if (m_file) {
		int headsize = 0;
		// ① 当前帧头
		long off = FindH264Head(headsize);
		if (off == -1) return Buffer();            // 没有起始码 = 没帧了
		// ② 从帧头之后找下一个头(下一帧的开始)
		fseek(m_file, off + headsize, SEEK_SET);
		long tail = FindH264Head(headsize);
		if (tail == -1) tail = m_size;             // 没有下一帧 = 当前是最后一帧,文件尾当边界
		// ③ [off, tail) 就是一个完整 NALU(含起始码)
		long size = tail - off;
		fseek(m_file, off, SEEK_SET);
		Buffer result(size);                       // 直接 resize(size):fread 写 c_str() 时 size 已对,不会有 sscanf 那种"size 不更新"的坑
		fread(result, 1, size, m_file);            // result → operator void*(),拿到可写缓冲
		return result;
	}
	return Buffer();   // 文件未打开
}
