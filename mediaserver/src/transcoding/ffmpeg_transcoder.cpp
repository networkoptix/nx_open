#include "ffmpeg_transcoder.h"
#include <QDebug>

extern QMutex global_ffmpeg_mutex;
static const int IO_BLOCK_SIZE = 1024*32;

static qint32 ffmpegReadPacket(void *opaque, quint8* buf, int size)
{
    Q_ASSERT_X(true, Q_FUNC_INFO, "This class for streaming encoding! This function call MUST not exists!");
    return 0;
}

static qint32 ffmpegWritePacket(void *opaque, quint8* buf, int size)
{
    QnFfmpegTranscoder* transcoder = reinterpret_cast<QnFfmpegTranscoder*> (opaque);
    return transcoder->m_dstByteArray->write((char*) buf, size);
}

static int64_t ffmpegSeek(void* opaque, int64_t pos, int whence)
{
    Q_ASSERT_X(true, Q_FUNC_INFO, "This class for streaming encoding! This function call MUST not exists!");
    return 0;
}

AVIOContext* QnFfmpegTranscoder::createFfmpegIOContext()
{
    quint8* ioBuffer;
    AVIOContext* ffmpegIOContext;

    ioBuffer = (quint8*) av_malloc(1024*32);
    ffmpegIOContext = avio_alloc_context(
        ioBuffer,
        IO_BLOCK_SIZE,
        1,
        this,
        &ffmpegReadPacket,
        &ffmpegWritePacket,
        &ffmpegSeek);

    return ffmpegIOContext;
}

QnFfmpegTranscoder::QnFfmpegTranscoder():
QnTranscoder(),
m_videoDecoder(0),
m_videoEncoderCodecCtx(0),
m_audioEncoderCodecCtx(0),
m_videoBitrate(0),
m_startDateTime(0),
m_formatCtx(0),
m_dstByteArray(0),
m_ioContext(0)
{
    m_videoEncodingBuffer = (quint8*) qMallocAligned(MAX_VIDEO_FRAME, 32);
}

QnFfmpegTranscoder::~QnFfmpegTranscoder()
{
    closeFfmpegContext();
    qFreeAligned(m_videoEncodingBuffer);
}

void QnFfmpegTranscoder::closeFfmpegContext()
{
    if (m_ioContext)
        avio_close(m_ioContext);
    m_ioContext = 0;
    if (m_formatCtx) {
        m_formatCtx->pb = 0;
        avformat_close_input(&m_formatCtx);
    }
}

int QnFfmpegTranscoder::setContainer(const QString& container)
{
    m_container = container;
    AVOutputFormat * outputCtx = av_guess_format(m_container.toLatin1().data(), NULL, NULL);
    if (outputCtx == 0)
    {
        m_lastErrMessage = tr("No %1 container in FFMPEG library.").arg(container);
        qWarning() << m_lastErrMessage;
        return -1;
    }

    global_ffmpeg_mutex.lock();
    int err = avformat_alloc_output_context2(&m_formatCtx, outputCtx, 0, "");
    global_ffmpeg_mutex.unlock();
    if (err != 0)
    {
        m_lastErrMessage = QString("Can't create output context for format") + container;
        qWarning() << m_lastErrMessage;
        return -2;
    }

    return 0;
}

bool QnFfmpegTranscoder::setVideoCodec(CodecID codec, QnVideoTranscoderPtr vTranscoder)
{
    QnTranscoder::setVideoCodec(codec, vTranscoder);
    return true;
}

bool QnFfmpegTranscoder::setAudioCodec(CodecID codec, QnAudioTranscoderPtr aTranscoder)
{
    QnTranscoder::setAudioCodec(codec, aTranscoder);
    return true;
}

