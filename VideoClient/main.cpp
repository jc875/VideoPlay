// VideoClient 入口:创建 Qt 应用并显示主窗口(VideoClientDlg)。
#include <QtWidgets/QApplication>
#include "VideoClientDlg.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    VideoClientDlg window;   // 主窗口(空画布,UI 在 VideoClientDlg.ui 里设计)
    window.show();

    return app.exec();       // 进入 Qt 事件循环,窗口事件(按钮/定时器/重绘)都由它驱动
}
