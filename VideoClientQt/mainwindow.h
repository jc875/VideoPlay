#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_playButton_clicked();
    void on_tick();
    void on_stopButton_clicked();

    void on_posSlider_sliderMoved(int position);

    void on_volumeSlider_valueChanged(int value);

private:
    Ui::MainWindow *ui;
    bool playing = false;
};
#endif // MAINWINDOW_H
