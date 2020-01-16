#include "liveclientwindow.h"
#include "ui_liveclientwindow.h"
#include <QMutex>
#include <QTime>

extern QMutex mutex_imgque;
extern QList<JYFrame* > listImage;
extern bool decoded_listOver;

extern AVPacketList *pktl;
extern AVPacketList *pktl_end;
extern QMutex mutex_pktl;
//extern QList<IplImage*> listImage;

LiveClientWindow::LiveClientWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::LiveClientWindow)
{
    ui->setupUi(this);
    ui->graphicsView->setScene(new QGraphicsScene());
    ui->graphicsView->scene()->addItem(&pixmap);
    timer.start(1000);
    ui->videoEdit->setText("udp://172.16.20.116:56789");
    decoder = NULL;
    m_stat = Ui::IDLE;
}

LiveClientWindow::~LiveClientWindow()
{
    delete ui;
}

void LiveClientWindow::set_decoder(Decoder *dcder){
    if (dcder!=NULL)
        decoder = dcder;
}


void LiveClientWindow::on_startBtn_pressed(){
    if(decoder!=NULL && decoder->isRunning()){
        assert(Ui::PLAYING == m_stat);
        disconnect(rcver, &QUdpSocket::readyRead, this, 0);
        decoder->requestInterruption();
        decoder->quit();
        decoder->wait();
        mutex_imgque.lock();
        for (auto x : listImage){
            x->frame.release();
            free(x);
        }
        listImage.clear();
        mutex_imgque.unlock();
        disconnect(&timer, &QTimer::timeout, this, 0);
        ui->startBtn->setText("Start");
        m_stat = Ui::IDLE;
        return;
    }
    else if (decoder!=NULL && decoder->isFinished() && (Ui::IDLE!=m_stat)){
        disconnect(rcver, &QUdpSocket::readyRead, this, 0);
        mutex_imgque.lock();
        for (auto x : listImage){
            x->frame.release();
            free(x);
        }
        listImage.clear();
        mutex_imgque.unlock();
        disconnect(&timer, &QTimer::timeout, this, 0);
        ui->startBtn->setText("Start");
        m_stat = Ui::IDLE;
        return;
    }
    else{
        if (  //ui->videoEdit->text().trimmed().contains("http://") ||
              ui->videoEdit->text().trimmed().contains("udp://")){
            QString addr = ui->videoEdit->text().trimmed();
            QStringList addr_list = addr.split(":");
            int port = addr_list.at(addr_list.size()-1).toInt();
            rcver = new QUdpSocket(this);
            rcver->bind(QHostAddress("172.16.20.116"), port+1);
            rcved_firstKeyFrame = false;
            connect(rcver, &QUdpSocket::readyRead, this, &LiveClientWindow::processPendingDatagram);
            vtype = Ui::VIDEOTYPE::NETWORK;
        }
        else if(ui->videoEdit->text().trimmed().contains("rtsp://") || ui->videoEdit->text().trimmed().contains("rtp://")){
            // TODO: check the validality of RTSP url
            // If want to use udp://..., just change the url to:
            //                      udp://srcIP:srcPORT
            m_stat = Ui::CONNECTING;
        }
        else{
            QMessageBox::critical(this,
                                      "Video Error",
                                      "Make sure your RTSP url is valid!");
            return;
        }
        decoder->set_filename_Run(ui->videoEdit->text().trimmed().toStdString());
        connect(&timer, &QTimer::timeout, this, &LiveClientWindow::updateFrame);
        timer.start(20);
        ui->startBtn->setText("Stop");
        m_stat = Ui::PLAYING;
    }
}

