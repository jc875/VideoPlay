#ifndef VIDEOCLIENTCONTROLLER_H
#define VIDEOCLIENTCONTROLLER_H
#include "evlc.h"

// 本类不 include mainwindow.h,也不持有任何 UI 指针:
// 单向架构 MainWindow → VideoClientController → Evlc,控制层不知道 UI 的存在。
enum EVlcCommand{
    EVLC_PLAY,
    EVLC_PAUSE,
    EVLC_STOP,
    EVLC_GET_VOLUME,
    EVLC_GET_POSITION,
    EVLC_GET_LENGTH,
};

class VideoClientController
{
public:
    VideoClientController() = default;      // 默认构造/析构:没有要初始化的 UI 成员了
    ~VideoClientController() = default;
    int SetMedia(const std::string& strUrl); // 设媒体
    float VideoCtrl(EVlcCommand cmd);        // 统一命令口:播放/暂停/停止/查音量/查进度/查时长
    void SetPosition(float pos);             // 跳播
    int SetWnd(HWND hWnd);                   // 存播放窗口句柄
    int SetVolume(int volume);               // 调音量
    VlcSize GetMediaInfo();                  // 视频宽高

protected:
    Evlc m_evlc;   // 唯一依赖:引擎封装。不知道 UI 的存在(单向依赖)
};

#endif // VIDEOCLIENTCONTROLLER_H
