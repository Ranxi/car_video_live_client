#ifndef LIVECLIENTWINDOW_H
#define LIVECLIENTWINDOW_H

#include <QMainWindow>
#include <QDebug>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QImage>
#include <QPixmap>
#include <QCloseEvent>
#include <QMessageBox>
#include <QTimer>
#include <QBuffer>
#include <QUdpSocket>
#include <QImageReader>
#include "opencv2/opencv.hpp"
#include "opencv2/imgproc/imgproc_c.h"
#include "decoder.h"


namespace Ui {
class LiveClientWindow;
enum VIDEOTYPE {CAMERA, NETWORK, LOCALFILE, NONE};
}

//enum VIDEOTYPE{
//    CAMERA,
//    NETWORK,
//    LOCALFILE,
//    NONE,
//};


class LiveClientWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit LiveClientWindow(QWidget *parent = 0);
    ~LiveClientWindow();

private:
    void closeEvent(QCloseEvent *event);
    Ui::LiveClientWindow *ui;
    QGraphicsPixmapItem pixmap;
    //cv::Mat frame;
    QImage qimg;
    QTimer timer;
    Decoder *decoder;
    Ui::VIDEOTYPE vtype;
private slots:
    void on_startBtn_pressed();
    void updateFrame();
//    void processPendingDatagram();
public:
    void set_decoder(Decoder *dcder);
};

#endif // LIVECLIENTWINDOW_H
