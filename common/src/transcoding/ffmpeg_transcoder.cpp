#include "ffmpeg_transcoder.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QDebug>

#include <nx/utils/log/log.h>

#include "ffmpeg_video_transcoder.h"
#include "ffmpeg_audio_transcoder.h"

static const int IO_BLOCK_SIZE = 1024*16;

static qint32 ffmpegReadPacket(void *opaque, quint8* buf, int size)
{
    Q_UNUSED(opaque)
    Q_UNUSED(buf)
    Q_UNUSED(size)
    NX_ASSERT(false, Q_FUNC_INFO, "This class for streaming encoding! This function call MUST not exists!");
    return 0;
}

bool QnFfmpegTranscoder::isCodecSupported(AVCodecID id) const
{
    if (!m_formatCtx || !m_formatCtx->oformat)
        return false;
    return avformat_query_codec(m_formatCtx->oformat, id, FF_COMPLIANCE_NORMAL) == 1;
}

static qint32 ffmpegWritePacket(void *opaque, quint8* buf, int size)
{
    Q_UNUSED(opaque)

    QnFfmpegTranscoder* transcoder = reinterpret_cast<QnFfmpegTranscoder*> (opaque);
    if (!transcoder || transcoder->inMiddleOfStream())
        return size; // ignore write

    return transcoder->writeBuffer((char*) buf, size);
}

