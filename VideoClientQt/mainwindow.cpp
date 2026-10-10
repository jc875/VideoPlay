#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include"QTimer"
#include "videoclientcontroller.h"
#include"QDebug"
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
     playing = false;
    ui->setupUi(this);
     QTimer *timer=new QTimer(this);
    connect(timer,&QTimer::timeout,this,&MainWindow::on_tick);
     timer->start(200);
    ui->posSlider->setRange(0, 1000);
    m_controller = new VideoClientController;   // 单向依赖:UI 持有 controller
    // 不再 SetView:controller 是纯逻辑层,不需要窗口指针(参考工程 Init 是 MFC 历史包袱)
}

MainWindow::~MainWindow()
{
    delete ui;
    delete m_controller;
}

void MainWindow::on_playButton_clicked()
{
    // 点一下在"播放↔暂停"之间切换按钮文字,先验证信号槽链路通不通。

    if (!playing) {
        QString url=ui->lineEdit->text().trimmed();

        if(url.isEmpty())return;
        m_controller->SetWnd((HWND)ui->videoWidget->winId());
        int ret=m_controller->SetMedia(url.toStdString());
        if(ret!=0){
            qDebug()<<"SetMedia 失败,错误码:"<<ret;
            return;
        }

        m_controller->VideoCtrl(EVLC_PLAY);
        ui->playButton->setText("暂停"); // ui 是指针,用箭头 ->(不是 ui.点号)
        playing = true;
    } else {
        m_controller->VideoCtrl(EVLC_PAUSE);
        ui->playButton->setText("播放");
        playing = false;                 // 注意:MFC 参考代码这里写成 true,是 bug
    }
}

void MainWindow::on_tick()
{
    float pos=m_controller->VideoCtrl(EVLC_GET_POSITION);
    if(pos!=-1.0f){
       ui->posSlider->setValue((int)(pos * 1000));
        float len = m_controller->VideoCtrl(EVLC_GET_LENGTH); // 总时长(秒)
        if (len > 0) {
            int cur = (int)(pos * len);                       // 当前秒数
            QString t = QString("%1:%2 / %3:%4")
                            .arg(cur / 60)                                // 当前:分钟
                            .arg(cur % 60, 2, 10, QChar('0'))             // 当前:秒,不足两位补 0
                            .arg((int)len / 60)                           // 总:分钟
                            .arg((int)len % 60, 2, 10, QChar('0'));       // 总:秒
            ui->timeLabel->setText(t);                        // "00:06 / 00:15"
        }
    }
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

