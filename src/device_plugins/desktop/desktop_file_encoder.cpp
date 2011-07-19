#include "desktop_file_encoder.h"

static const int DEFAULT_VIDEO_STREAM_ID = 4113;

extern "C" 
{
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
}

struct FffmpegLog
{
    static void av_log_default_callback_impl(void* ptr, int level, const char* fmt, va_list vl)
    {
        Q_UNUSED(level)
            Q_UNUSED(ptr)
            Q_ASSERT(fmt && "NULL Pointer");

        if (!fmt) {
            return;
        }
        static char strText[1024 + 1];
        vsnprintf(&strText[0], sizeof(strText) - 1, fmt, vl);
        va_end(vl);

        qDebug() << "ffmpeg library: " << strText;
    }
};


int DesktopFileEncoder::initIOContext()
{
    enum {
        MAX_READ_SIZE = 1024 * 4*2
    };

    struct IO_ffmpeg {
        static qint32 writePacket(void* opaque, quint8* buf, int bufSize)
        {
            DesktopFileEncoder* stream = reinterpret_cast<DesktopFileEncoder *>(opaque);
            return stream ? stream->writePacketImpl(buf, bufSize) : 0;
        }
        static qint32 readPacket(void *opaque, quint8* buf, int size)
        {
            Q_ASSERT(opaque && "NULL Pointer");
            DesktopFileEncoder* stream = reinterpret_cast<DesktopFileEncoder *>(opaque);
            return stream ? stream->readPacketImpl(buf, size) : 0;
        }
        static qint64 seek(void* opaque, qint64 offset, qint32 whence)
        {
            Q_ASSERT(opaque && "NULL Pointer");
            DesktopFileEncoder* stream = reinterpret_cast<DesktopFileEncoder *>(opaque);
            return stream ? stream->seekPacketImpl(offset, whence) : 0;

        }
    };
    m_buffer.resize(MAX_READ_SIZE);
    m_iocontext = new ByteIOContext();
    int res = init_put_byte(m_iocontext, &m_buffer[0], MAX_READ_SIZE, 1, this, &IO_ffmpeg::readPacket, &IO_ffmpeg::writePacket, &IO_ffmpeg::seek);
    return res >= 0;
}
qint32 DesktopFileEncoder::writePacketImpl(quint8* buf, qint32 bufSize)
{
    Q_ASSERT(buf && "NULL Pointer");
    return m_device ? m_device->write(reinterpret_cast<const char *>(buf), bufSize) : 0;
}
qint32 DesktopFileEncoder::readPacketImpl(quint8* buf, quint32 bufSize)
{
    Q_ASSERT(buf && "NULL Pointer");
    Q_UNUSED(bufSize);
    return 0;
}
qint64 DesktopFileEncoder::seekPacketImpl(qint64 offset, qint32 whence)
{
    Q_UNUSED(whence);
    if (!m_device) {
        return -1;
    }
    if (whence != SEEK_END && whence != SEEK_SET && whence != SEEK_CUR) {
        return -1;
    }
    if (SEEK_END == whence && -1 == offset) {
        return UINT_MAX;
    }
    if (SEEK_END == whence) {
        offset = m_device->size() - offset;
        offset = m_device->seek(offset) ? offset : -1;
        return offset;
    }
    if (SEEK_CUR == whence) {
        offset += m_device->pos();
        offset = m_device->seek(offset) ? offset : -1;
        return offset;
    }
    if (SEEK_SET == whence) {
        offset = m_device->seek(offset) ? offset : -1;
        return offset;
    }
    return -1;
}





DesktopFileEncoder::DesktopFileEncoder(const QString& fileName, int desktopNum):
    CLLongRunnable(),
    m_videoBuf(0),
    m_videoBufSize(0),
    m_videoCodecCtx(0),
    m_initialized(false),
    m_fileName(fileName),
    m_device(0),
    m_iocontext(0),
    m_formatCtx(0)
{
    m_grabber = new CLBufferedScreenGrabber(desktopNum);
    m_encoderCodecName = "mpeg2video";
    //m_encoderCodecName = "mpeg4";
    m_needStop = false;
    start();
}

DesktopFileEncoder::~DesktopFileEncoder()
{
    m_needStop = true;
    wait();
    closeStream();
    delete m_device;
}

