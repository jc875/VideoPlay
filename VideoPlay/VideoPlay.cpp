#include <Windows.h>
#include <iostream>
#include "vlc.h"
#include <conio.h>          // _kbhit(): 检测键盘有没有按键,Windows 专用

// ============================================================
// Unicode2Utf8:宽字符字符串(std::wstring,UTF-16)转 UTF-8
//
// 为什么需要它:libVLC 的 API 只认 UTF-8 字符串,而 Windows 的
// 文件名是宽字符(UTF-16)。中文路径(如"股市讨论.mp4")不转码,
// 传进去就是乱码,libvlc 找不到文件。
//
// 写法说明(cchWideChar 传 -1):
//   WideCharToMultiByte 的 cchWideChar 参数传 -1,表示"从输入
//   自动数到结尾 '\0',并把 '\0' 也算进输出长度"。
//   第一次调用只求长度(length 含结尾 '\0'),第二次真正转换。
//   str 分配 length-1 个字符(去掉 '\0'),转换函数把 '\0' 写到
//   &str[0] 的末尾,得到标准 C 字符串。
//   (比"先 resize(length+1) 再强转 c_str() 写"的写法更规范,
//   不修改 const 指针,MSVC 上两种都能跑,但这是标准做法)
// ============================================================
std::string Unicode2Utf8(const std::wstring& strIn)
{
    if (strIn.empty()) return "";

    // 第一次调用:只求 UTF-8 字节长度(含结尾 '\0')
    int length = ::WideCharToMultiByte(CP_UTF8, 0, strIn.c_str(), -1, NULL, 0, NULL, NULL);

    // 分配 length-1 个字符(真正的字符部分)
    std::string str(length - 1, '\0');

    // 第二次调用:真正转换,写入 &str[0]
    ::WideCharToMultiByte(CP_UTF8, 0, strIn.c_str(), -1, &str[0], length, NULL, NULL);
    return str;
}

