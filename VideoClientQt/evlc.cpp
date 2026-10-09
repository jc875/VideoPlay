#include "evlc.h"

Evlc::Evlc()
{
    // 引擎建一次,整个程序生命周期都复用
    m_vlc = libvlc_new(0, NULL);
    m_media = NULL;
    m_player = NULL;
    m_hwnd = NULL;
}

Evlc::~Evlc()
{
    // 销毁顺序:player → media → vlc(和创建相反,后建先拆,防止依赖对象先没了)
    if (m_player != NULL) {
        libvlc_media_player_t* temp = m_player;
        m_player = NULL;                      // 先置空,防止误用野指针
        libvlc_media_player_release(temp);
    }
    if (m_media != NULL) {
        libvlc_media_t* temp = m_media;
        m_media = NULL;
        libvlc_media_release(temp);
    }
    if (m_vlc != NULL) {
        libvlc_instance_t* temp = m_vlc;
        m_vlc = NULL;
        libvlc_release(temp);
    }
}

int Evlc::SetHwnd(HWND hwnd)
{
    // 只存值。真正调用 libvlc_media_player_set_hwnd 是在 SetMedia 内部,
    // 因为每次 SetMedia 都会 new 一个新 player,只有那时才知道该把句柄给谁。
    m_hwnd = hwnd;
    return 0;
}

int Evlc::SetMedia(std::string strUrl)
{
    // 外部有中文的路径:Qt 里 QString::toStdString() 直接就是 UTF-8,不用手动转
    if (m_vlc == nullptr || m_hwnd == nullptr) {
        return -1;
    }
    if (m_url == strUrl) return 0;
    m_url = strUrl;
    if (m_media != nullptr) {
        libvlc_media_release(m_media);
        m_media = nullptr;
    }
    m_media = libvlc_media_new_location(m_vlc, strUrl.c_str());
    if (!m_media) {
        return -2;
    }
    if (m_player != nullptr) {
        libvlc_media_player_release(m_player);
        m_player = nullptr;
    }
    m_player = libvlc_media_player_new_from_media(m_media);
    if (m_player == nullptr) {
        return -3;
    }
    // 关键!新 player 是"白纸",必须把窗口句柄设上去,否则画面不知道画到哪
    libvlc_media_player_set_hwnd(m_player, m_hwnd);
    return 0;
}

int Evlc::Play()
{
    if (!m_player || !m_vlc || !m_media) return -1;
    return libvlc_media_player_play(m_player);
}

int Evlc::Pause()
{
    if (!m_player || !m_vlc || !m_media) return -1;
    libvlc_media_player_pause(m_player);
    return 0;
}

int Evlc::Stop()
{
    if (!m_player || !m_vlc || !m_media) return -1;
    libvlc_media_player_stop(m_player);
    return 0;
}

int Evlc::SetVolume(int volume)
{
    if (!m_player || !m_vlc || !m_media) return -1;
    return libvlc_audio_set_volume(m_player, volume);
}

int Evlc::GetVolume()
{
    if (!m_player || !m_vlc || !m_media) return -1;
    return libvlc_audio_get_volume(m_player);
}

void Evlc::SetPosition(float position)
{
    if (!m_player || !m_vlc || !m_media) return;
    libvlc_media_player_set_position(m_player, position);
}

float Evlc::GetPosition()
{
    if (!m_player || !m_vlc || !m_media) return -1.0f;
    // 返回 0.0~1.0 的小数:0 = 开头,1 = 结尾
    return libvlc_media_player_get_position(m_player);
}

float Evlc::GetLength()
{
    if (!m_player || !m_vlc || !m_media) return -1.0f;
    // libvlc 返回毫秒(libvlc_time_t),转成秒给 UI 用
    libvlc_time_t tm = libvlc_media_player_get_length(m_player);
    return tm / 1000.0f;
}

VlcSize Evlc::GetMediaInfo()
{
    if (!m_player || !m_vlc || !m_media) return VlcSize(-1, -1);
    return VlcSize(
        libvlc_video_get_width(m_player),
        libvlc_video_get_height(m_player)
    );
}
