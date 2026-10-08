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
