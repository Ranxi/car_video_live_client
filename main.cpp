#include "liveclientwindow.h"
#include <QApplication>
#include "decoder.h"
#include <QMutex>
#include <QList>


QMutex mutex_imgque;
QList<JYFrame* > listImage;

QMutex mutex_pktl;
AVPacketList *pktl = NULL;
AVPacketList *pktl_end = NULL;
bool decoded_listOver;
//QList<IplImage*> listImage;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    LiveClientWindow w;
    w.show();
    Decoder abc;
    decoded_listOver = true;

    w.set_decoder(&abc);
    return a.exec();
}
