#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include"QTimer"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
     playing = false;
    ui->setupUi(this);
     QTimer *timer=new QTimer(this);
    connect(timer,&QTimer::timeout,this,&MainWindow::on_tick);
     timer->start(200);
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

void MainWindow::on_tick()
{

}


void MainWindow::on_stopButton_clicked()
{
    playing=false;
    ui->playButton->setText("播放");
}


void MainWindow::on_posSlider_sliderMoved(int position)
{

}


void MainWindow::on_volumeSlider_valueChanged(int value)
{

}

