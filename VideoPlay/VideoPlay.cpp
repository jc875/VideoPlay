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

int main()
{
    // ---- ① 创建:instance → media → player ----
    int argc = 1;
    char* argv[2];
    argv[0] = (char*)"--ignore-config";           // 忽略 VLC 配置,干净启动引擎
    libvlc_instance_t* vlc_ins = libvlc_new(argc, argv);   // 开引擎
    if (vlc_ins == NULL) {                         // new 失败要判空
        printf("libvlc_new failed!\r\n");
        return -1;
    }

    // ---- ② 用"当前工作目录 + 文件名"拼出视频路径(不再写死 E:\...) ----
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
    std::string mrl = "file:///" + utf8Path;       // 拼成 libvlc 认识的 MRL 地址

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
        // 解析完之前 get_volume 返回 -1(代表"参数还不可用")
        int vol = -1;
        while (vol == -1) {
            Sleep(10);
            vol = libvlc_audio_get_volume(player);
        }
        printf("volume is %d\r\n", vol);
        libvlc_audio_set_volume(player, 10);      // 音量调到 10(0~100)

        // ---- ⑤ 查询:总时长 / 分辨率 ----
        libvlc_time_t tm = libvlc_media_player_get_length(player);  // 毫秒
        printf("%02d:%02d:%02d.%03d\r\n",
            int(tm / 3600000), int(tm / 60000) % 60, int(tm / 1000) % 60, int(tm) % 1000);
        int width = libvlc_video_get_width(player);
        int height = libvlc_video_get_height(player);
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
