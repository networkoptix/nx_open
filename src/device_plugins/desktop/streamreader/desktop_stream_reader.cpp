#include "desktop_stream_reader.h"
#include "device/device.h"
#include "data/mediadata.h"

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

        //return;
        qDebug() << "ffmpeg library: " << strText;
    }
};


CLDesktopStreamreader::CLDesktopStreamreader(CLDevice* dev):
    CLServerPushStreamreader(dev),
    m_videoBuf(0),
    m_videoBufSize(0),
    m_videoCodecCtx(0),
    m_initialized(false)
{
    QString num= dev->getUniqueId();
    int idx = num.length()-1;
    while (idx >= 0 && num[idx] >= '0' && num[idx] <= '9')
        idx--;
    m_grabber = new CLBufferedScreenGrabber(num.right(num.length()-idx-1).toInt()-1);
    m_encoderCodecName = "mpeg2video";
    //m_encoderCodecName = "mpeg4";
}

CLDesktopStreamreader::~CLDesktopStreamreader()
{
    closeStream();
}

bool CLDesktopStreamreader::init()
{
    // must be called before using avcodec 
    avcodec_init();
    // register all the codecs (you can also register only the codec you wish to have smaller code
    avcodec_register_all();

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
    
    m_videoCodecCtx = avcodec_alloc_context();

    m_videoCodecCtx->time_base = m_grabber->getFrameRate();
    m_videoCodecCtx->pix_fmt = m_grabber->format(); 
    m_videoCodecCtx->width = m_grabber->width();
    m_videoCodecCtx->height = m_grabber->height();
    m_videoCodecCtx->has_b_frames = 0;
    m_videoCodecCtx->me_method = ME_EPZS;
    m_videoCodecCtx->sample_aspect_ratio.num = 1;
    m_videoCodecCtx->sample_aspect_ratio.den = 1;
    m_videoCodecCtx->bit_rate = 1024*1024*8;

    m_videoCodecCtx->qmin = m_videoCodecCtx->qmax = 3;
    m_videoCodecCtx->mb_lmin = m_videoCodecCtx->lmin = m_videoCodecCtx->qmin * FF_QP2LAMBDA;
    m_videoCodecCtx->mb_lmax = m_videoCodecCtx->lmax = m_videoCodecCtx->qmax * FF_QP2LAMBDA;
    m_videoCodecCtx->flags |= CODEC_FLAG_QSCALE;

    if (avcodec_open(m_videoCodecCtx, codec) < 0)
    {
        cl_log.log("Can't initialize encoder", cl_logWARNING);
        return false;
    }

    return true;
}

CLAbstractMediaData* CLDesktopStreamreader::getNextData()
{
    if (!m_initialized)
        return 0;
    while (!m_needStop)
    {
        IDirect3DSurface9* surface = m_grabber->getNextFrame();
        if (!surface)
            continue;
        m_grabber->SurfaceToFrame(surface, m_frame);

        int out_size = avcodec_encode_video(m_videoCodecCtx, m_videoBuf, m_videoBufSize, m_frame);
        if (out_size < 1)
            continue;

        CLCompressedVideoData* videoData = new CLCompressedVideoData(CL_MEDIA_ALIGNMENT, out_size);
        videoData->data.write((const char*) m_videoBuf, out_size);

        videoData->width = m_grabber->width();
        videoData->height = m_grabber->height();
        videoData->channelNumber = 0;
        videoData->keyFrame = m_videoCodecCtx->coded_frame->key_frame;
        videoData->compressionType = m_videoCodecCtx->codec_id;
        videoData->timestamp = m_frame->pts * 1000;
        return videoData;
    }
    return 0;
}

void CLDesktopStreamreader::openStream()
{
    if (init())
        m_initialized = true;
}

void CLDesktopStreamreader::closeStream()
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

    m_initialized = false;
}

bool CLDesktopStreamreader::isStreamOpened() const
{
    return m_initialized;
}
