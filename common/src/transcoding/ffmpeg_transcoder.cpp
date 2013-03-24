#include "ffmpeg_transcoder.h"
#include <QDebug>
#include "ffmpeg_video_transcoder.h"
#include "ffmpeg_audio_transcoder.h"

//extern QMutex global_ffmpeg_mutex;
static const int IO_BLOCK_SIZE = 1024*16;

static qint32 ffmpegReadPacket(void *opaque, quint8* buf, int size)
{
    Q_UNUSED(opaque)
    Q_UNUSED(buf)
    Q_UNUSED(size)
    Q_ASSERT_X(false, Q_FUNC_INFO, "This class for streaming encoding! This function call MUST not exists!");
    return 0;
}

static qint32 ffmpegWritePacket(void *opaque, quint8* buf, int size)
{
    Q_UNUSED(opaque)
    QnFfmpegTranscoder* transcoder = reinterpret_cast<QnFfmpegTranscoder*> (opaque);
    return transcoder->writeBuffer((char*) buf, size);
}

static int64_t ffmpegSeek(void* opaque, int64_t pos, int whence)
{
    Q_UNUSED(opaque)
    Q_UNUSED(pos)
    Q_UNUSED(whence)
    Q_ASSERT_X(false, Q_FUNC_INFO, "This class for streaming encoding! This function call MUST not exists!");
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
    ffmpegIOContext->seekable = 0;
    return ffmpegIOContext;
}

static QAtomicInt QnFfmpegTranscoder_count = 0;

QnFfmpegTranscoder::QnFfmpegTranscoder():
QnTranscoder(),
m_videoEncoderCodecCtx(0),
m_audioEncoderCodecCtx(0),
m_videoBitrate(0),
m_formatCtx(0),
m_ioContext(0),
m_baseTime(AV_NOPTS_VALUE)
{
    NX_LOG( QString::fromLatin1("Created new ffmpeg transcoder. Total transcoder count %1").
        arg(QnFfmpegTranscoder_count.fetchAndAddOrdered(1)+1), cl_logDEBUG1 );
}

QnFfmpegTranscoder::~QnFfmpegTranscoder()
{
    NX_LOG( QString::fromLatin1("Destroying ffmpeg transcoder. Total transcoder count %1").
        arg(QnFfmpegTranscoder_count.fetchAndAddOrdered(-1)-1), cl_logDEBUG1 );
    closeFfmpegContext();
}

void QnFfmpegTranscoder::closeFfmpegContext()
{
    //QMutexLocker mutex(&global_ffmpeg_mutex);
    if (m_formatCtx)
    {
        for (unsigned i = 0; i < m_formatCtx->nb_streams; ++i)
        {
            if (m_formatCtx->streams[i]->codec->codec)
                avcodec_close(m_formatCtx->streams[i]->codec);
        }
    }
    if (m_ioContext)
    {
        //m_ioContext->opaque = 0;
        //avio_close(m_ioContext);
        av_free(m_ioContext->buffer);
        av_free(m_ioContext);
        m_ioContext = 0;
        if (m_formatCtx)
            m_formatCtx->pb = 0;
    }

    if (m_formatCtx) 
        avformat_close_input(&m_formatCtx);
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

    //global_ffmpeg_mutex.lock();
    int err = avformat_alloc_output_context2(&m_formatCtx, outputCtx, 0, "");
    //global_ffmpeg_mutex.unlock();
    if (err != 0)
    {
        m_lastErrMessage = tr("Can't create output context for format %1").arg(container);
        qWarning() << m_lastErrMessage;
        return -2;
    }
    if (container == QLatin1String("rtp"))
        m_formatCtx->packet_size = MTU_SIZE;


    return 0;
}