void LiveClientWindow::updateFrame(){
    if (!decoded_listOver){
//        IplImage *pimage = &IplImage(frame);
//        IplImage *image = cvCloneImage(pimage);
        JYFrame* jyf;
        cv::Mat frame;
        int64_t l_time;
        bool updated = false;
        mutex_imgque.lock();
        while(listImage.size() > 0){
            jyf = listImage[0];
    //        listImage.append(image);
            listImage.pop_front();
            qDebug("[Player] listImage count : %d\n", listImage.size());
            updated = true;
            if(listImage.size() > 0){
                jyf->frame.release();
                free(jyf);
            }
        }
        mutex_imgque.unlock();
        if (!updated)
            return;

        frame = jyf->frame;
        l_time = jyf->launch_time;
        jyf->frame.release();
        free(jyf);

        QImage qimg((uchar*)(frame.data),
                    frame.cols,
                    frame.rows,
                    frame.step,
                    QImage::Format_RGB888);
        // qimg.scaled(ui->graphicsView->width(), ui->graphicsView->height());
        // If your QImage is not the same size as the label and setPixmap does the scaling,
        // you could perhaps use a faster scaling routine
        QTime cur_time;
        cur_time.start();
        QPixmap tmpPixMap = QPixmap::fromImage(qimg.rgbSwapped());
        pixmap.setPixmap(tmpPixMap);
        qDebug("[Player] update pixmap: %d\n", cur_time.elapsed());
        frame.release();
        //qApp->processEvents();
    }
    else{
        on_startBtn_pressed();
    }
}
// http://172.16.20.252:23323
// C:\Users\admin\Videos\cam\hhhh.mp4
// C:\Users\Public\Videos\Sample Videos\Wildlife.wmv
// C:\Users\admin\Videos\Prepar3D\Prepar3D 04.13.2018 - 16.53.52.01.mp4
// C:\Users\admin\Desktop\shifeiJ20\VID_20180513_203441.mp4_20180526_155356.mkv
// D:\LYQ\entertainment\18s.Relying.on.Heaven.to.Slaughter.Dragons.2019.E43.1080p.WEB-DL.H264.mpeg
// D:\LYQ\entertainment\49s.Relying.on.Heaven.to.Slaughter.Dragons.2019.E43.1080p.WEB-DL.H264.mpeg



//void LiveClientWindow::on_startBtn_pressed(){
//    //using namespace cv;
//    if(video.isOpened()){
//        ui->startBtn->setText("Start");
//        video.release();
//        return;
//    }
//    bool isCamera;
//    int cameraIndex = ui->videoEdit->text().toInt(&isCamera);
//    if(isCamera){
//        if(!video.open(cameraIndex)){
//            QMessageBox::critical(this, "amera Error", "Make sure you entered a correct camera index, or that the camera is not being accessed by another program!");
//            return;
//        }
//    }
//    else{
//        if(!video.open(ui->videoEdit->text().trimmed().toStdString())){
//                QMessageBox::critical(this,
//                                          "Video Error",
//                                          "Make sure you entered a correct and supported video file path,"
//                                          "<br>or a correct RTSP feed URL!");
//        }
//    }

//    ui->startBtn->setText("Stop");

//    cv::Mat frame;
//    QTime cur_time;
//    while(video.isOpened()){
//        cur_time.start();
//        video >> frame;
//        printf("video >> frame,  time : %d\n", cur_time.elapsed());
//        cur_time.start();
//        if (!frame.empty()){
////            cv::copyMakeBorder(frame,
////                               frame,
////                               frame.rows/2,
////                               frame.rows/2,
////                               frame.cols/2,
////                               frame.cols/2,
////                               cv::BORDER_ISOLATED);

