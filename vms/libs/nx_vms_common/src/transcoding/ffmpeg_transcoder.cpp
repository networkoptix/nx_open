// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ffmpeg_transcoder.h"

extern "C"
{
#include <libavutil/opt.h>
}

#include <QtCore/QDebug>

#include <nx/utils/log/log.h>
#include <utils/media/jpeg_utils.h>
#include <utils/media/utils.h>

#include "ffmpeg_video_transcoder.h"
#include "ffmpeg_audio_transcoder.h"

static const int IO_BLOCK_SIZE = 1024*16;

using namespace nx::vms::api;

static AVPixelFormat jpegPixelFormatToRtp(AVPixelFormat value)
{
    switch (value)
    {
        case AV_PIX_FMT_YUV420P: return AV_PIX_FMT_YUVJ420P;
        case AV_PIX_FMT_YUV422P: return AV_PIX_FMT_YUVJ422P;
        case AV_PIX_FMT_YUV444P: return AV_PIX_FMT_YUVJ444P;
        default: return value;
    }
}

static qint32 ffmpegReadPacket(void* /*opaque*/, quint8* /*buf*/, int /*size*/)
{
    NX_ASSERT(false, "This class for streaming encoding! This function call MUST not exists!");
    return 0;
}

bool QnFfmpegTranscoder::isCodecSupported(AVCodecID id) const
{
    if (!m_formatCtx || !m_formatCtx->oformat)
        return false;
    return avformat_query_codec(m_formatCtx->oformat, id, FF_COMPLIANCE_NORMAL) == 1;
}

static qint32 ffmpegWritePacket(void* opaque, quint8* buf, int size)
{
    QnFfmpegTranscoder* transcoder = reinterpret_cast<QnFfmpegTranscoder*> (opaque);
    if (!transcoder || transcoder->inMiddleOfStream())
        return size; // ignore write

    return transcoder->writeBuffer((char*) buf, size);
}

