// VideoClientDlg.h —— 视频客户端主窗口类(QMainWindow)。
// 由 Qt VS Tools 的 QtMoc 处理(Q_OBJECT 宏需要 moc)。
#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_VideoClientDlg.h"   // uic 根据 VideoClientDlg.ui 生成的头文件

class VideoClientDlg : public QMainWindow
{
    Q_OBJECT   // Qt 元对象宏:启用信号槽/moc

public:
    explicit VideoClientDlg(QWidget *parent = nullptr);
    ~VideoClientDlg();

private:
    Ui::VideoClientDlgClass ui;   // .ui 画布生成的界面对象,setupUi(this) 后即可用 ui.xxx 访问控件
};
