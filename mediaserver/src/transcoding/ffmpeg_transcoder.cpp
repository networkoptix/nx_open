#include "ffmpeg_transcoder.h"
#include <QDebug>

extern QMutex global_ffmpeg_mutex;
static const int IO_BLOCK_SIZE = 1024*32;

QnFfmpegVideoTranscoder::QnFfmpegVideoTranscoder(CodecID codecId):
    QnVideoTranscoder(codecId),
    m_videoDecoder(0)
{
    m_videoEncodingBuffer = (quint8*) qMallocAligned(MAX_VIDEO_FRAME, 32);
}

QnFfmpegVideoTranscoder::~QnFfmpegVideoTranscoder()
{
    qFreeAligned(m_videoEncodingBuffer);
}

int QnFfmpegVideoTranscoder::transcodePacket(QnAbstractMediaDataPtr media, QnAbstractMediaDataPtr& result)
{
    QnCompressedVideoDataPtr video = qSharedPointerDynamicCast<QnCompressedVideoData>(media);
    if (!m_videoDecoder) {
        m_videoDecoder = new CLFFmpegVideoDecoder(video->compressionType, video, false);

        AVCodec* avCodec = avcodec_find_decoder(m_codecId);
        if (avCodec == 0)
        {
            m_lastErrMessage = QObject::tr("Transcoder error: can't find codec").arg(m_codecId);
            return -2;
        }
        AVCodecContext* ctx = avcodec_alloc_context3(avCodec);
        ctx->codec_type = AVMEDIA_TYPE_VIDEO;
        ctx->codec_id = m_codecId;
        ctx->width = m_size.width();
        ctx->height = m_size.height();
        ctx->pix_fmt = PIX_FMT_YUV420P;
        ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

        ctx->bit_rate = m_bitrate;
        ctx->gop_size = 32;
        ctx->time_base.num = 1;
        ctx->time_base.den = 60;
        m_encoderCtx = QnMediaContextPtr(new QnMediaContext(ctx));
    }
    if (m_videoDecoder->decode(video, &m_decodedVideoFrame)) {
        int encoded = avcodec_encode_video(m_encoderCtx->ctx(), m_videoEncodingBuffer, MAX_VIDEO_FRAME, &m_decodedVideoFrame);
        if (encoded < 0)
        {
            return -3;
        }
        result = QnCompressedVideoDataPtr(new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, encoded, m_encoderCtx));
        return 0;
    }
    else {
        result = QnCompressedVideoDataPtr();
        return 0; // ignore decode error
    }
}

// -------------------------- QnFfmpegTranscoder --------------------------

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
m_videoEncoderCodecCtx(0),
m_audioEncoderCodecCtx(0),
m_videoBitrate(0),
m_startDateTime(0),
m_formatCtx(0),
m_dstByteArray(0),
m_ioContext(0)
{
}

QnFfmpegTranscoder::~QnFfmpegTranscoder()
{
    closeFfmpegContext();
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
        AVStream* videoStream = av_new_stream(m_formatCtx, 0);
        if (videoStream == 0)
        {
            m_lastErrMessage = tr("Can't allocate output stream for recording.");
            cl_log.log(m_lastErrMessage, cl_logERROR);
            return -1;
        }

        AVCodec* avCodec = avcodec_find_decoder(m_videoCodec);
        if (avCodec == 0)
        {
            m_lastErrMessage = tr("Transcoder error: can't find codec").arg(m_videoCodec);
            return -2;
        }
        videoStream->codec = m_videoEncoderCodecCtx = avcodec_alloc_context3(avCodec);
        m_videoEncoderCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
        m_videoEncoderCodecCtx->codec_id = m_videoCodec;
        m_videoEncoderCodecCtx->pix_fmt = PIX_FMT_YUV420P;

        if (m_vTranscoder)
        {
            if (m_vTranscoder->getCodecContext()) {
                avcodec_copy_context(m_videoEncoderCodecCtx, m_vTranscoder->getCodecContext());
            }
            else {
                m_videoEncoderCodecCtx->width = m_vTranscoder->getSize().width();
                m_videoEncoderCodecCtx->height = m_vTranscoder->getSize().height();
            }
        }
        else 
        {
            if (!video || video->width < 1 || video->height < 1)
            {
                m_lastErrMessage = tr("Transcoder error: for direct stream copy video frame size must exists");
                return -3;
            }

            if (video->context->ctx()) {
                avcodec_copy_context(m_videoEncoderCodecCtx, video->context->ctx());
            }
            else {
                m_videoEncoderCodecCtx->width = video->width;
                m_videoEncoderCodecCtx->height = video->height;
            }
        }
        m_videoEncoderCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
        m_videoEncoderCodecCtx->bit_rate = video->width * video->height*5; // auto fill bitrate. 10Mbit for full HD
        m_videoEncoderCodecCtx->time_base.num = 1;
        m_videoEncoderCodecCtx->time_base.den = 60;
        m_videoEncoderCodecCtx->gop_size = 30;
        m_videoEncoderCodecCtx->sample_aspect_ratio.num = 1;
        m_videoEncoderCodecCtx->sample_aspect_ratio.den = 1;

        videoStream->sample_aspect_ratio = m_videoEncoderCodecCtx->sample_aspect_ratio;
        videoStream->first_dts = 0;
    }

    if (m_audioCodec != CODEC_ID_NONE)
    {
        Q_ASSERT_X(true, Q_FUNC_INFO, "Not implemented! Under construction!!!");

        AVStream* audioStream = av_new_stream(m_formatCtx, 0);
        if (audioStream == 0)
        {
            m_lastErrMessage = tr("Can't allocate output stream for recording.");
            cl_log.log(m_lastErrMessage, cl_logERROR);
            return -1;
        }
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

    QnCompressedVideoDataPtr video = qSharedPointerDynamicCast<QnCompressedVideoData>(media);
    QnAbstractMediaDataPtr transcodedData;
    
    if (video)
    {
        if (m_vTranscoder)
        {
            // transcode video
            int errCode = m_vTranscoder->transcodePacket(media, transcodedData);
            if (errCode != 0)
                return errCode;
            packet.data = (quint8*) transcodedData->data.data();
            packet.size = transcodedData->data.size();
            if(transcodedData->flags & AV_PKT_FLAG_KEY)
                packet.flags |= AV_PKT_FLAG_KEY;
        }
        else {
            // direct stream copy
            packet.data = (quint8*) media->data.data();
            packet.size = media->data.size();
            if(media->flags & AV_PKT_FLAG_KEY)
                packet.flags |= AV_PKT_FLAG_KEY;
        }
    }
    else {
        Q_ASSERT_X(true, Q_FUNC_INFO, "Not implemented! Under cunstruction!!!");
    }
    
    if (av_write_frame(m_formatCtx, &packet) < 0) {
        qWarning() << QLatin1String("Transcoder error: can't write AV packet");
        //return -1; // ignore error and continue
    }
    return 0;
}