static int64_t ffmpegSeek(void* opaque, int64_t pos, int whence)
{
    //NX_ASSERT(false, "This class for streaming encoding! This function call MUST not exists!");
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

QnFfmpegTranscoder::QnFfmpegTranscoder(const Config& config, nx::metrics::Storage* metrics)
:
    QnTranscoder(config.decoderConfig, metrics),
    m_config(config)
{
    NX_DEBUG(this, "Created new ffmpeg transcoder. Total transcoder count %1",
        QnFfmpegTranscoder_count.fetchAndAddOrdered(1) + 1);
}

QnFfmpegTranscoder::~QnFfmpegTranscoder()
{
    NX_DEBUG(this, "Destroying ffmpeg transcoder. Total transcoder count %1",
        QnFfmpegTranscoder_count.fetchAndAddOrdered(-1) - 1);
    closeFfmpegContext();
}

void QnFfmpegTranscoder::closeFfmpegContext()
{
    if (m_formatCtx)
    {
        if (m_initialized)
            av_write_trailer(m_formatCtx);
        if (m_formatCtx->pb)
            m_formatCtx->pb->opaque = 0;
        QnFfmpegHelper::closeFfmpegIOContext(m_formatCtx->pb);
        m_formatCtx->pb = nullptr;
        avformat_close_input(&m_formatCtx);
    }
}

void QnFfmpegTranscoder::setSourceResolution(const QSize& resolution)
{
    m_sourceResolution = resolution;
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
        m_formatCtx->packet_size = m_rtpMtu;

    return 0;
}

void QnFfmpegTranscoder::setFormatOption(const QString& option, const QString& value)
{
    av_opt_set(m_formatCtx->priv_data, option.toLatin1().data(), value.toLatin1().data(), 0);
}

AVPixelFormat QnFfmpegTranscoder::getPixelFormatJpeg(const QnConstCompressedVideoDataPtr& video)
{
    nx_jpg::ImageInfo info;
    if (!nx_jpg::readJpegImageInfo((const uint8_t*)video->data(), video->dataSize(), &info) ||
        info.pixelFormat == AV_PIX_FMT_NONE)
    {
        NX_DEBUG(this, "Failed to parse MJPEG header");
        return AV_PIX_FMT_YUV420P;
    }
    if (m_container == "rtp")
        return jpegPixelFormatToRtp(info.pixelFormat);
    else
        return info.pixelFormat;
}

int QnFfmpegTranscoder::open(const QnConstCompressedVideoDataPtr& video, const QnConstCompressedAudioDataPtr& audio)
{
    if (!m_formatCtx)
        setContainer(m_container);

    if (video && m_videoCodec != AV_CODEC_ID_NONE)
    {
        AVStream* videoStream = avformat_new_stream(m_formatCtx, nullptr);
        if (videoStream == 0)
        {
            m_lastErrMessage = tr("Could not allocate output stream for recording.");
            NX_ERROR(this, m_lastErrMessage);
            return -1;
        }

        videoStream->id = 0;
        m_videoCodecParameters = videoStream->codecpar;
        m_videoCodecParameters->codec_type = AVMEDIA_TYPE_VIDEO;
        m_videoCodecParameters->codec_id = m_videoCodec;
        if (m_vTranscoder)
        {
            m_vTranscoder->setSourceResolution(m_sourceResolution);
            if (!m_vTranscoder->open(video))
            {
                m_vTranscoder->getLastError().isEmpty();
                NX_WARNING(this, "Can't open video transcoder for RTSP streaming: [%1]",
                    m_vTranscoder->getLastError());
                return -1;
            }

            QnFfmpegVideoTranscoderPtr ffmpegVideoTranscoder = m_vTranscoder.dynamicCast<QnFfmpegVideoTranscoder>();
            if (ffmpegVideoTranscoder->getCodecContext())
            {
                avcodec_parameters_from_context(
                    m_videoCodecParameters, ffmpegVideoTranscoder->getCodecContext());
            }
            else
            {
                m_videoCodecParameters->width = m_vTranscoder->getOutputResolution().width();
                m_videoCodecParameters->height = m_vTranscoder->getOutputResolution().height();
            }
            m_videoCodecParameters->bit_rate = m_vTranscoder->getBitrate();
        }
        else
        {
            int videoWidth = video->width;
            int videoHeight = video->height;
            if (!video || video->width < 1 || video->height < 1)
            {
                QnFfmpegVideoDecoder decoder(
                    m_config.decoderConfig,
                    m_metrics,
                    video);
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
                avcodec_parameters_copy(m_videoCodecParameters, video->context->getAvCodecParameters());

            if (m_container.compare("mp4", Qt::CaseInsensitive) == 0
                ||  m_container.compare("ismv", Qt::CaseInsensitive) == 0)
            {
                if (!nx::media::fillExtraData(video.get(), &m_videoCodecParameters->extradata, &m_videoCodecParameters->extradata_size))
                    NX_WARNING(this, "Failed to build extra data");
            }

            m_videoCodecParameters->width = videoWidth;
            m_videoCodecParameters->height = videoHeight;

            // Auto-fill bitrate: 2Mbit for full HD, 1Mbit for 720x768.
            m_videoCodecParameters->bit_rate = videoWidth * videoHeight;

        }
        if (m_videoCodec == AV_CODEC_ID_MJPEG && !m_vTranscoder)
            m_videoCodecParameters->format = getPixelFormatJpeg(video);

        m_videoCodecParameters->sample_aspect_ratio.num = 1;
        m_videoCodecParameters->sample_aspect_ratio.den = 1;

        videoStream->time_base.num = 1;
        videoStream->time_base.den = 60;
        videoStream->sample_aspect_ratio = m_videoCodecParameters->sample_aspect_ratio;
        videoStream->first_dts = 0;
    }

    if (audio)
    {
        if (m_audioCodec == AV_CODEC_ID_PROBE)
        {
            m_audioCodec = AV_CODEC_ID_NONE;
            if (audio->context && isCodecSupported(audio->context->getCodecId()))
            {
                setAudioCodec(audio->context->getCodecId(), TranscodeMethod::TM_DirectStreamCopy);
            }
            else
            {
                static const std::vector<AVCodecID> audioCodecs =
                    {AV_CODEC_ID_MP3, AV_CODEC_ID_VORBIS}; //< Audio codecs to transcode.
                for (const auto& codec: audioCodecs)
                {
                    if (isCodecSupported(codec))
                    {
                        setAudioCodec(codec, TranscodeMethod::TM_FfmpegTranscode);
                        break;
                    }
                }
            }
            NX_DEBUG(this, "Auto select audio codec %1 for format %2",
                m_audioCodec, m_container);
        }

        if (m_aTranscoder && !m_aTranscoder->open(audio))
            m_audioCodec = AV_CODEC_ID_NONE; // can't open transcoder. disable audio
    }

    if (audio && m_audioCodec != AV_CODEC_ID_NONE)
    {
        AVStream* audioStream = avformat_new_stream(m_formatCtx, nullptr);
        if (audioStream == 0)
        {
            m_lastErrMessage = tr("Could not allocate output stream for recording.");
            NX_ERROR(this, m_lastErrMessage);
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
        m_audioCodecParameters = audioStream->codecpar;
        m_audioCodecParameters->codec_type = AVMEDIA_TYPE_AUDIO;
        m_audioCodecParameters->codec_id = m_audioCodec;

        if (m_aTranscoder)
        {
            QnFfmpegAudioTranscoderPtr ffmpegAudioTranscoder = m_aTranscoder.dynamicCast<QnFfmpegAudioTranscoder>();
            if (ffmpegAudioTranscoder->getCodecContext())
                avcodec_parameters_from_context(m_audioCodecParameters, ffmpegAudioTranscoder->getCodecContext());

            m_audioCodecParameters->bit_rate = m_aTranscoder->getBitrate();
        }
        else
        {
            if (audio->context)
                avcodec_parameters_copy(m_audioCodecParameters, audio->context->getAvCodecParameters());
        }
        audioStream->first_dts = 0;
    }
    if (m_formatCtx->nb_streams == 0)
        return -4;

    m_formatCtx->pb = createFfmpegIOContext();

    int rez = avformat_write_header(m_formatCtx, 0);
    if (rez < 0)
    {
        closeFfmpegContext();
        m_lastErrMessage = tr("Video or audio codec is incompatible with container %1.").arg(m_container);
        NX_ERROR(this, m_lastErrMessage);
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

int QnFfmpegTranscoder::muxPacket(const QnConstAbstractMediaDataPtr& mediaPacket)
{
    int streamIndex = 0;
    if (m_videoCodec != AV_CODEC_ID_NONE && mediaPacket->dataType == QnAbstractMediaData::AUDIO)
       streamIndex = 1;

    if (streamIndex >= (int)m_formatCtx->nb_streams)
    {
        NX_DEBUG(this, "Invalid packet media type: %1, skip it", mediaPacket->dataType);
        return 0;
    }

    AVStream* stream = m_formatCtx->streams[streamIndex];
    AVRational srcRate = {1, 1000000};
    QnFfmpegAvPacket packet;

    if (m_config.useAbsoluteTimestamp)
        packet.pts = av_rescale_q(mediaPacket->timestamp, srcRate, stream->time_base);
    else
       packet.pts = av_rescale_q(mediaPacket->timestamp - m_baseTime, srcRate, stream->time_base);

    packet.data = (uint8_t*)mediaPacket->data();
    packet.size = static_cast<int>(mediaPacket->dataSize());
    if (mediaPacket->dataType == QnAbstractMediaData::AUDIO || mediaPacket->flags & AV_PKT_FLAG_KEY)
        packet.flags |= AV_PKT_FLAG_KEY;

    packet.stream_index = streamIndex;
    packet.dts = packet.pts;
    m_lastPacketTimestamp.ntpTimestamp = mediaPacket->timestamp;
    m_lastPacketTimestamp.rtpTimestamp = packet.pts;

    int status = av_write_frame(m_formatCtx, &packet);
    if (status < 0)
    {
        NX_WARNING(this, "Muxing packet error: can't write AV packet, error: %1",
            QnFfmpegHelper::avErrorToString(status));
        return status;
    }

    if (m_config.computeSignature)
    {
        auto context = mediaPacket->dataType == QnAbstractMediaData::VIDEO ?
            m_videoCodecParameters : m_audioCodecParameters;
        m_mediaSigner.processMedia(context, packet.data, packet.size);
    }
    return 0;
}

int QnFfmpegTranscoder::transcodePacketInternal(
    const QnConstAbstractMediaDataPtr& media, QnByteArray* const result)
{
    if (m_baseTime == AV_NOPTS_VALUE)
        m_baseTime = media->timestamp - m_startTimeOffset;

    if (m_audioCodec == AV_CODEC_ID_NONE && media->dataType == QnAbstractMediaData::AUDIO)
        return 0;
    else if (m_videoCodec == AV_CODEC_ID_NONE && media->dataType == QnAbstractMediaData::VIDEO)
        return 0;

    QnCodecTranscoderPtr transcoder;
    if (dynamic_cast<const QnCompressedVideoData*>(media.get()))
        transcoder = m_vTranscoder;
    else
        transcoder = m_aTranscoder;

    bool doTranscoding = true;
    do {
        QnConstAbstractMediaDataPtr mediaPacket;
        if (transcoder)
        {
            // transcode media
            QnAbstractMediaDataPtr transcodedPacket;
            int errCode = transcoder->transcodePacket(doTranscoding ? media : QnConstAbstractMediaDataPtr(), result ? &transcodedPacket : NULL);
            if (errCode != 0)
            {
                NX_DEBUG(this, "Transcoding error: %1", QnFfmpegHelper::avErrorToString(errCode));
                return errCode;
            }
            mediaPacket = transcodedPacket;
        }
        else
        {
            // direct stream copy
            mediaPacket = media;
        }
        if (mediaPacket && mediaPacket->dataSize() > 0)
            muxPacket(mediaPacket);

        doTranscoding = false;
    } while (transcoder && transcoder->existMoreData());
    return 0;
}

int QnFfmpegTranscoder::finalizeInternal(QnByteArray* const /*result*/)
{
    for (int streamIndex = 0; streamIndex < 2; ++streamIndex)
    {
        //finalizing codec transcoder
        QnCodecTranscoderPtr transcoder;
        if (streamIndex == 0)
            transcoder = m_vTranscoder;
        else if (streamIndex == 1)
            transcoder = m_aTranscoder;
        if (!transcoder)
            continue;

        QnAbstractMediaDataPtr transcodedData;
        do
        {
            transcodedData.reset();

            // transcode media
            int errCode = transcoder->transcodePacket(QnConstAbstractMediaDataPtr(), &transcodedData);
            if (errCode != 0)
                return errCode;
            if (transcodedData && transcodedData->dataSize() > 0)
                muxPacket(QnConstAbstractMediaDataPtr(transcodedData));

        } while (transcodedData);
    }

    closeFfmpegContext();
    return 0;
}

QByteArray QnFfmpegTranscoder::getSignature(QnLicensePool* licensePool, const QnUuid& serverId)
{
    return m_mediaSigner.buildSignature(licensePool, serverId);
}

AVCodecParameters* QnFfmpegTranscoder::getVideoCodecParameters() const
{
    return m_videoCodecParameters;
}

AVCodecParameters* QnFfmpegTranscoder::getAudioCodecParameters() const
{
    return m_audioCodecParameters;
}
