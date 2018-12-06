#ifdef ENABLE_DATA_PROVIDERS

#include "ffmpeg_audio_transcoder.h"

#include <nx/streaming/audio_data_packet.h>
#include <nx/streaming/av_codec_media_context.h>
#include <nx/streaming/config.h>
#include <nx/utils/log/assert.h>

namespace
{
    const std::chrono::microseconds kMaxAudioJitterUs = std::chrono::milliseconds(200);
    const int kDefaultFrameSize = 1024;

    int calcBits(quint64 value)
    {
        int result = 0;
        while (value)
        {
            if (value & 1)
                ++result;
            value >>= 1;
        }
        return result;
    }

    int getMaxAudioChannels(AVCodec* avCodec)
    {
        if (!avCodec->channel_layouts)
            return 1; // default value if unknown

        int result = 0;
        for (const uint64_t* layout = avCodec->channel_layouts; *layout; ++layout)
            result = qMax(result, calcBits(*layout));
        return result;
    }

    int getDefaultDstSampleRate(int srcSampleRate, AVCodec* avCodec)
    {
        int result = srcSampleRate;
        bool isPcmCodec = avCodec->id == AV_CODEC_ID_ADPCM_G726
            || avCodec->id == AV_CODEC_ID_PCM_MULAW
            || avCodec->id == AV_CODEC_ID_PCM_ALAW;

        if (isPcmCodec)
            result = 8000;
        else
            result = std::max(result, 16000);

        return result;
    }
}

QnFfmpegAudioTranscoder::QnFfmpegAudioTranscoder(AVCodecID codecId):
    QnAudioTranscoder(codecId),
    m_encoderCtx(nullptr),
    m_decoderCtx(nullptr),

    m_firstEncodedPts(AV_NOPTS_VALUE),
    m_lastTimestamp(AV_NOPTS_VALUE),
    m_encodedDuration(0),

    m_dstSampleRate(0),
    m_frameDecodeTo(av_frame_alloc()),
    m_isOpened(false)
{
}

QnFfmpegAudioTranscoder::~QnFfmpegAudioTranscoder()
{

    QnFfmpegHelper::deleteAvCodecContext(m_encoderCtx);
    QnFfmpegHelper::deleteAvCodecContext(m_decoderCtx);

    av_frame_free(&m_frameDecodeTo);
}

bool QnFfmpegAudioTranscoder::open(const QnConstCompressedAudioDataPtr& audio)
{
    if (!audio->context)
    {
        m_lastErrMessage = tr("Audio context was not specified.");
        return false;
    }

    QnAudioTranscoder::open(audio);

    return open(audio->context);
}

int getDefaultBitrate(AVCodecContext* context)
{
    if (context->codec_id == AV_CODEC_ID_ADPCM_G726)
        return 16000; // G726 supports bitrate in range [16000..40000] Kbps only.
    return 64000 * context->channels;
}

bool QnFfmpegAudioTranscoder::open(const QnConstMediaContextPtr& context)
{
    NX_ASSERT(context);

    AVCodec* avCodec = avcodec_find_encoder(m_codecId);
    if (!avCodec)
    {
        m_lastErrMessage = tr("Could not find encoder for codec %1.").arg(m_codecId);
        return false;
    }

    m_encoderCtx = avcodec_alloc_context3(avCodec);
    m_encoderCtx->sample_fmt = avCodec->sample_fmts[0] != AV_SAMPLE_FMT_NONE ? avCodec->sample_fmts[0] : AV_SAMPLE_FMT_S16;

    m_encoderCtx->channels = context->getChannels();
    m_encoderCtx->channel_layout = context->getChannelLayout();
    int maxEncoderChannels = getMaxAudioChannels(avCodec);

    if (m_encoderCtx->channels > maxEncoderChannels)
        m_encoderCtx->channels = maxEncoderChannels;

    if (m_dstSampleRate > 0)
        m_encoderCtx->sample_rate = m_dstSampleRate;
    else
        m_encoderCtx->sample_rate = getDefaultDstSampleRate(context->getSampleRate(), avCodec);

    m_encoderCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
    m_encoderCtx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    m_encoderCtx->bit_rate = m_bitrate > 0 ? m_bitrate : getDefaultBitrate(m_encoderCtx);

    if (avcodec_open2(m_encoderCtx, avCodec, 0) < 0)
    {
        m_lastErrMessage = tr("Could not initialize audio encoder.");
        return false;
    }

    avCodec = avcodec_find_decoder(context->getCodecId());
    if (!avCodec)
    {
        m_lastErrMessage = tr("Could not find decoder for codec %1.")
            .arg(context->getCodecId());
        return false;
    }

    m_decoderCtx = avcodec_alloc_context3(0);
    QnFfmpegHelper::mediaContextToAvCodecContext(m_decoderCtx, context);
    if (avcodec_open2(m_decoderCtx, avCodec, 0) < 0)
    {
        m_lastErrMessage = tr("Could not initialize audio decoder.");
        return false;
    }

    m_encodedDuration = 0;
    m_isOpened = true;
    return true;
}

bool QnFfmpegAudioTranscoder::isOpened() const
{
    return m_isOpened;
}

bool QnFfmpegAudioTranscoder::existMoreData() const
{
    return m_resampler.hasFrame();
}

