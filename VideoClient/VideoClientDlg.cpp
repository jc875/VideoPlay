// VideoClientDlg.cpp —— 主窗口实现。
#include "VideoClientDlg.h"

VideoClientDlg::VideoClientDlg(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);   // 把 .ui 里拖的控件构建到本窗口;之后用 ui.控件名 访问
}

VideoClientDlg::~VideoClientDlg()
{
}

// 播放按钮槽:点一下在"播放↔暂停"之间切换按钮文字。
// 现在只切文字,用来验证信号槽链路通不通;以后这里再填真正的 libvlc_media_player_play/pause。
void VideoClientDlg::on_playButton_clicked()
{
    static bool playing = false;   // 局部静态:函数退出后值不销毁,下次进来还是上次的状态
    if (!playing) {
        ui.playButton->setText("暂停");  // 从"播放"点进来=开始播,按钮改成"暂停"
        playing = true;
    } else {
        ui.playButton->setText("播放");  // 从"暂停"点进来=暂停,按钮改回"播放"
        playing = false;                // 注意:MFC 参考代码这里写成了 true,是 bug,我们写成 false
    }
}