int QnFfmpegTranscoder::open(QnCompressedVideoDataPtr video, QnCompressedAudioDataPtr audio)
{
    if (m_videoCodec != CODEC_ID_NONE)
    {
        if (!m_vTranscoder)
        {
            AVCodec* avCodec = avcodec_find_decoder(m_videoCodec);
            if (avCodec == 0)
            {
                m_lastErrMessage = tr("Transcoder error: can't find codec").arg(m_videoCodec);
                return false;
            }

            m_videoEncoderCodecCtx = avcodec_alloc_context3(avCodec);
            m_videoEncoderCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
            m_videoEncoderCodecCtx->codec_id = m_videoCodec;
            m_videoEncoderCodecCtx->width = video->width;
            m_videoEncoderCodecCtx->height = video->height;
            m_videoEncoderCodecCtx->pix_fmt = PIX_FMT_YUV420P;
            m_videoEncoderCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
            m_videoEncoderCodecCtx->bit_rate = video->width * video->height*5; // auto fill bitrate. 10Mbit for full HD
            m_videoEncoderCodecCtx->gop_size = 30;
            m_videoEncoderCodecCtx->time_base.num = 1;
            m_videoEncoderCodecCtx->time_base.den = 60;
        }

        AVStream* videoStream = av_new_stream(m_formatCtx, 0);
        if (videoStream == 0)
        {
            m_lastErrMessage = tr("Can't allocate output stream for recording.");
            cl_log.log(m_lastErrMessage, cl_logERROR);
            return -1;
        }

        if (m_videoStreamCopy) {
            AVCodecContext* srcContext = video->context->ctx();
            avcodec_copy_context(m_videoEncoderCodecCtx, srcContext);
        }
        videoStream->codec = m_videoEncoderCodecCtx;
    }

    if (m_audioCodec != CODEC_ID_NONE)
    {
        AVStream* audioStream = av_new_stream(m_formatCtx, 0);
        if (audioStream == 0)
        {
            m_lastErrMessage = tr("Can't allocate output stream for recording.");
            cl_log.log(m_lastErrMessage, cl_logERROR);
            return -1;
        }

        if (!m_aTranscoder) {
            AVCodecContext* srcContext = audio->context->ctx();
            avcodec_copy_context(m_audioEncoderCodecCtx, srcContext);
        }
        audioStream->codec = m_videoEncoderCodecCtx;
    }

    m_formatCtx->pb = m_ioContext = createFfmpegIOContext();

    int rez = avformat_write_header(m_formatCtx, 0);
    if (rez < 0) 
    {
        closeFfmpegContext();
        m_lastErrMessage = tr("Video or audio codec is incompatible with %1 format. Try another format.").arg(m_container);
        cl_log.log(m_lastErrMessage, cl_logERROR);
        return -3;
    }
    return 0;
}

int QnFfmpegTranscoder::transcodePacketInternal(QnAbstractMediaDataPtr media, QnByteArray& result)
{
    m_dstByteArray = &result;
    AVRational srcRate = {1, 1000000};
    AVStream* stream = m_formatCtx->streams[media->dataType == QnAbstractMediaData::VIDEO ? 0 : 1];
    AVPacket packet;
    av_init_packet(&packet);
    packet.pts = av_rescale_q(media->timestamp-m_startDateTime, srcRate, stream->time_base);
    packet.dts = packet.pts;
    if(media->flags & AV_PKT_FLAG_KEY)
        packet.flags |= AV_PKT_FLAG_KEY;
    packet.data = (quint8*) media->data.data();
    packet.size = media->data.size();

    QnCompressedVideoDataPtr video = qSharedPointerDynamicCast<QnCompressedVideoData>(media);
    /*
    if (video)
    {
        if (m_vTranscoder )
        {
            if (!m_videoDecoder) 
                m_videoDecoder = new CLFFmpegVideoDecoder(video->compressionType, video, false);
            m_videoDecoder->decode(video, &m_decodedVideoFrame);

            packet.size = avcodec_encode_video(m_videoEncoderCodecCtx, m_videoEncodingBuffer, MAX_VIDEO_FRAME, &m_decodedVideoFrame);
            packet.data = m_videoEncodingBuffer;
        }
    }
    */
    if (av_write_frame(m_formatCtx, &packet) < 0) {
        qWarning() << QLatin1String("Transcoder error: can't write AV packet");
        //return -1; // ignore error and continue
    }
    return 0;
}
