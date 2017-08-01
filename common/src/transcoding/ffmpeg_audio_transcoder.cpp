#ifdef ENABLE_DATA_PROVIDERS

#include "ffmpeg_audio_transcoder.h"

#include <nx/streaming/audio_data_packet.h>
#include <nx/streaming/av_codec_media_context.h>
#include <nx/streaming/config.h>
#include <nx/utils/log/assert.h>

extern "C" {
#include <libavutil/opt.h>
}

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
    m_resampleCtx(nullptr),
    m_context(nullptr),

    m_firstEncodedPts(AV_NOPTS_VALUE),
    m_lastTimestamp(AV_NOPTS_VALUE),
    m_encodedDuration(0),

    m_sampleBuffers(nullptr),
    m_resampleDstBuffers(nullptr),
    m_currentSampleCount(0),
    m_srcBufferOffsetInSamples(0),

    m_dstSampleRate(0),
    m_frameDecodeTo(av_frame_alloc()),
    m_frameToEncode(av_frame_alloc()),
    m_isOpened(false)
{
}

QnFfmpegAudioTranscoder::~QnFfmpegAudioTranscoder()
{
    if (m_resampleCtx)
        swr_free(&m_resampleCtx);

    QnFfmpegHelper::deleteAvCodecContext(m_encoderCtx);
    QnFfmpegHelper::deleteAvCodecContext(m_decoderCtx);

    av_frame_free(&m_frameDecodeTo);
    av_frame_free(&m_frameToEncode);

    if (m_sampleBuffers)
    {
        // AllocSampleBuffers does two av_allocs: buffer for pointers and payload data.
        // It requires two free calls.
        av_freep(m_sampleBuffers);
        av_freep(&m_sampleBuffers);
    }

    if (m_resampleDstBuffers)
        delete[] m_resampleDstBuffers;
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
    int maxEncoderChannels = getMaxAudioChannels(avCodec);

    if (m_encoderCtx->channels > maxEncoderChannels)
        m_encoderCtx->channels = maxEncoderChannels;

    if (m_dstSampleRate > 0)
        m_encoderCtx->sample_rate = m_dstSampleRate;
    else
        m_encoderCtx->sample_rate = getDefaultDstSampleRate(context->getSampleRate(), avCodec);

    m_encoderCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
    m_encoderCtx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    m_encoderCtx->bit_rate = m_bitrate > 0 ? m_bitrate : 64000 * m_encoderCtx->channels;

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
    return m_currentSampleCount >= m_encoderCtx->frame_size;
}