int QnFfmpegTranscoder::open(QnCompressedVideoDataPtr video, QnCompressedAudioDataPtr audio)
{
    //QMutexLocker mutex(&global_ffmpeg_mutex);

    if (m_videoCodec != CODEC_ID_NONE)
    {
        // TODO: #vasilenko avoid using deprecated methods
        AVStream* videoStream = av_new_stream(m_formatCtx, 0);
        if (videoStream == 0)
        {
            m_lastErrMessage = tr("Can't allocate output stream for recording.");
            cl_log.log(m_lastErrMessage, cl_logERROR);
            return -1;
        }

        videoStream->codec = m_videoEncoderCodecCtx = avcodec_alloc_context3(0);
        m_videoEncoderCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
        m_videoEncoderCodecCtx->codec_id = m_videoCodec;
        m_videoEncoderCodecCtx->pix_fmt = m_videoCodec == CODEC_ID_MJPEG ? PIX_FMT_YUVJ420P : PIX_FMT_YUV420P;

        if (m_vTranscoder)
        {
            m_vTranscoder->open(video);
            if (!m_vTranscoder->getLastError().isEmpty())
                qWarning() << "Can't open video transcoder for RTSP streaming: " << m_vTranscoder->getLastError();

            QnFfmpegVideoTranscoderPtr ffmpegVideoTranscoder = m_vTranscoder.dynamicCast<QnFfmpegVideoTranscoder>();
            if (ffmpegVideoTranscoder->getCodecContext()) {
                avcodec_copy_context(m_videoEncoderCodecCtx, ffmpegVideoTranscoder->getCodecContext());
            }
            else {
                m_videoEncoderCodecCtx->width = m_vTranscoder->getResolution().width();
                m_videoEncoderCodecCtx->height = m_vTranscoder->getResolution().height();
            }
            m_videoEncoderCodecCtx->bit_rate = m_vTranscoder->getBitrate();
        }
        else 
        {
            int videoWidth = video->width;
            int videoHeight = video->height;
            if (!video || video->width < 1 || video->height < 1)
            {
                CLFFmpegVideoDecoder decoder(video->compressionType, video, false);
                QSharedPointer<CLVideoDecoderOutput> decodedVideoFrame( new CLVideoDecoderOutput() );
                decoder.decode(video, &decodedVideoFrame);
                videoWidth = decoder.getWidth();
                videoHeight = decoder.getHeight();
                if (videoWidth < 1 || videoHeight < 1)
                {
                    m_lastErrMessage = tr("Transcoder error: for direct stream copy video frame size must exists");
                    return -3;
                }
            }

            if (video->context && video->context->ctx()) {
                avcodec_copy_context(m_videoEncoderCodecCtx, video->context->ctx());
            }
            else {
                m_videoEncoderCodecCtx->width = videoWidth;
                m_videoEncoderCodecCtx->height = videoHeight;
            }
            m_videoEncoderCodecCtx->bit_rate = videoWidth * videoHeight; // auto fill bitrate. 2Mbit for full HD, 1Mbit for 720x768
        }
        m_videoEncoderCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
        m_videoEncoderCodecCtx->time_base.num = 1;
        m_videoEncoderCodecCtx->time_base.den = 60;
        m_videoEncoderCodecCtx->gop_size = 30;
        m_videoEncoderCodecCtx->sample_aspect_ratio.num = 1;
        m_videoEncoderCodecCtx->sample_aspect_ratio.den = 1;

        videoStream->sample_aspect_ratio = m_videoEncoderCodecCtx->sample_aspect_ratio;
        videoStream->first_dts = 0;
    }

    if (m_aTranscoder && !m_aTranscoder->open(audio))
    {
        m_audioCodec = CODEC_ID_NONE; // can't open transcoder. disable audio
    }

    if (m_audioCodec != CODEC_ID_NONE)
    {
        //Q_ASSERT_X(false, Q_FUNC_INFO, "Not implemented! Under construction!!!");

        // TODO: #vasilenko avoid using deprecated methods
        AVStream* audioStream = av_new_stream(m_formatCtx, 0);
        if (audioStream == 0)
        {
            m_lastErrMessage = tr("Can't allocate output stream for recording.");
            cl_log.log(m_lastErrMessage, cl_logERROR);
            return -1;
        }

        AVCodec* avCodec = avcodec_find_decoder(m_audioCodec);
        if (avCodec == 0)
        {
            m_lastErrMessage = tr("Transcoder error: can't find codec").arg(m_audioCodec);
            return -2;
        }
        audioStream->codec = m_audioEncoderCodecCtx = avcodec_alloc_context3(avCodec);
        m_audioEncoderCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
        m_audioEncoderCodecCtx->codec_id = m_audioCodec;

        if (m_aTranscoder)
        {
            QnFfmpegAudioTranscoderPtr ffmpegAudioTranscoder = m_aTranscoder.dynamicCast<QnFfmpegAudioTranscoder>();
            if (ffmpegAudioTranscoder->getCodecContext()) {
                avcodec_copy_context(m_audioEncoderCodecCtx, ffmpegAudioTranscoder->getCodecContext());
            }
            m_audioEncoderCodecCtx->bit_rate = m_aTranscoder->getBitrate();
        }
        else 
        {
            if (audio->context && audio->context->ctx()) {
                avcodec_copy_context(m_audioEncoderCodecCtx, audio->context->ctx());
            }
            //m_audioEncoderCodecCtx->bit_rate = 1024 * 96;
        }
        m_audioEncoderCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
        audioStream->first_dts = 0;
        //audioStream->time_base = m_audioEncoderCodecCtx->time_base;
    }
    if (m_formatCtx->nb_streams == 0)
        return -4;

    m_formatCtx->pb = m_ioContext = createFfmpegIOContext();

    int rez = avformat_write_header(m_formatCtx, 0);
    if (rez < 0) 
    {
        closeFfmpegContext();
        m_lastErrMessage = tr("Video or audio codec is incompatible with %1 format. Try another format.").arg(m_container);
        cl_log.log(m_lastErrMessage, cl_logERROR);
        return -3;
    }
    m_initialized = true;
    return 0;
}

