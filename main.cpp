#include "liveclientwindow.h"
#include <QApplication>
#include "decoder.h"
#include <QMutex>
#include <QList>


QMutex mutex_imgque;
QList<cv::Mat > listImage;
bool listOver;
//QList<IplImage*> listImage;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    LiveClientWindow w;
    w.show();
    Decoder abc;
    listOver = true;

    w.set_decoder(&abc);
    return a.exec();
}