bool QnFfmpegAudioTranscoder::initResampler()
{
    FfmpegAudioResampler::Config config {
        m_decoderCtx->sample_rate,
        static_cast<int64_t>(m_decoderCtx->channel_layout),
        m_decoderCtx->sample_fmt,
        m_encoderCtx->sample_rate,
        static_cast<int64_t>(m_encoderCtx->channel_layout),
        m_encoderCtx->sample_fmt,
        static_cast<uint32_t>(m_encoderCtx->frame_size),
        m_encoderCtx->channels};

    return m_resampler.init(config);
}

int QnFfmpegAudioTranscoder::transcodePacket(
    const QnConstAbstractMediaDataPtr& media, QnAbstractMediaDataPtr* const result)
{
    int error = 0;

    if (result)
        result->reset();

    // push media to decoder
    if (media)
    {
        if (media->dataType != QnAbstractMediaData::DataType::AUDIO)
            return 0;

        tuneContextsWithMedia(m_decoderCtx, m_encoderCtx, media);
        if (!m_resamplerInitialized)
        {
            if (!initResampler())
                return AVERROR(EINVAL);
            m_resamplerInitialized = true;
        }

        const std::chrono::microseconds kTimestampDiffUs(media->timestamp - m_lastTimestamp);
        const bool tooBigJitter = std::abs(kTimestampDiffUs.count()) > kMaxAudioJitterUs.count()
            || m_lastTimestamp == AV_NOPTS_VALUE;

        if (tooBigJitter)
        {
            m_encodedDuration = 0;
            m_firstEncodedPts = media->timestamp;
        }

        m_lastTimestamp = media->timestamp;

        QnFfmpegAvPacket packetToDecode(
            const_cast<quint8*>((const quint8*) media->data()),
            static_cast<int>(media->dataSize()));

        error = avcodec_send_packet(m_decoderCtx, &packetToDecode);

        NX_ASSERT(
            error != AVERROR(EAGAIN),
            lit("Wrong transcoder usage: decoder can't receive more data, pull all the encoded packets first"));

        if (error)
            return error;
    }

    while (true)
    {
        // first we try to fill the frame with existing samples
        AVFrame* frame = m_resampler.nextFrame();
        if (frame)
        {
            error = avcodec_send_frame(m_encoderCtx, frame);
            if (error)
            {
                m_lastErrMessage = tr("Could not send audio frame to encoder, Error code: %1.")
                    .arg(error);
                return error;
            }

            QnFfmpegAvPacket encodedPacket;
            error = avcodec_receive_packet(m_encoderCtx, &encodedPacket);
            if (error == AVERROR(EAGAIN)) // Not enough data to encode packet.
                continue;

            if (error)
            {
                m_lastErrMessage = tr("Could not receive audio packet from encoder, Error code: %1.")
                    .arg(error);
                return error;
            }

            *result = createMediaDataFromAVPacket(encodedPacket);
            return 0;
        }

        error = avcodec_receive_frame(m_decoderCtx, m_frameDecodeTo);
        // There is not enough data to decode
        if (error == AVERROR(EAGAIN))
            return 0;

        if (error)
        {
            m_lastErrMessage = tr("Could not receive audio frame from decoder, Error code: %1.")
                .arg(error);
            return error;
        }
        if (!m_resampler.pushFrame(m_frameDecodeTo))
        {
            m_lastErrMessage = tr("Could not allocate sample buffers");
            return AVERROR(EINVAL);
        }
    }

    return 0;
}

AVCodecContext* QnFfmpegAudioTranscoder::getCodecContext()
{
    return m_encoderCtx;
}

void QnFfmpegAudioTranscoder::setSampleRate(int value)
{
    m_dstSampleRate = value;
}

void QnFfmpegAudioTranscoder::tuneContextsWithMedia(
    AVCodecContext* inCtx,
    AVCodecContext* outCtx,
    const QnConstAbstractMediaDataPtr& media)
{
    if (inCtx->frame_size == 0)
        inCtx->frame_size = kDefaultFrameSize;

    if (inCtx->channel_layout == 0 && media->context)
        inCtx->channel_layout = media->context->getChannelLayout();

    if (inCtx->channel_layout == 0)
        inCtx->channel_layout = av_get_default_channel_layout(inCtx->channels);

    if (outCtx->frame_size == 0)
        outCtx->frame_size = inCtx->frame_size;

    if (outCtx->channel_layout == 0)
        outCtx->channel_layout = av_get_default_channel_layout(outCtx->channels);
}

QnAbstractMediaDataPtr QnFfmpegAudioTranscoder::createMediaDataFromAVPacket(const AVPacket &packet)
{
    if (!m_context)
        m_context = QnConstMediaContextPtr(new QnAvCodecMediaContext(m_encoderCtx));

    auto resultAudioData = new QnWritableCompressedAudioData(CL_MEDIA_ALIGNMENT, packet.size, m_context);
    resultAudioData->compressionType = m_codecId;

    AVRational r = {1, 1000000};
    m_encodedDuration += packet.duration;
    resultAudioData->timestamp =
        av_rescale_q(m_encodedDuration, m_encoderCtx->time_base, r) + m_firstEncodedPts;
    resultAudioData->m_data.write((const char*)packet.data, packet.size);
    return  QnCompressedAudioDataPtr(resultAudioData);
}

#endif // ENABLE_DATA_PROVIDERS
