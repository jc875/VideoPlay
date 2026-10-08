#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_playButton_clicked()
{
    // 点一下在"播放↔暂停"之间切换按钮文字,先验证信号槽链路通不通。
    // 以后这里再填真正的 libvlc_media_player_play / pause。
    static bool playing = false;        // 局部静态:函数退出后值不销毁,下次进来还是上次状态
    if (!playing) {
        ui->playButton->setText("暂停"); // ui 是指针,用箭头 ->(不是 ui.点号)
        playing = true;
    } else {
        ui->playButton->setText("播放");
        playing = false;                 // 注意:MFC 参考代码这里写成 true,是 bug
    }
}

