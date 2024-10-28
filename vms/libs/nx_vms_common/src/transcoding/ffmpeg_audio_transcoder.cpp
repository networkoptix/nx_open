// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ffmpeg_audio_transcoder.h"

#include <nx/media/audio_data_packet.h>
#include <nx/media/codec_parameters.h>
#include <nx/media/config.h>
#include <nx/media/ffmpeg/av_packet.h>
#include <nx/media/ffmpeg/ffmpeg_utils.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <transcoding/transcoding_utils.h>

namespace {

const int kDefaultFrameSize = 1024;

int getMaxAudioChannels(const AVCodec* avCodec)
{
    if (!avCodec->ch_layouts)
        return 1; // default value if unknown

    int result = 0;
    for (auto layout = avCodec->ch_layouts; layout->nb_channels; ++layout)
        result = std::max(result, layout->nb_channels);
    return result;
}

int getDefaultDstSampleRate(int srcSampleRate, const AVCodec* avCodec)
{
    int result = srcSampleRate;
    bool isPcmCodec = avCodec->id == AV_CODEC_ID_ADPCM_G726
        || avCodec->id == AV_CODEC_ID_PCM_MULAW
        || avCodec->id == AV_CODEC_ID_PCM_ALAW;

    if (isPcmCodec)
        result = 8000;
    else
        result = std::max(result, 16000);

    if (avCodec->id == AV_CODEC_ID_VORBIS) // supported_samplerates is empty for this codec type
        result = std::min(result, 44100);

    if (avCodec->supported_samplerates) // select closest supported sample rate
    {
        int diff = std::numeric_limits<int>::max();
        for (const int* sampleRate = avCodec->supported_samplerates; *sampleRate; ++sampleRate)
        {
            int currentDiff = abs(*sampleRate - srcSampleRate);
            if (diff > currentDiff)
            {
                result = *sampleRate;
                diff = currentDiff;
            }
        }
    }
    return result;
}

int getDefaultBitrate(AVCodecContext* context)
{
    if (context->codec_id == AV_CODEC_ID_ADPCM_G726)
        return 16'000; // G726 supports bitrate in range [16'000..40'000] Kbps only.
    return 64'000 * context->ch_layout.nb_channels;
}

} // namespace

QnFfmpegAudioTranscoder::QnFfmpegAudioTranscoder(AVCodecID codecId):
    QnAudioTranscoder(codecId),
    m_encoderCtx(nullptr),
    m_decoderCtx(nullptr),
    m_dstSampleRate(0),
    m_isOpened(false),
    m_dstFrameSize(0)
{
}

QnFfmpegAudioTranscoder::~QnFfmpegAudioTranscoder()
{
    avcodec_free_context(&m_encoderCtx);
    avcodec_free_context(&m_decoderCtx);

}

bool QnFfmpegAudioTranscoder::open(const QnConstCompressedAudioDataPtr& audio)
{
    if (!audio->context)
    {
        NX_WARNING(this, "Audio context was not specified.");
        return false;
    }
    return open(audio->context);
}

bool QnFfmpegAudioTranscoder::open(const CodecParametersConstPtr& context)
{
    NX_ASSERT(context);

    const AVCodec* avCodec = avcodec_find_encoder(m_codecId);
    if (!avCodec)
    {
        NX_WARNING(this, "Could not find encoder for codec %1.", m_codecId);
        return false;
    }

    m_encoderCtx = avcodec_alloc_context3(avCodec);
    m_encoderCtx->sample_fmt = avCodec->sample_fmts[0] != AV_SAMPLE_FMT_NONE ? avCodec->sample_fmts[0] : AV_SAMPLE_FMT_S16;

    av_channel_layout_default(
        &m_encoderCtx->ch_layout, std::min(context->getChannels(), getMaxAudioChannels(avCodec)));
    if (m_dstSampleRate > 0)
        m_encoderCtx->sample_rate = m_dstSampleRate;
    else
        m_encoderCtx->sample_rate = getDefaultDstSampleRate(context->getSampleRate(), avCodec);

    m_encoderCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    m_encoderCtx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    m_encoderCtx->bit_rate = m_bitrate > 0 ? m_bitrate : getDefaultBitrate(m_encoderCtx);

    if (avcodec_open2(m_encoderCtx, avCodec, 0) < 0)
    {
        NX_WARNING(this, "Could not initialize audio encoder.");
        return false;
    }

    avCodec = avcodec_find_decoder(context->getCodecId());
    if (!avCodec)
    {
        NX_WARNING(this, "Could not find decoder for codec %1.", context->getCodecId());
        return false;
    }

    m_decoderCtx = avcodec_alloc_context3(0);
    context->toAvCodecContext(m_decoderCtx);
    if (avcodec_open2(m_decoderCtx, avCodec, 0) < 0)
    {
        NX_WARNING(this, "Could not initialize audio decoder.");
        return false;
    }

    m_isOpened = true;
    return true;
}

bool QnFfmpegAudioTranscoder::isOpened() const
{
    return m_isOpened;
}

bool QnFfmpegAudioTranscoder::initResampler()
{
    FfmpegAudioResampler::Config config {
        m_decoderCtx->sample_rate,
        m_decoderCtx->ch_layout,
        m_decoderCtx->sample_fmt,
        m_encoderCtx->sample_rate,
        m_encoderCtx->ch_layout,
        m_encoderCtx->sample_fmt,
        static_cast<uint32_t>(m_encoderCtx->frame_size)};

    return m_resampler.init(config);
}

