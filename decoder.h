#ifndef DECODER_H
#define DECODER_H

#include <math.h>
#include <QObject>
#include <QThread>
#include <QDebug>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavformat/version.h>
#include <libavutil/time.h>
#include <libavutil/mathematics.h>
#include <libavutil/imgutils.h>
#include <libavutil/timestamp.h>
}
#include "opencv2/imgproc/imgproc_c.h"


class Decoder : public QThread
{
    Q_OBJECT

public:
    Decoder(QObject *parent=0);
    ~Decoder(void);
    void decode_iplimage(const char *s);
    void run();
private:
    std::string filename;
public:
    void set_filename_Run(std::string fnm);
};

#endif // DECODER_H