int QnFfmpegAudioTranscoder::transcodePacket(const QnConstAbstractMediaDataPtr& media, QnAbstractMediaDataPtr* const result)
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

    error = initResampleCtx(m_decoderCtx, m_encoderCtx);

    if (error)
    {
        m_lastErrMessage = tr("Could not initialize resampling context, error code: %1")
            .arg(error);
        return error;
    }

    error = allocSampleBuffers(m_decoderCtx, m_encoderCtx, m_resampleCtx);
    if (error)
    {
        m_lastErrMessage = tr("Could not allocate sample buffers, error code: %1")
            .arg(error);
        return error;
    }

    while (true)
    {
        // first we try to fill the frame with existing samples
        if (m_currentSampleCount >= m_encoderCtx->frame_size)
        {
            fillFrameWithSamples(
                m_frameToEncode,
                m_sampleBuffers,
                m_srcBufferOffsetInSamples,
                m_encoderCtx);

            m_srcBufferOffsetInSamples += m_encoderCtx->frame_size;
            m_currentSampleCount -= m_encoderCtx->frame_size;

            error = avcodec_send_frame(m_encoderCtx, m_frameToEncode);

            if (error)
            {
                m_lastErrMessage = tr("Could not send audio frame to encoder, Error code: %1.")
                    .arg(error);
                return error;
            }

            QnFfmpegAvPacket encodedPacket;
            error = avcodec_receive_packet(m_encoderCtx, &encodedPacket);

            // Not enough data to encode packet.
            if (error == AVERROR(EAGAIN))
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

        shiftSamples(
            m_encoderCtx,
            m_sampleBuffers,
            m_srcBufferOffsetInSamples,
            m_currentSampleCount);

        m_srcBufferOffsetInSamples = 0;

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

        error = doResample();

        if (error)
            return error;
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

int QnFfmpegAudioTranscoder::initResampleCtx(
    const AVCodecContext *inCtx,
    const AVCodecContext *outCtx)
{
    if (m_resampleCtx)
        return 0;

    m_resampleCtx = swr_alloc();

    av_opt_set_channel_layout(
        m_resampleCtx,
        "in_channel_layout",
        inCtx->channel_layout,
        0);

    av_opt_set_channel_layout(
        m_resampleCtx,
        "out_channel_layout",
        outCtx->channel_layout,
        0);
    av_opt_set_int(m_resampleCtx, "in_channel_count", inCtx->channels, 0);
    av_opt_set_int(m_resampleCtx, "out_channel_count", outCtx->channels, 0);

    av_opt_set_int(m_resampleCtx, "in_sample_rate", inCtx->sample_rate, 0);
    av_opt_set_int(m_resampleCtx, "out_sample_rate", outCtx->sample_rate, 0);
    av_opt_set_sample_fmt(m_resampleCtx, "in_sample_fmt", inCtx->sample_fmt, 0);
    av_opt_set_sample_fmt(m_resampleCtx, "out_sample_fmt", outCtx->sample_fmt, 0);

    return swr_init(m_resampleCtx);
}

int QnFfmpegAudioTranscoder::allocSampleBuffers(
    const AVCodecContext *inCtx,
    const AVCodecContext *outCtx,
    const SwrContext* resampleCtx)
{
    if (m_sampleBuffers)
        return 0;

    auto outCtxBufferCount = getPlanesCount(outCtx);
    auto inCtxBufferCount = getPlanesCount(inCtx);

    // I don't get why we can't just use outCtxBufferCount,
    // but it leads to segfault in swr_convert.
    auto bufferCount = std::max(inCtxBufferCount, outCtxBufferCount);

    //get output sample count after resampling of a single frame
    auto outSampleCount = av_rescale_rnd(
        swr_get_delay(
            const_cast<SwrContext*>(resampleCtx),
            inCtx->sample_rate) + inCtx->frame_size,
        outCtx->sample_rate,
        inCtx->sample_rate,
        AV_ROUND_UP);

    std::size_t bufferSize = outSampleCount + outCtx->frame_size;

    int linesize = 0;
    const int kDefaultAlign = 0;

    auto result = av_samples_alloc_array_and_samples(
        &m_sampleBuffers,
        &linesize,
        bufferCount,
        bufferSize,
        outCtx->sample_fmt,
        kDefaultAlign);

    // Maybe it's worth to replace raw pointer with the unique one.
    m_resampleDstBuffers = new uint8_t*[bufferCount];
    for (std::size_t i = 0; i < bufferCount; ++i)
        m_resampleDstBuffers[i] = m_sampleBuffers[i];

    return result >= 0 ? 0 : result;
}

void QnFfmpegAudioTranscoder::shiftSamples(
    const AVCodecContext* ctx,
    uint8_t* const* const buffersBase,
    const std::size_t samplesOffset,
    const std::size_t samplesCountPerChannel)
{
    if (!samplesOffset)
        return;

    auto buffersCount = getPlanesCount(ctx);
    auto multCoefficient = getSampleMultiplyCoefficient(ctx);

    auto bytesNumberToCopy = samplesCountPerChannel * multCoefficient;

    for (std::size_t bufferNum = 0; bufferNum < buffersCount; ++bufferNum)
    {
        memmove(
            buffersBase[bufferNum],
            buffersBase[bufferNum] + samplesOffset * multCoefficient,
            bytesNumberToCopy);

        m_resampleDstBuffers[bufferNum] = buffersBase[bufferNum] + bytesNumberToCopy;
    }
}

void QnFfmpegAudioTranscoder::fillFrameWithSamples(
    AVFrame* frame,
    uint8_t** sampleBuffers,
    const std::size_t offset,
    const AVCodecContext* encoderCtx)
{
    auto planesCount = getPlanesCount(m_encoderCtx);

    for (auto i = 0; i < planesCount; ++i)
    {
        frame->data[i] = sampleBuffers[i]
            + offset * getSampleMultiplyCoefficient(m_encoderCtx);
    }

    frame->extended_data = frame->data;
    frame->nb_samples = encoderCtx->frame_size;
    frame->sample_rate = encoderCtx->sample_rate;
}

int QnFfmpegAudioTranscoder::doResample()
{
    auto outSampleCount = av_rescale_rnd(
        swr_get_delay(
            const_cast<SwrContext*>(m_resampleCtx),
            m_decoderCtx->sample_rate) + m_decoderCtx->frame_size,
        m_encoderCtx->sample_rate,
        m_decoderCtx->sample_rate,
        AV_ROUND_UP);

    auto outSamplesPerChannel = swr_convert(
        m_resampleCtx,
        m_resampleDstBuffers, //< out bufs
        outSampleCount, //< available out size in sample
        const_cast<const uint8_t**>(m_frameDecodeTo->extended_data), //< in bufs
        m_frameDecodeTo->nb_samples); //< in size in samples

    if (outSamplesPerChannel < 0)
        return outSamplesPerChannel;

    m_currentSampleCount += outSamplesPerChannel;

    auto buffersCount = getPlanesCount(m_encoderCtx);

    for (std::size_t bufferNum = 0; bufferNum < buffersCount; ++bufferNum)
    {
        m_resampleDstBuffers[bufferNum] += outSamplesPerChannel
            * getSampleMultiplyCoefficient(m_encoderCtx);
    }

    return 0;
}

std::size_t QnFfmpegAudioTranscoder::getSampleMultiplyCoefficient(const AVCodecContext* ctx)
{
    return av_get_bytes_per_sample(ctx->sample_fmt)
        * (av_sample_fmt_is_planar(ctx->sample_fmt) ? 1 : ctx->channels);
}

std::size_t QnFfmpegAudioTranscoder::getPlanesCount(const AVCodecContext* ctx)
{
    return av_sample_fmt_is_planar(ctx->sample_fmt) ? ctx->channels : 1;
}

QnAbstractMediaDataPtr QnFfmpegAudioTranscoder::createMediaDataFromAVPacket(const AVPacket &packet)
{
    if (!m_context)
        m_context = QnConstMediaContextPtr(new QnAvCodecMediaContext(m_encoderCtx));

    auto resultAudioData = new QnWritableCompressedAudioData(CL_MEDIA_ALIGNMENT, packet.size, m_context);
    resultAudioData->compressionType = m_codecId;

    AVRational r = {1, 1000000};
    m_encodedDuration += packet.duration;
    resultAudioData->timestamp  = av_rescale_q(m_encodedDuration, m_encoderCtx->time_base, r) + m_firstEncodedPts;

    resultAudioData->m_data.write((const char*)packet.data, packet.size);
    return  QnCompressedAudioDataPtr(resultAudioData);
}

#endif // ENABLE_DATA_PROVIDERS