bool DesktopFileEncoder::init()
{
    AVRational st_rate = {1,90000};

    avcodec_init();
    av_register_all();


    av_log_set_callback(FffmpegLog::av_log_default_callback_impl);


    m_videoBufSize = avpicture_get_size((PixelFormat) m_grabber->format(), m_grabber->width(), m_grabber->height());
    m_videoBuf = (quint8*) av_malloc(m_videoBufSize);

    m_frame = avcodec_alloc_frame();
    avpicture_alloc((AVPicture*) m_frame, m_grabber->format(), m_grabber->width(), m_grabber->height() );

    AVCodec* codec = avcodec_find_encoder_by_name(m_encoderCodecName);
    if(codec == 0)
    {
        cl_log.log("Can't find encoder", cl_logWARNING);
        return false;
    }
    
    // allocate container
    m_device = new QFile(m_fileName);
    m_device->open(QIODevice::WriteOnly);

    m_formatCtx = avformat_alloc_context();
    initIOContext();
    m_formatCtx->pb = m_iocontext;
    m_outputCtx = av_guess_format("mpegts",NULL,NULL);

    m_outputCtx->video_codec = codec->id;

    m_formatCtx->oformat = m_outputCtx;

    if (av_set_parameters(m_formatCtx, NULL) < 0) {
        cl_log.log("Invalid output format parameters", cl_logERROR);
        return false;
    }

    m_outStream = NULL;
    m_outStream = av_new_stream(m_formatCtx, DEFAULT_VIDEO_STREAM_ID);
    if (!m_outStream)
    {
        cl_log.log("Can't create output stream for encoding", cl_logWARNING);
        return false;
    }
    m_videoCodecCtx = m_outStream->codec;
    m_videoCodecCtx->codec_id = m_outputCtx->video_codec;
    m_videoCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO ;
    
    
    m_videoCodecCtx->time_base = m_grabber->getFrameRate();

    m_videoCodecCtx->pix_fmt = m_grabber->format(); 
    m_videoCodecCtx->width = m_grabber->width();
    m_videoCodecCtx->height = m_grabber->height();

    
    m_videoCodecCtx->gop_size = 12;
    m_videoCodecCtx->has_b_frames = 0;
    m_videoCodecCtx->max_b_frames = 0;
    m_videoCodecCtx->me_threshold = 0;
    m_videoCodecCtx->intra_dc_precision = 0;
    m_videoCodecCtx->strict_std_compliance = 0;
    m_videoCodecCtx->me_method = ME_EPZS;
    m_videoCodecCtx->bit_rate = 1024*1024*8;

    //m_videoCodecCtx->rc_min_rate = m_videoCodecCtx->bit_rate;
    //m_videoCodecCtx->rc_max_rate = m_videoCodecCtx->bit_rate;
    //m_videoCodecCtx->rc_buffer_size= m_videoCodecCtx->bit_rate*2;
    //m_videoCodecCtx->bit_rate_tolerance = 0.1;
    
    //m_videoCodecCtx->qmin = 5;
    //m_videoCodecCtx->qmax = 5;
    

    m_videoCodecCtx->sample_aspect_ratio.num = 1;
    m_videoCodecCtx->sample_aspect_ratio.den = 1;
    

    m_outStream->sample_aspect_ratio.den = 1;
    m_outStream->sample_aspect_ratio.num = 1;


    //m_videoCodecCtx->qmin = m_videoCodecCtx->qmax = 3;
    //m_videoCodecCtx->mb_lmin = m_videoCodecCtx->lmin = m_videoCodecCtx->qmin * FF_QP2LAMBDA;
    //m_videoCodecCtx->mb_lmax = m_videoCodecCtx->lmax = m_videoCodecCtx->qmax * FF_QP2LAMBDA;
    //m_videoCodecCtx->flags |= CODEC_FLAG_QSCALE;

    if (avcodec_open(m_videoCodecCtx, codec) < 0)
    {
        cl_log.log("Can't initialize encoder", cl_logWARNING);
        return false;
    }

    m_outStream->first_dts = 0;
    m_videoStream = m_outStream->index;
    m_outStream->pts.num=st_rate.num; // 1
    m_outStream->pts.den=st_rate.den; // 90000

    av_write_header(m_formatCtx);

    return true;
}

void DesktopFileEncoder::run()
{
    openStream();

    while (!m_needStop)
    {
        IDirect3DSurface9* surface = m_grabber->getNextFrame();
        if (!surface)
            continue;
        m_grabber->SurfaceToFrame(surface, m_frame);

        m_frame->pts = m_frame->coded_picture_number;

        int out_size = avcodec_encode_video(m_videoCodecCtx, m_videoBuf, m_videoBufSize, m_frame);
        if (out_size < 1)
            continue;

        AVStream * stream = m_formatCtx->streams[m_videoStream];
        AVPacket pkt;
        av_init_packet(&pkt);
        if (m_videoCodecCtx->coded_frame->pts != AV_NOPTS_VALUE)
            pkt.pts = av_rescale_q(m_videoCodecCtx->coded_frame->pts, m_videoCodecCtx->time_base, stream->time_base);

        //cl_log.log("pkt.pts=" , pkt.pts, cl_logALWAYS);

        if(m_videoCodecCtx->coded_frame->key_frame)
            pkt.flags |= AV_PKT_FLAG_KEY;

        pkt.stream_index= stream->index;
        pkt.data = m_videoBuf;
        pkt.size = out_size;

        if (av_write_frame(m_formatCtx,&pkt)<0)	
        {
            QString s ="VIDEO av_write_frame(formatCtx,&pkt) error. current time:"+QTime::currentTime().toString() +"\n";
            s+="packet pts: " + QString::number(pkt.pts) + "dts:"+QString::number(pkt.dts) + "\n";
            s+="error code: " + QString::number(m_iocontext->error) + "\n";
            s+="eof_reached: " + QString::number(m_iocontext->eof_reached) + "\n";
            s+="pos: " + QString::number(m_iocontext->pos) + "\n";
            s+="write_flag: " + QString::number(m_iocontext->write_flag) + "\n";
            s+= "will exit now!\n";
            cl_log.log(s, cl_logWARNING);
            av_free_packet(&pkt);
            exit(-1);
        }
        av_free_packet(&pkt);

    }
    closeStream();
}

void DesktopFileEncoder::openStream()
{
    if (init())
        m_initialized = true;
}

void DesktopFileEncoder::closeStream()
{
    delete m_grabber;
    m_grabber = 0;
    if (m_videoCodecCtx)
        avcodec_close(m_videoCodecCtx);
    m_videoCodecCtx = 0;
    if (m_frame)
        av_free(m_frame);
    m_frame = 0;
    if (m_videoBuf)
        av_free(m_videoBuf);
    m_videoBuf = 0;
    if (m_device)
        m_device->close();

    if (m_formatCtx)
        av_close_input_stream(m_formatCtx);
    if (m_iocontext)
        av_free(m_iocontext);

    m_initialized = false;
}