int QnFfmpegTranscoder::transcodePacketInternal(QnAbstractMediaDataPtr media, QnByteArray* const result)
{
    Q_UNUSED(result)
    if ((quint64)m_baseTime == AV_NOPTS_VALUE)
        m_baseTime = media->timestamp - 1000*100;

    if (m_audioCodec == CODEC_ID_NONE && media->dataType == QnAbstractMediaData::AUDIO)
        return 0;

    AVRational srcRate = {1, 1000000};
    int streamIndex = 0;
    if (m_vTranscoder && m_aTranscoder && media->dataType == QnAbstractMediaData::AUDIO)
        streamIndex = 1;

    AVStream* stream = m_formatCtx->streams[streamIndex];
    AVPacket packet;
    av_init_packet(&packet);

    QnCompressedVideoDataPtr video = qSharedPointerDynamicCast<QnCompressedVideoData>(media);
    QnAbstractMediaDataPtr transcodedData;
    

    QnCodecTranscoderPtr transcoder;
    if (video)
        transcoder = m_vTranscoder;
    else
        transcoder = m_aTranscoder;

    do {
        packet.data = 0;
        packet.size = 0;

        if (transcoder)
        {
            // transcode media
            int errCode = transcoder->transcodePacket(media, result ? &transcodedData : NULL);
            if (errCode != 0)
                return errCode;
            if (transcodedData) {
                packet.data = (quint8*) transcodedData->data.data();
                packet.size = transcodedData->data.size();
                packet.pts = av_rescale_q(transcodedData->timestamp - m_baseTime, srcRate, stream->time_base);
                if(transcodedData->flags & AV_PKT_FLAG_KEY)
                    packet.flags |= AV_PKT_FLAG_KEY;
            }
        }
        else {
            // direct stream copy
            packet.pts = av_rescale_q(media->timestamp - m_baseTime, srcRate, stream->time_base);
            packet.data = (quint8*) media->data.data();
            packet.size = media->data.size();
            if((media->dataType == QnAbstractMediaData::AUDIO) || (media->flags & AV_PKT_FLAG_KEY))
                packet.flags |= AV_PKT_FLAG_KEY;
        }
        packet.stream_index = streamIndex;
        packet.dts = packet.pts;
        
        if (packet.size > 0)
        {
            //qDebug() << "packet.pts=" << packet.pts;

            if (av_write_frame(m_formatCtx, &packet) < 0) {
                qWarning() << QLatin1String("Transcoder error: can't write AV packet");
                //return -1; // ignore error and continue
            }
        }
        media.clear();
    } while (transcoder && packet.size > 0);
    return 0;
}

AVCodecContext* QnFfmpegTranscoder::getVideoCodecContext() const 
{ 
    /*
    QnFfmpegVideoTranscoderPtr ffmpegVideoTranscoder = m_vTranscoder.dynamicCast<QnFfmpegVideoTranscoder>();
    if (ffmpegVideoTranscoder)
        return ffmpegVideoTranscoder->getCodecContext();
    else
    */
        return m_videoEncoderCodecCtx; 
}

AVCodecContext* QnFfmpegTranscoder::getAudioCodecContext() const 
{ 
    return m_audioEncoderCodecCtx; 
}
