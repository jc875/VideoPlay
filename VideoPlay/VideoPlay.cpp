#include <Windows.h>
#include <iostream>
#include "vlc.h"
#include <conio.h>          // _kbhit(): 检测键盘有没有按键,Windows 专用

// 中文路径转 UTF-8:libVLC 的 API 只认 UTF-8 字符串,
// Windows 的宽字符文件名是 UTF-16,不转码就找不到 "股市讨论.mp4"
std::string Unicode2Utf8(const std::wstring& strIn)
{
    std::string str;
    int length = ::WideCharToMultiByte(CP_UTF8, 0, strIn.c_str(), strIn.size(), NULL, 0, NULL, NULL);
    str.resize(length + 1);
    ::WideCharToMultiByte(CP_UTF8, 0, strIn.c_str(), strIn.size(), (LPSTR)str.c_str(), length, NULL, NULL);
    return str;
}

int main()
{
    // ---- ① 创建:instance → media → player ----
    int argc = 1;
    char* argv[2];
    argv[0] = (char*)"--ignore-config";           // 忽略 VLC 配置,干净启动引擎
    libvlc_instance_t* vlc_ins = libvlc_new(argc, argv);   // 开引擎

    // ⚠️ 坑:这里写死了 E:\edoyun\... 绝对路径,你机器上必挂,要改成相对路径
    std::string path = Unicode2Utf8(L"file:///E:\\edoyun\\study_project\\VideoPlay\\VideoPlay\\股市讨论.mp4");
    libvlc_media_t* media = libvlc_media_new_location(vlc_ins, path.c_str());  // 加载媒体

    libvlc_media_player_t* player = libvlc_media_player_new_from_media(media); // 建播放器并绑定媒体

    do {   // do-while(0):只执行一次的写法,只是为了能用 break 跳出
        // ---- ② 播放(异步!)----
        int ret = libvlc_media_player_play(player);
        if (ret == -1) { printf("error found!\r\n"); break; }   // play 失败必须检查

        // ---- ③ 轮询等媒体解析完成 ----
        // 播放是异步的:play 只负责开播,媒体还在后台解析。
        // 解析完之前 get_volume 返回 -1(代表"参数还不可用")
        int vol = -1;
        while (vol == -1) {
            Sleep(10);
            vol = libvlc_audio_get_volume(player);
        }
        printf("volume is %d\r\n", vol);
        libvlc_audio_set_volume(player, 10);      // 音量调到 10(0~100)

        // ---- ④ 查询:总时长 / 分辨率 ----
        libvlc_time_t tm = libvlc_media_player_get_length(player);  // 毫秒
        printf("%02d:%02d:%02d.%03d\r\n",
            int(tm / 3600000), int(tm / 60000) % 60, int(tm / 1000) % 60, int(tm) % 1000);
        int width = libvlc_video_get_width(player);
        int height = libvlc_video_get_height(player);
        printf("width=%d height=%d\r\n", width, height);

        // ---- ⑤ 没按键就一直打印播放进度(相对进度 0.0~1.0)----
        while (!_kbhit()) {
            printf("%f%%\r", 100.0 * libvlc_media_player_get_position(player));
            Sleep(500);                            // 每 500ms 刷一次
        }
        // ---- ⑥ 按键后:暂停 → 续播 → 停止(每次回车)----
        getchar();
        libvlc_media_player_pause(player);
        getchar();
        libvlc_media_player_play(player);          // 同一个函数:第一次=开播,暂停后再调=续播
        getchar();
        libvlc_media_player_stop(player);
    } while (0);

    // ---- ⑦ 释放:顺序 = 创建顺序反过来 ----
    libvlc_media_player_release(player);
    libvlc_media_release(media);
    libvlc_release(vlc_ins);
    return 0;
}
