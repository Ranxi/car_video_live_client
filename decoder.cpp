#include <decoder.h>
#include <cinttypes>
#include <QMutex>
#include <QTime>
#include <ext/hash_map>
#include <algorithm>


extern QMutex mutex_imgque;
extern QList<cv::Mat > listImage;
extern bool listOver;

__gnu_cxx::hash_map<int, int64_t> buff_latency;
std::vector<int> laten_list;
int v_stream_idx, a_stream_idx;
int width, height;
enum AVPixelFormat pix_fmt;
int video_frame_count = 0;
int audio_frame_count = 0;
int refcount = 0;

Decoder::Decoder(QObject *parent):QThread(parent)
{
//    qDebug("%s", avcodec_configuration());
//    qDebug("version: %d", avcodec_version());
}

Decoder::~Decoder(void){

}


void Decoder::run()
{
    decode_iplimage();
}


void Decoder::set_filename_Run(std::string fnm){
    this->filename = fnm;
    this->start();
}

static cv::Mat avframe_to_cvmat(const AVFrame * frame){
    // |>>>>>>>>> code from dalao, refer to: https://zhuanlan.zhihu.com/p/80350418 >>>>>>>>
    int width = frame->width;
    int height = frame->height;
    cv::Mat image(height, width, CV_8UC3);
    int cvLinesizes[1];
    cvLinesizes[0] = image.step1();
    SwsContext* conversion = sws_getContext(width, height, (AVPixelFormat) frame->format,
                                            width, height, AVPixelFormat::AV_PIX_FMT_BGR24,
                                            SWS_FAST_BILINEAR, NULL, NULL, NULL);
    sws_scale(conversion, frame->data, frame->linesize, 0, height, &image.data, cvLinesizes);
    sws_freeContext(conversion);
    return image;
}

static void cvmat_to_avframe(cv::Mat &matframe, AVFrame *frame){
//    AVFrame dst;
////    cvtColor( matframe , matframe , CV_BGR2RGB ) ;
//    cv::Size frameSize = matframe->size();

//    for(int h=0; h < frameSize.height; h++){
//        memcpy(&(frame->data[0][h*frame->linesize[0]]), &(matframe->data[h*matframe->step]), frameSize.width*3);
//    }
//    return ;
    // |>>>>>>>>> code from dalao, refer to: https://zhuanlan.zhihu.com/p/80350418 >>>>>>>>
    int width = matframe.cols;
    int height = matframe.rows;
    // qDebug("matframe cols: %d, rows: %d", width, height);
    if ((matframe.cols <= 0 || matframe.cols > 2000) || matframe.rows <= 0 || matframe.rows > 2000)
        qDebug("something wrong with matframe");
    int cvLinesizes[1];
    cvLinesizes[0] = matframe.step1();
    if(frame==NULL){
        frame = av_frame_alloc();
        av_image_alloc(frame->data, frame->linesize, width, height, AVPixelFormat::AV_PIX_FMT_YUV420P, 1);
    }
    SwsContext * conversion = sws_getContext(width, height, AVPixelFormat::AV_PIX_FMT_BGR24,
                                             width, height, (AVPixelFormat)frame->format,
                                             SWS_FAST_BILINEAR,NULL,NULL,NULL);
    sws_scale(conversion, &matframe.data, cvLinesizes, 0, height, frame->data, frame->linesize);
    sws_freeContext(conversion);
    // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>|
}

static void decode(AVCodecContext *enc_ctx, AVPacket *pkt, AVFrame *frame)
{
    int ret, ret2, acquire_lock = false;
    /* send the frame to the Decoder */
    if (pkt)
        qDebug("[Decoder] ----> Send packet %3""lld""\n", pkt->pts);
    ret = avcodec_send_packet(enc_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error sending a packet for encoding\n");
        exit(1);
    }
    while (ret >= 0) {
        ret = avcodec_receive_frame(enc_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
            // 在解码或编码开始时，编解码器可能会接收多个输入帧/数据包而不返回帧，
            // 直到其内部缓冲区被填充为止。
            return;
        }
        else if (ret < 0) {
            printf("Error during encoding\n");
            exit(1);
        }
//        printf("Write packet %3""ld"" (size=%5d)\n", pkt->pts, pkt->size);
//        fwrite(pkt->data, 1, pkt->size, outfile);
        mutex_imgque.lock();
        acquire_lock = true;
        qDebug("[Decoder] ----> Receive frame %3""lld"", latency: %d\n", frame->pts, av_gettime()-buff_latency[frame->pts]);
        laten_list.push_back(av_gettime()-buff_latency[frame->pts]);
        buff_latency.erase(buff_latency[frame->pts]);
        listImage.append(avframe_to_cvmat(frame));
        // av_packet_unref(pkt);
    }
    if (acquire_lock)
        mutex_imgque.unlock();
}