static int64_t ffmpegSeek(void* opaque, int64_t pos, int whence)
{
    Q_UNUSED(opaque)
    Q_UNUSED(pos)
    Q_UNUSED(whence)
    //NX_ASSERT(false, Q_FUNC_INFO, "This class for streaming encoding! This function call MUST not exists!");
    QnFfmpegTranscoder* transcoder = reinterpret_cast<QnFfmpegTranscoder*> (opaque);
    transcoder->setInMiddleOfStream(!(pos == 0 && whence == SEEK_END));
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

QnFfmpegTranscoder::QnFfmpegTranscoder()
:
    QnTranscoder(),
    m_videoEncoderCodecCtx(0),
    m_audioEncoderCodecCtx(0),
    m_videoBitrate(0),
    m_formatCtx(0),
    m_baseTime(AV_NOPTS_VALUE),
    m_inMiddleOfStream(false),
    m_startTimeOffset(0)
{
    NX_LOG( lit("Created new ffmpeg transcoder. Total transcoder count %1").
        arg(QnFfmpegTranscoder_count.fetchAndAddOrdered(1)+1), cl_logDEBUG1 );
}

QnFfmpegTranscoder::~QnFfmpegTranscoder()
{
    NX_LOG( lit("Destroying ffmpeg transcoder. Total transcoder count %1").
        arg(QnFfmpegTranscoder_count.fetchAndAddOrdered(-1)-1), cl_logDEBUG1 );
    closeFfmpegContext();
}

void QnFfmpegTranscoder::closeFfmpegContext()
{
    if (m_formatCtx)
    {
        if (m_formatCtx->pb)
            m_formatCtx->pb->opaque = 0;
        QnFfmpegHelper::closeFfmpegIOContext(m_formatCtx->pb);
        m_formatCtx->pb = nullptr;
        avformat_close_input(&m_formatCtx);
    }
}

int QnFfmpegTranscoder::setContainer(const QString& container)
{
    m_container = container;
    AVOutputFormat * outputCtx = av_guess_format(m_container.toLatin1().data(), NULL, NULL);
    if (outputCtx == 0)
    {
        m_lastErrMessage = tr("Container %1 was not found in FFMPEG library.").arg(container);
        qWarning() << m_lastErrMessage;
        return -1;
    }
    //outputCtx->flags |= AVFMT_VARIABLE_FPS;

    int err = avformat_alloc_output_context2(&m_formatCtx, outputCtx, 0, "");
    if (err != 0)
    {
        m_lastErrMessage = tr("Could not create output context for format %1.").arg(container);
        qWarning() << m_lastErrMessage;
        return -2;
    }
    if (container == QLatin1String("rtp"))
        m_formatCtx->packet_size = MTU_SIZE;


    return 0;
}

int QnFfmpegTranscoder::open(const QnConstCompressedVideoDataPtr& video, const QnConstCompressedAudioDataPtr& audio)
{
    if (video && m_videoCodec != AV_CODEC_ID_NONE)
    {
        AVStream* videoStream = avformat_new_stream(m_formatCtx, nullptr);
        if (videoStream == 0)
        {
            m_lastErrMessage = tr("Could not allocate output stream for recording.");
            NX_LOG(m_lastErrMessage, cl_logERROR);
            return -1;
        }

        videoStream->id = 0;
        //videoStream->codec = m_videoEncoderCodecCtx = avcodec_alloc_context3(0);
        m_videoEncoderCodecCtx = videoStream->codec;
        m_videoEncoderCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
        m_videoEncoderCodecCtx->codec_id = m_videoCodec;
        m_videoEncoderCodecCtx->pix_fmt = m_videoCodec == AV_CODEC_ID_MJPEG ? AV_PIX_FMT_YUVJ420P : AV_PIX_FMT_YUV420P;

        if (m_vTranscoder)
        {
            m_vTranscoder->open(video);
            if (!m_vTranscoder->getLastError().isEmpty())
                qWarning() << "Can't open video transcoder for RTSP streaming: " << m_vTranscoder->getLastError();

            QnFfmpegVideoTranscoderPtr ffmpegVideoTranscoder = m_vTranscoder.dynamicCast<QnFfmpegVideoTranscoder>();
            if (ffmpegVideoTranscoder->getCodecContext()) {

                QnFfmpegHelper::copyAvCodecContex(
                    m_videoEncoderCodecCtx,
                    ffmpegVideoTranscoder->getCodecContext());
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
                QnFfmpegVideoDecoder decoder(video->compressionType, video, false);
                QSharedPointer<CLVideoDecoderOutput> decodedVideoFrame( new CLVideoDecoderOutput() );
                decoder.decode(video, &decodedVideoFrame);
                videoWidth = decoder.getWidth();
                videoHeight = decoder.getHeight();
                if (videoWidth < 1 || videoHeight < 1)
                {
                    m_lastErrMessage = tr("Could not perform direct stream copy because frame size is undefined.");
                    closeFfmpegContext();
                    return -3;
                }
            }

            if (video->context)
                QnFfmpegHelper::mediaContextToAvCodecContext(m_videoEncoderCodecCtx, video->context);

            m_videoEncoderCodecCtx->width = videoWidth;
            m_videoEncoderCodecCtx->height = videoHeight;
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

    if (audio && m_aTranscoder && !m_aTranscoder->open(audio))
    {
        m_audioCodec = AV_CODEC_ID_NONE; // can't open transcoder. disable audio
    }

    if (audio && m_audioCodec != AV_CODEC_ID_NONE)
    {
        //NX_ASSERT(false, Q_FUNC_INFO, "Not implemented! Under construction!!!");

        AVStream* audioStream = avformat_new_stream(m_formatCtx, nullptr);
        if (audioStream == 0)
        {
            m_lastErrMessage = tr("Could not allocate output stream for recording.");
            NX_LOG(m_lastErrMessage, cl_logERROR);
            return -1;
        }

        audioStream->id = 0;
        AVCodec* avCodec = avcodec_find_decoder(m_audioCodec);
        if (avCodec == 0)
        {
            m_lastErrMessage = tr("Could not find codec %1.").arg(m_audioCodec);
            closeFfmpegContext();
            return -2;
        }
        audioStream->codec = m_audioEncoderCodecCtx = avcodec_alloc_context3(avCodec);
        m_audioEncoderCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
        m_audioEncoderCodecCtx->codec_id = m_audioCodec;

        if (m_aTranscoder)
        {
            QnFfmpegAudioTranscoderPtr ffmpegAudioTranscoder = m_aTranscoder.dynamicCast<QnFfmpegAudioTranscoder>();
            if (ffmpegAudioTranscoder->getCodecContext())
                QnFfmpegHelper::copyAvCodecContex(m_audioEncoderCodecCtx, ffmpegAudioTranscoder->getCodecContext());

            m_audioEncoderCodecCtx->bit_rate = m_aTranscoder->getBitrate();
        }
        else
        {
            if (audio->context)
                QnFfmpegHelper::mediaContextToAvCodecContext(m_audioEncoderCodecCtx, audio->context);

            //m_audioEncoderCodecCtx->bit_rate = 1024 * 96;
        }
        m_audioEncoderCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
        audioStream->first_dts = 0;
        //audioStream->time_base = m_audioEncoderCodecCtx->time_base;
    }
    if (m_formatCtx->nb_streams == 0)
        return -4;

    m_formatCtx->pb = createFfmpegIOContext();

    int rez = avformat_write_header(m_formatCtx, 0);
    if (rez < 0)
    {
        closeFfmpegContext();
        m_lastErrMessage = tr("Video or audio codec is incompatible with container %1.").arg(m_container);
        NX_LOG(m_lastErrMessage, cl_logERROR);
        return -3;
    }

    if (video)
        m_initializedVideo = true;
    else
        m_vTranscoder.reset();

    if (audio)
        m_initializedAudio = true;
    else
        m_aTranscoder.reset();

    m_initialized = true;
    return 0;
}

bool QnFfmpegTranscoder::addTag( const QString& name, const QString& value )
{
    return av_dict_set( &m_formatCtx->metadata, name.toUtf8().constData(), value.toUtf8().constData(), 0 ) >= 0;
}

int QnFfmpegTranscoder::transcodePacketInternal(const QnConstAbstractMediaDataPtr& media, QnByteArray* const result)
{
    Q_UNUSED(result)
    if (m_baseTime == AV_NOPTS_VALUE)
        m_baseTime = media->timestamp - m_startTimeOffset;

    if (m_audioCodec == AV_CODEC_ID_NONE && media->dataType == QnAbstractMediaData::AUDIO)
        return 0;
    else if (m_videoCodec == AV_CODEC_ID_NONE && media->dataType == QnAbstractMediaData::VIDEO)
        return 0;

    AVRational srcRate = {1, 1000000};
    int streamIndex = 0;
    if (m_vTranscoder && m_aTranscoder && media->dataType == QnAbstractMediaData::AUDIO)
        streamIndex = 1;

    AVStream* stream = m_formatCtx->streams[streamIndex];
    QnFfmpegAvPacket packet;
    QnAbstractMediaDataPtr transcodedData;

    QnCodecTranscoderPtr transcoder;
    if (dynamic_cast<const QnCompressedVideoData*>(media.get()))
        transcoder = m_vTranscoder;
    else
        transcoder = m_aTranscoder;

    bool doTranscoding = true;
    do {
        packet.data = 0;
        packet.size = 0;

        if (transcoder)
        {
            // transcode media
            int errCode = transcoder->transcodePacket(doTranscoding ? media : QnConstAbstractMediaDataPtr(), result ? &transcodedData : NULL);
            if (errCode != 0)
                return errCode;
            if (transcodedData) {
                packet.data = const_cast<quint8*>((const quint8*) transcodedData->data());  //const_cast is here because av_write_frame accepts
                                                                                            //non-const pointer, but does not modifiy packet buffer
                packet.size = static_cast<int>(transcodedData->dataSize());
                packet.pts = av_rescale_q(transcodedData->timestamp - m_baseTime, srcRate, stream->time_base);
                if(transcodedData->flags & AV_PKT_FLAG_KEY)
                    packet.flags |= AV_PKT_FLAG_KEY;
            }
        }
        else {
            // direct stream copy
            packet.pts = av_rescale_q(media->timestamp - m_baseTime, srcRate, stream->time_base);
            packet.data = const_cast<quint8*>((const quint8*) media->data());
            packet.size = static_cast<int>(media->dataSize());
            if((media->dataType == QnAbstractMediaData::AUDIO) || (media->flags & AV_PKT_FLAG_KEY))
                packet.flags |= AV_PKT_FLAG_KEY;
        }
        packet.stream_index = streamIndex;
        packet.dts = packet.pts;

        if (packet.size > 0)
        {
            if (av_write_frame(m_formatCtx, &packet) < 0) {
                qWarning() << QLatin1String("Transcoder error: can't write AV packet");
                //return -1; // ignore error and continue
            }
        }
        doTranscoding = false;
    } while (transcoder && transcoder->existMoreData());
    return 0;
}

int QnFfmpegTranscoder::finalizeInternal(QnByteArray* const /*result*/)
{
    if (!m_vTranscoder && !m_aTranscoder)
        return 0;

    for (int streamIndex = 0; streamIndex < 2; ++streamIndex)
    {
        AVRational srcRate = { 1, 1000000 };

        //finalizing codec transcoder
        QnCodecTranscoderPtr transcoder;
        if (streamIndex == 0)
            transcoder = m_vTranscoder;
        else if (streamIndex == 1)
            transcoder = m_aTranscoder;
        if (!transcoder)
            continue;

        AVStream* stream = m_formatCtx->streams[streamIndex];

        QnAbstractMediaDataPtr transcodedData;
        do
        {
            QnFfmpegAvPacket packet;
            transcodedData.reset();

            // transcode media
            int errCode = transcoder->transcodePacket(QnConstAbstractMediaDataPtr(), &transcodedData);
            if (errCode != 0)
                return errCode;
            if (transcodedData)
            {
                packet.data = const_cast<quint8*>((const quint8*)transcodedData->data());  //const_cast is here because av_write_frame accepts
                                                                                            //non-const pointer, but does not modifiy packet buffer
                packet.size = static_cast<int>(transcodedData->dataSize());
                packet.pts = av_rescale_q(transcodedData->timestamp - m_baseTime, srcRate, stream->time_base);
                if (transcodedData->flags & AV_PKT_FLAG_KEY)
                    packet.flags |= AV_PKT_FLAG_KEY;
            }

            packet.stream_index = streamIndex;
            packet.dts = packet.pts;

            if (packet.size > 0)
            {
                if (av_write_frame(m_formatCtx, &packet) < 0)
                    qWarning() << QLatin1String("QnFfmpegTranscoder::finalizeIntenal. Transcoder error: can't write AV packet");
            }
        } while (transcodedData);
    }

    //finalizing container stream
    av_write_trailer(m_formatCtx);

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

#endif // ENABLE_DATA_PROVIDERS