//            QImage qimg((uchar*)(frame.data),
//                        frame.cols,
//                        frame.rows,
//                        frame.step,
//                        QImage::Format_RGB888);
//            //qimg.scaled(ui->graphicsView->width(), ui->graphicsView->height());
//            printf("qimg,  time : %d\n", cur_time.elapsed());
//            cur_time.start();
//            // If your QImage is not the same size as the label and setPixmap does the scaling,
//            // you could perhaps use a faster scaling routine
//            pixmap.setPixmap(QPixmap::fromImage(qimg.rgbSwapped()));
//            //ui->graphicsView->fitInView(&pixmap, Qt::KeepAspectRatio);
//        }
//        printf("setPixmap time : %d\n", cur_time.elapsed());
//        cur_time.start();
//        qApp->processEvents();
//        printf("qApp time : %d\n", cur_time.elapsed());
//    }
//    ui->startBtn->setText("Start");
//    printf("ui height %d and width %d\n",ui->graphicsView->width(), ui->graphicsView->height());
//}
// http://172.16.20.252:23323
// /media/teeshark/OS/Users/Public/Videos/Sample Videos/animal.wmv
// /media/teeshark/OS/Users/admin/Videos/Prepar3D/Prepar3D 04.13.2018 - 16.53.52.01.mp4
// /media/teeshark/OS/Users/admin/Desktop/shifeiJ20/VID_20180513_203441.mp4_20180526_155356.mkv
// /media/teeshark/Work/LYQ/entertainment/Relying.on.Heaven.to.Slaughter.Dragons.E49.1080P.WEB-DL.AAC.H264-DanNi.mp4
// /media/teeshark/Work/LYQ/entertainment/18s.Relying.on.Heaven.to.Slaughter.Dragons.2019.E43.1080p.WEB-DL.H264.mpeg
// /media/teeshark/Work/LYQ/entertainment/49s.Relying.on.Heaven.to.Slaughter.Dragons.2019.E43.1080p.WEB-DL.H264.mpeg


void LiveClientWindow::processPendingDatagram(){
    QByteArray datagram;
    datagram.resize(this->rcver->pendingDatagramSize());
    this->rcver->readDatagram(datagram.data(), datagram.size());
    QDataStream in_data(&datagram, QIODevice::ReadOnly);
    // encapulate into AVPacket
    AVPacket *newpkt = av_packet_alloc();
    int64_t *p64;
    int * p32;
    uint16_t * up16;
    char head_field[62];
    int ret = in_data.readRawData(head_field, 62);
    assert(ret >= 62);
//    p32 = (int*)(head_field+48);       newpkt->side_data_elems = *p32;
    newpkt->side_data_elems = 0;
    newpkt->side_data = NULL;
    p32 = (int*)(head_field+58);        newpkt->size = *p32;
    assert(newpkt->size > 0);
    ret = av_new_packet(newpkt, newpkt->size);
    p64 = (int64_t*)head_field;        newpkt->convergence_duration = *p64;
    p64 = (int64_t*)(head_field+8);    newpkt->dts = *p64;
    p64 = (int64_t*)(head_field+16);   newpkt->duration = *p64;
    p32 = (int*)(head_field+24);       newpkt->flags = *p32;
    p64 = (int64_t*)(head_field+28);   newpkt->pos = *p64;
    p64 = (int64_t*)(head_field+36);   newpkt->pts = *p64;
    p32 = (int*)(head_field+44);       newpkt->stream_index = *p32;
    if (ret < 0){
        printf("Allocating memory for new packet failed.");
        return;
    }
    ret = in_data.readRawData((char*)newpkt->data, newpkt->size);
    if (!rcved_firstKeyFrame && !(newpkt->flags & AV_PKT_FLAG_KEY))
        return;
    mutex_pktl.lock();
    AVPacketList *pktl_next = (AVPacketList *)av_mallocz(sizeof(AVPacketList));
    pktl_next->pkt = *newpkt;
    pktl_next->next = NULL;
    if (pktl_end !=NULL)
        pktl_end->next = pktl_next;
    if (pktl == NULL)
        pktl = pktl_next;
    pktl_end = pktl_next;
    rcved_firstKeyFrame = true;
    mutex_pktl.unlock();
}


void LiveClientWindow::closeEvent(QCloseEvent *event){
    if(decoder!=NULL && decoder->isRunning()){
        QMessageBox::warning(this, "Warning",
                             "Stop the video before closing the application!");
        //event->ignore();
        on_startBtn_pressed();
        event->accept();
    }
    else{
        event->accept();
    }
}