int main(int argc, char* argv[])
{
    setvbuf(stdout, NULL, _IONBF, 0);   // 无缓冲:printf 立即输出(重定向到文件时不会被 4KB 块缓冲吞掉)
    // ---- ① 创建:instance → media → player ----
    int argc_vlc = 1;
    char* argv_vlc[2];
    argv_vlc[0] = (char*)"--ignore-config";           // 忽略 VLC 配置,干净启动引擎
    libvlc_instance_t* vlc_ins = libvlc_new(argc_vlc, argv_vlc);   // 开引擎
    if (vlc_ins == NULL) {                         // new 失败要判空
        printf("libvlc_new failed!\r\n");
        return -1;
    }

    // ---- ② 媒体地址:优先命令行参数,支持 rtsp://(端到端连自己的 RTSP 服务器)----
    // 用法:VideoPlay.exe                       → 播工作目录下的股市讨论.mp4(原行为)
    //       VideoPlay.exe rtsp://127.0.0.1:554/live → 播 RTSP 流(信令+RTP 收流+解码 libvlc 全包)
    //       VideoPlay.exe D:\x\y.mp4           → 播指定本地文件
    std::string mrl;
    if (argc > 1) {
        mrl = argv[1];
        // 带协议的(rtsp:// http:// file://)直接用;否则当本地文件路径,拼 file:/// 前缀
        if (mrl.find("://") == std::string::npos) {
            for (auto& c : mrl) { if (c == '\\') c = '/'; }   // 反斜杠换正斜杠
            mrl = "file:///" + mrl;
        }
    }
    else {
        // 没传参数:保持原逻辑——"当前工作目录 + 文件名"拼出视频路径(不再写死 E:\...)
        // GetCurrentDirectoryW 拿的是"工作目录"(不是 exe 所在目录!):
        //   · VS 里 F5 调试 → 工作目录 = 工程目录(vcxproj 所在),股市讨论.mp4 也在这 → 能找到
        //   · 双击 exe 运行 → 工作目录 = exe 所在目录 → 视频得放到 exe 旁边才找得到
        //   这个坑记住:控制台程序"当前目录"取决于启动方式,不是固定的。
        wchar_t curDir[MAX_PATH] = { 0 };
        GetCurrentDirectoryW(MAX_PATH, curDir);
        std::wstring videoPath = std::wstring(curDir) + L"\\股市讨论.mp4";

        // 转 UTF-8,并把反斜杠换成正斜杠:
        //   file:/// 的标准写法是正斜杠(file:///D:/code/...),
        //   参考工程用反斜杠(file:///E:\...) 也能跑,但正斜杠更稳。
        std::string utf8Path = Unicode2Utf8(videoPath);
        for (auto& c : utf8Path) {
            if (c == '\\') c = '/';
        }
        mrl = "file:///" + utf8Path;       // 拼成 libvlc 认识的 MRL 地址
    }
    printf("MRL: %s\r\n", mrl.c_str());

    // 加载媒体(location 一个函数通吃 file/http/rtsp/screen,这就是"一切都是流")
    libvlc_media_t* media = libvlc_media_new_location(vlc_ins, mrl.c_str());
    if (media == NULL) {
        printf("libvlc_media_new_location failed!\r\n");
        libvlc_release(vlc_ins);
        return -1;
    }

    // 建播放器并绑定媒体
    libvlc_media_player_t* player = libvlc_media_player_new_from_media(media);
    if (player == NULL) {
        printf("libvlc_media_player_new_from_media failed!\r\n");
        libvlc_media_release(media);
        libvlc_release(vlc_ins);
        return -1;
    }

    do {   // do-while(0):只执行一次的写法,只是为了能用 break 跳出
        // ---- ③ 播放(异步!)----
        int ret = libvlc_media_player_play(player);
        if (ret == -1) { printf("error found!\r\n"); break; }   // play 失败必须检查

        // ---- ④ 轮询等媒体解析完成 ----
        // 播放是异步的:play 只负责开播,媒体还在后台解析。
        // ⚠️ 不能拿 libvlc_audio_get_volume 当"解析完成"信号:
        //    RTSP 服务器推的是纯视频流(无音频轨),get_volume 永远返回 -1 → 死循环!
        //    (股市讨论.mp4 有音频轨所以原逻辑能过,纯视频流直接卡死,本次实测暴露)
        //    改成等"视频解码器就绪"(width>0,收到 SPS 后 libvlc 才解出分辨率)或时长可用。
        int width = 0, height = 0;
        libvlc_time_t tm = -1;
        int wait_cnt = 0;
        while (tm == -1 && width == 0 && wait_cnt < 1000) {   // 最多等 10 秒(rtsp 握手+首帧)
            Sleep(10);
            tm = libvlc_media_player_get_length(player);      // 毫秒;RTSP 流播放中会更新
            width = libvlc_video_get_width(player);
            wait_cnt++;
        }
        int vol = libvlc_audio_get_volume(player);
        if (vol != -1) {
            printf("volume is %d\r\n", vol);
            libvlc_audio_set_volume(player, 10);      // 音量调到 10(0~100)
        }
        else {
            printf("no audio track (volume n/a)\r\n");   // 纯视频流:没有音频,跳过音量
        }

        // ---- ⑤ 查询:总时长 / 分辨率 ----
        printf("%02d:%02d:%02d.%03d\r\n",
            int(tm / 3600000), int(tm / 60000) % 60, int(tm / 1000) % 60, int(tm) % 1000);
        width = libvlc_video_get_width(player);
        height = libvlc_video_get_height(player);
        printf("width=%d height=%d\r\n", width, height);

        // ---- ⑥ 没按键就一直打印播放进度(相对进度 0.0~1.0)----
        while (!_kbhit()) {
            printf("%f%%\r", 100.0 * libvlc_media_player_get_position(player));
            Sleep(500);                            // 每 500ms 刷一次
        }
        // ---- ⑦ 按键后:暂停 → 续播 → 停止(每次回车)----
        getchar();
        libvlc_media_player_pause(player);
        getchar();
        libvlc_media_player_play(player);          // 同一个函数:第一次=开播,暂停后再调=续播
        getchar();
        libvlc_media_player_stop(player);
    } while (0);

    // ---- ⑧ 释放:顺序 = 创建顺序反过来 ----
    libvlc_media_player_release(player);
    libvlc_media_release(media);
    libvlc_release(vlc_ins);
    return 0;
}