int QnFfmpegAudioTranscoder::transcodePacket(
    const QnConstAbstractMediaDataPtr& media, QnAbstractMediaDataPtr* const result)
{
    if (result)
        result->reset();

    if (media && media->dataType != QnAbstractMediaData::DataType::AUDIO)
        return 0;

    if (media && !sendPacket(media))
        return -1;

    return receivePacket(result) ? 0 : -1;
}

bool QnFfmpegAudioTranscoder::sendPacket(const QnConstAbstractMediaDataPtr& media)
{
    m_channelNumber = media->channelNumber;

    // 1. push media to decoder
    tuneContextsWithMedia(m_decoderCtx, m_encoderCtx, media);
    if (!m_resamplerInitialized)
    {
        if (!initResampler())
            return false;

        m_resamplerInitialized = true;
    }

    auto avPacket = av_packet_alloc();
    avPacket->data = (uint8_t*)(media->data());
    avPacket->size = media->dataSize();
    avPacket->dts = media->timestamp;
    avPacket->pts = media->timestamp;

    int error = avcodec_send_packet(m_decoderCtx, avPacket);
    av_packet_free(&avPacket);

    if (error && error != AVERROR_EOF)
    {
        NX_WARNING(this, "ffmpeg audio decoder error: %1", nx::media::ffmpeg::avErrorToString(error));
        return false;
    }

    // 2. get media from decoder
    while (true)
    {
        AVFrame* decodedFrame = av_frame_alloc();
        if (!decodedFrame)
        {
            NX_ERROR(this, "Failed to transcode audio packet: out of memory");
            return false;
        }
        auto guard = nx::utils::makeScopeGuard([&decodedFrame]() {av_frame_free(&decodedFrame); });

        error = avcodec_receive_frame(m_decoderCtx, decodedFrame);
        // There is not enough data to decode
        if (error == AVERROR(EAGAIN) || error == AVERROR_EOF)
            break;

        if (error != 0)
        {
            NX_WARNING(this, "Could not receive audio frame from decoder, Error code: %1.",
                nx::media::ffmpeg::avErrorToString(error));
            return false;
        }

        // 3. Resample data
        if (!m_resampler.pushFrame(decodedFrame))
        {
            NX_WARNING(this, "Could not allocate sample buffers");
            return false;
        }
    }
    return true;
}

bool QnFfmpegAudioTranscoder::receivePacket(QnAbstractMediaDataPtr* const result)
{
    while (true)
    {
        // 1. Try to get media from encoder.
        nx::media::ffmpeg::AvPacket avPacket;
        auto packet = avPacket.get();
        int status = avcodec_receive_packet(m_encoderCtx, packet);
        if (status == 0)
        {
            *result = createMediaDataFromAVPacket(*packet);
            return true;
        }
        if (status == AVERROR_EOF)
            return true;

        if (status && status != AVERROR(EAGAIN))
        {
            NX_WARNING(this, "Could not receive audio packet from encoder, Error code: %1.",
                nx::media::ffmpeg::avErrorToString(status));
            return false;
        }

        // 2. Send data to the encoder.
        AVFrame* resampledFrame = m_resampler.nextFrame();
        if (!resampledFrame)
            return true;

        status = avcodec_send_frame(m_encoderCtx, resampledFrame);
        if (status && status != AVERROR_EOF)
        {
            NX_WARNING(this, "Could not send audio frame to encoder, Error code: %1.",
                nx::media::ffmpeg::avErrorToString(status));
            return false;
        }
    }
    return true;
}

AVCodecContext* QnFfmpegAudioTranscoder::getCodecContext()
{
    return m_encoderCtx;
}

void QnFfmpegAudioTranscoder::setSampleRate(int value)
{
    m_dstSampleRate = value;
}

void QnFfmpegAudioTranscoder::setFrameSize(int value)
{
    m_dstFrameSize = value;
}

void QnFfmpegAudioTranscoder::tuneContextsWithMedia(
    AVCodecContext* inCtx,
    AVCodecContext* outCtx,
    const QnConstAbstractMediaDataPtr& media)
{
    if (inCtx->frame_size == 0)
        inCtx->frame_size = kDefaultFrameSize;

    if (inCtx->ch_layout.u.mask == 0 && media->context)
        inCtx->ch_layout = media->context->getAvCodecParameters()->ch_layout;

    if (outCtx->frame_size == 0)
        outCtx->frame_size = (m_dstFrameSize > 0 ? m_dstFrameSize : inCtx->frame_size);
}

QnAbstractMediaDataPtr QnFfmpegAudioTranscoder::createMediaDataFromAVPacket(const AVPacket &packet)
{
    if (!m_context)
        m_context = CodecParametersConstPtr(new CodecParameters(m_encoderCtx));

    auto resultAudioData = new QnWritableCompressedAudioData(packet.size, m_context);
    resultAudioData->compressionType = m_codecId;

    resultAudioData->timestamp = packet.pts;
    resultAudioData->m_data.write((const char*)packet.data, packet.size);
    resultAudioData->channelNumber = m_channelNumber;
    return  QnCompressedAudioDataPtr(resultAudioData);
}
