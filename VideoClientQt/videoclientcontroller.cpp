#include "videoclientcontroller.h"

// 注意:这里不再 include mainwindow.h——控制层纯转发,不碰 UI。
// (原构造函数里 m_window->m_controller=this 是野指针解引用 bug,已随 SetView 一起移除)

int VideoClientController::SetMedia(const std::string &strUrl)
{
    return this->m_evlc.SetMedia(strUrl);
}

float VideoClientController::VideoCtrl(EVlcCommand cmd)
{
    switch (cmd) {
    case EVLC_STOP:
        return m_evlc.Stop();
        break;
    case EVLC_GET_VOLUME:
        return m_evlc.GetVolume();
    case EVLC_GET_POSITION:
        return m_evlc.GetPosition();
    case EVLC_PAUSE:
        return m_evlc.Pause();
    case EVLC_PLAY:
        return m_evlc.Play();
    case EVLC_GET_LENGTH:
        return m_evlc.GetLength();
    default:
        break;
    }
    return -1.0f;
}

void VideoClientController::SetPosition(float pos)
{
    m_evlc.SetPosition(pos);
}

int VideoClientController::SetWnd(HWND hWnd)
{
    return m_evlc.SetHwnd(hWnd);
}

int VideoClientController::SetVolume(int volume)
{
    return m_evlc.SetVolume(volume);

}

VlcSize VideoClientController::GetMediaInfo()
{
    return m_evlc.GetMediaInfo();
}
