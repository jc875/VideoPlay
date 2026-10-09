#ifndef EVLC_H
#define EVLC_H

#include <string>
#include <qwindowdefs_win.h>   // 提供 HWND 类型(Qt 的轻量 Windows 头,只拿句柄定义)
#include "vlc/vlc.h"           // libvlc 全部 API

// 视频宽高容器:就像"快递盒",一次装两个 int。
// 对齐参考工程的 VlcSize(默认构造/拷贝构造/operator= 三个都给,方便值传递)。
class VlcSize {
public:
    int nWidth;
    int nHeight;
    VlcSize(int width = 0, int height = 0) {
        nWidth = width;
        nHeight = height;
    }
    VlcSize(const VlcSize& size) {
        nWidth = size.nWidth;
        nHeight = size.nHeight;
    }
    VlcSize& operator=(const VlcSize& size) {
        if (this != &size) {
            nWidth = size.nWidth;
            nHeight = size.nHeight;
        }
        return *this;
    }
};

// libvlc 播放器封装 = "遥控器"。
// MainWindow(界面)只跟它打交道,不直接碰 libvlc 的 C API。
// 生命周期铁律:构造建引擎,析构按 player → media → vlc 顺序 release。
class Evlc {
public:
    Evlc();
    ~Evlc();

    // ① 先调:存播放窗口句柄(只存,真正设给 player 在 SetMedia 内部)
    int SetHwnd(HWND hwnd);

    // ② 再调:设媒体地址。内部会重建 player 并自动把存的句柄设上去。
    //    strUrl 必须 UTF-8 编码(Qt 里 QString::toStdString() 直接就是 UTF-8,中文路径没问题)
    int SetMedia(std::string strUrl);

    int Play();
    int Pause();
    int Stop();

    int SetVolume(int volume);   // 0~100,返回 0 成功 / -1 失败
    int GetVolume();             // 当前音量 0~100

    void SetPosition(float position);  // 进度跳播,0.0~1.0(0% ~ 100%)
    float GetPosition();               // 当前进度 0.0~1.0,定时器轮询用

    float GetLength();                 // 总时长(秒),进度条最大值用
    VlcSize GetMediaInfo();            // 视频宽高,显示分辨率用

private:
    libvlc_instance_t* m_vlc;        // 引擎(整个 VLC,1 个,最后释放)
    libvlc_media_t* m_media;         // 媒体(一个文件/流,其次释放)
    libvlc_media_player_t* m_player; // 播放器(负责播出来,最先释放)
    HWND m_hwnd;                     // 播放窗口句柄(存值,不是指针)
    std::string m_url;               // 当前媒体地址,用于去重
};

#endif // EVLC_H