static int decode_packet(AVCodecContext *dec_v_ctx, AVCodecContext *dec_a_ctx, AVPacket *pkt, AVFrame *frame, int *got_frame, int cached)
{
    int ret = 0;
    int decoded = pkt->size;
    *got_frame = 0;
    if (pkt->stream_index == v_stream_idx) {
        qDebug("[Decoder] ----> Send packet %3""lld""\n", pkt->pts);
        ret = avcodec_decode_video2(dec_v_ctx, frame, got_frame, pkt);
        if (ret < 0) {
            fprintf(stderr, "Error decoding video frame (%s)\n", av_err2str(ret));
            return ret;
        }
        if (*got_frame){
            if (frame->width != width || frame->height != height ||
                frame->format != pix_fmt) {
                /* To handle this change, one could call av_image_alloc again and
                 * decode the following frames into another rawvideo file. */
                fprintf(stderr, "Error: Width, height and pixel format have to be "
                        "constant in a rawvideo file, but the width, height or "
                        "pixel format of the input video changed:\n"
                        "old: width = %d, height = %d, format = %s\n"
                        "new: width = %d, height = %d, format = %s\n",
                        width, height, av_get_pix_fmt_name(pix_fmt),
                        frame->width, frame->height,
                        av_get_pix_fmt_name((AVPixelFormat)frame->format));
                return -1;
            }
//            printf("video_frame%s n:%d coded_n:%d\n",
//                               cached ? "(cached)" : "",
//                               video_frame_count++, frame->coded_picture_number);
            qDebug("[Decoder] ----> Receive frame %3""lld"", latency: %d\n", pkt->pts, av_gettime()-buff_latency[pkt->pts]);
            laten_list.push_back(av_gettime()-buff_latency[frame->pts]);
            buff_latency.erase(buff_latency[frame->pts]);
            mutex_imgque.lock();
            listImage.append(avframe_to_cvmat(frame));
            mutex_imgque.unlock();
        }
    }
    else if (pkt->stream_index == a_stream_idx){
        ret = avcodec_decode_audio4(dec_a_ctx, frame, got_frame, pkt);
        if (ret < 0) {
            fprintf(stderr, "Error decoding audio frame (%s)\n", av_err2str(ret));
            return ret;
        }
        decoded = FFMIN(ret, pkt->size);
        if (*got_frame) {
            size_t unpadded_linesize = frame->nb_samples * av_get_bytes_per_sample((AVSampleFormat)frame->format);
            printf("audio_frame%s n:%d nb_samples:%d pts:%s\n",
                cached ? "(cached)" : "",
                audio_frame_count++, frame->nb_samples,
                av_ts2timestr(frame->pts, &dec_a_ctx->time_base));
            // add to audio queue
        }
    }
    if (*got_frame)
        av_frame_unref(frame);

    return decoded;
}

int open_codec_context(int *stream_idx, AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type)
{
    int ret, stream_index;
    AVStream *st;
    AVCodec *dec = NULL;
    AVDictionary *opts = NULL;
    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input file '%s'\n",
                av_get_media_type_string(type), fmt_ctx->filename);
        return ret;
    } else {
        stream_index = ret;
        st = fmt_ctx->streams[stream_index];
        /* find decoder for the stream */
        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            fprintf(stderr, "Failed to find %s codec\n", av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }
        /* Allocate a codec context for the decoder */
        *dec_ctx = avcodec_alloc_context3(dec);
        if (!*dec_ctx) {
            fprintf(stderr, "Failed to allocate the %s codec context\n", av_get_media_type_string(type));
            return AVERROR(ENOMEM);
        }
        /* Copy codec parameters from input stream to output codec context */
        if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {
            fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
                    av_get_media_type_string(type));
            return ret;
        }
        /* Init the decoders, with or without reference counting */
        av_dict_set(&opts, "refcounted_frames", refcount ? "1" : "0", 0);
        if ((ret = avcodec_open2(*dec_ctx, dec, &opts)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n", av_get_media_type_string(type));
            return ret;
        }
        *stream_idx = stream_index;
    }
    return 0;
}

