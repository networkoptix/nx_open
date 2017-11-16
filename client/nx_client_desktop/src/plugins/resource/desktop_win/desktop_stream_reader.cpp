#include "desktop_stream_reader.h"

#include <core/resource/resource.h>

#include <nx/streaming/config.h>

#include <nx/utils/log/log.h>
#include <utils/media/ffmpeg_helper.h>
#include <nx/streaming/video_data_packet.h>

struct FffmpegLog
{
    static void av_log_default_callback_impl(void* ptr, int level, const char* fmt, va_list vl)
    {
        Q_UNUSED(level)
            Q_UNUSED(ptr)
            NX_ASSERT(fmt && "NULL Pointer");

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


QnDesktopStreamreader::QnDesktopStreamreader(const QnResourcePtr &dev):
    QnAbstractStreamDataProvider(dev),
    m_videoBuf(0),
    m_videoBufSize(0),
    m_videoCodecCtx(0),
    m_initialized(false),
    m_frame(nullptr),
    m_outPacket(av_packet_alloc())
{
    QString num= dev->getUniqueId();
    int idx = num.length()-1;
    while (idx >= 0 && num[idx].unicode() >= '0' && num[idx].unicode() <= '9')
        idx--;
    m_grabber = new QnBufferedScreenGrabber(num.right(num.length()-idx-1).toInt()-1);
    m_encoderCodecName = "mpeg2video";
    m_grabber->start(QThread::HighestPriority);
    //m_encoderCodecName = "mpeg4";
}

QnDesktopStreamreader::~QnDesktopStreamreader()
{
    closeStream();
	av_packet_free(&m_outPacket);
}

bool QnDesktopStreamreader::init()
{
    av_log_set_callback(FffmpegLog::av_log_default_callback_impl);


    m_videoBufSize = avpicture_get_size((AVPixelFormat) m_grabber->format(), m_grabber->width(), m_grabber->height());
    m_videoBuf = (quint8*) av_malloc(m_videoBufSize);

    m_frame = av_frame_alloc();
    avpicture_alloc((AVPicture*) m_frame, m_grabber->format(), m_grabber->width(), m_grabber->height() );

    AVCodec* codec = avcodec_find_encoder_by_name(m_encoderCodecName);
    if(codec == 0)
    {
        NX_WARNING(this, "Can't find encoder");
        return false;
    }

    m_videoCodecCtx = avcodec_alloc_context3(codec);

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

    if (avcodec_open2(m_videoCodecCtx, codec, nullptr) < 0)
    {
        NX_WARNING(this, "Can't initialize encoder");
        return false;
    }

    return true;
}

QnAbstractMediaDataPtr QnDesktopStreamreader::getNextData()
{
    if (!m_initialized)
        return QnAbstractMediaDataPtr();

    while (!needToStop())
    {
        CaptureInfoPtr capturedData = m_grabber->getNextFrame();
        if (!capturedData || !capturedData->opaque || capturedData->width == 0 || capturedData->height == 0)
            continue;
        m_grabber->capturedDataToFrame(capturedData, m_frame);

        m_outPacket->data = m_videoBuf;
        m_outPacket->size = m_videoBufSize;
        int got_packet = 0;
        int encodeResult = avcodec_encode_video2(m_videoCodecCtx, m_outPacket, m_frame, &got_packet);

        if (encodeResult < 0)
            continue;

        if (got_packet)
        {
            QnWritableCompressedVideoData* videoData = new QnWritableCompressedVideoData(CL_MEDIA_ALIGNMENT, m_outPacket->size);
            videoData->m_data.write((const char*)m_videoBuf, m_outPacket->size);

            videoData->width = m_grabber->width();
            videoData->height = m_grabber->height();
            videoData->channelNumber = 0;
            if (m_videoCodecCtx->coded_frame->key_frame)
                videoData->flags |= QnAbstractMediaData::MediaFlags_AVKey;
            videoData->compressionType = m_videoCodecCtx->codec_id;
            videoData->timestamp = m_frame->pts * 1000;
            return QnAbstractMediaDataPtr(videoData);
        }
    }
    return QnAbstractMediaDataPtr();
}

CameraDiagnostics::Result QnDesktopStreamreader::openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params)
{
    Q_UNUSED(isCameraControlRequired);
    Q_UNUSED(params)
    if (init())
    {
        m_initialized = true;
        return CameraDiagnostics::NoErrorResult();
    }
    else
    {
        return CameraDiagnostics::Result(CameraDiagnostics::ErrorCode::unknown);
    }
}

void QnDesktopStreamreader::closeStream()
{
    delete m_grabber;
    m_grabber = 0;

    QnFfmpegHelper::deleteAvCodecContext(m_videoCodecCtx);
    m_videoCodecCtx = 0;
    if (m_frame)
        av_free(m_frame);
    m_frame = 0;
    if (m_videoBuf)
        av_free(m_videoBuf);
    m_videoBuf = 0;

    m_initialized = false;
}

bool QnDesktopStreamreader::isStreamOpened() const
{
    return m_initialized;
}