void Decoder::decode_iplimage(){
    avcodec_register_all();

    const AVCodec *codec;
    AVCodecContext *video_c= NULL, *audio_c=NULL;
    AVFormatContext *fmtctx;
    AVStream *video_st=NULL, *audio_st=NULL;
    int i, ret, x, y;
    v_stream_idx = -1, a_stream_idx = -1;
    //FILE *f;
    AVFrame *frame; // *tmpframe;
    AVPacket *pkt;
    uint8_t endcode[] = { 0, 0, 1, 0xb7 };
    listOver = false;


    // |>>>>>>>>>>>>>>> init for RTSP >>>>>>>>>>>>>
    avformat_network_init();
    // open input file, and allocate format context
    fmtctx = avformat_alloc_context();
    if(avformat_open_input(&fmtctx, this->filename.c_str(), NULL, NULL) < 0 ){
        fprintf(stderr, "Could not open source file %s\n", this->filename.c_str());
        exit(1);
    }
    // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>|
    /* retrieve stream information */
    if (avformat_find_stream_info(fmtctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        exit(1);
    }

    // get video stream
    if (open_codec_context(&v_stream_idx, &video_c, fmtctx, AVMEDIA_TYPE_VIDEO) >= 0) {
        video_st = fmtctx->streams[v_stream_idx];
        width = video_c->width;
        height = video_c->height;
        pix_fmt = video_c->pix_fmt;
    }

//    // get audio stream
//    if (open_codec_context(&a_stream_idx, &audio_c, fmtctx, AVMEDIA_TYPE_AUDIO) >= 0) {
//        audio_st = fmtctx->streams[a_stream_idx];
//    }


//    if(fmtctx->oformat->flags & AVFMT_GLOBALHEADER)
//        video_c->flags|=  AV_CODEC_FLAG_GLOBAL_HEADER;
//    av_opt_set(video_c->priv_data, "preset", "veryfast", 0);
//    av_opt_set(video_c->priv_data, "tune","stillimage,zerolatency",0);
//    av_opt_set(c->priv_data, "x264opts","crf=26:vbv-maxrate=728:vbv-bufsize=364:keyint=25",0);


//    AVDictionary *format_opts = NULL;
//    av_dict_set(&format_opts, "stimeout", std::to_string(2 * 1000000).c_str(), 0);
//    av_dict_set(&format_opts, "rtsp_transport", "udp", 0);


    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }

    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate the video frame data: %s\n", av_err2str(ret));
        exit(1);
    }
    // alloc packet for receiving encoded img
    pkt = av_packet_alloc();
    if (!pkt)
        exit(1);
    /* encode video */
    QTime cur_time;
    cur_time.start();
    int sum_time = 0, intval;
    int index = 0;
    int got_frame;

    pkt->size = 0;

    while (!isInterruptionRequested() && av_read_frame(fmtctx, pkt) >=0){
        AVPacket orig_pkt = *pkt;
        buff_latency[pkt->pts] = av_gettime();
        do {
            ret = decode_packet(video_c, audio_c, pkt, frame, &got_frame, 0);
            if (ret < 0)
                break;
            pkt->data += ret;
            pkt->size -= ret;
        } while (pkt->size > 0);
        av_packet_unref(&orig_pkt);
    }

    // flush cached frames
    pkt->data = NULL;
    pkt->size = 0;
    do {
        decode_packet(video_c, audio_c, pkt, frame, &got_frame, 1);
    } while (got_frame);

    listOver = true;


    int64_t sum_laten = 0;
    for(auto x : laten_list)
        sum_laten += x;
    qDebug("\navg latency:%f\n", (float)sum_laten*1.0/laten_list.size());

    intval = cur_time.elapsed();
    sum_time += intval;
    qDebug("last interval: %d, total time: %d\n", intval, sum_time);

    avcodec_free_context(&video_c);
    avformat_close_input(&fmtctx);
    av_frame_free(&frame);
    //av_packet_free(&pkt);
    //avformat_free_context(fmtctx);

}
