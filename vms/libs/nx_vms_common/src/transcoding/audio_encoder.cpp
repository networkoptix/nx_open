// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audio_encoder.h"

#include <nx/media/ffmpeg/av_packet.h>
#include <nx/media/ffmpeg/ffmpeg_utils.h>
#include <nx/utils/log/log.h>

namespace {

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
    switch(avCodec->id)
    {
        case AV_CODEC_ID_VORBIS: //< Supported_samplerates is empty for this codec type.
            result = std::min(result, 44100);
            break;
        case AV_CODEC_ID_ADPCM_G726:
        case AV_CODEC_ID_PCM_MULAW:
        case AV_CODEC_ID_PCM_ALAW:
            result = 8000;
            break;
        default:
            result = std::max(result, 16000);
    }

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

int getDefaultFrameSize(const AVCodec* avCodec)
{
    switch(avCodec->id)
    {
        case AV_CODEC_ID_ADPCM_G726:
        case AV_CODEC_ID_PCM_MULAW:
        case AV_CODEC_ID_PCM_ALAW:
            return 128;
        default:
            return 1024;
    }
}

int getDefaultBitrate(AVCodecContext* context)
{
    if (context->codec_id == AV_CODEC_ID_ADPCM_G726)
        return 16'000; // G726 supports bitrate in range [16'000..40'000] Kbps only.
    return 64'000 * context->ch_layout.nb_channels;
}

}

namespace nx::media::ffmpeg {

constexpr int64_t kTimeScale = 1'000'000;

AudioEncoder::AudioEncoder():
    m_inputFrame(av_frame_alloc())
{
}

AudioEncoder::~AudioEncoder()
{
    close();
    av_frame_free(&m_inputFrame);
}

void AudioEncoder::close()
{
    if (m_encoderContext)
        avcodec_free_context(&m_encoderContext);
    m_resampler.reset();
    m_flushMode = false;
}

CodecParametersPtr AudioEncoder::codecParameters() const
{
    return m_codecParams;
}

void AudioEncoder::setSampleRate(int value)
{
    m_dstSampleRate = value;
}

bool AudioEncoder::open(
    AVCodecID codecId,
    int sampleRate,
    AVSampleFormat format,
    AVChannelLayout layout,
    int bitrate,
    int dstFrameSize)
{
    close();

    const AVCodec* codec = avcodec_find_encoder(codecId);
    if (!codec)
    {
        NX_WARNING(this, "Failed to initialize audio encoder, codec not found: %1", codecId);
        return false;
    }
    m_sourceformat = format;
    m_encoderContext = avcodec_alloc_context3(codec);
    m_encoderContext->sample_fmt = codec->sample_fmts[0] != AV_SAMPLE_FMT_NONE
        ? codec->sample_fmts[0] : format;
    av_channel_layout_default(
        &m_encoderContext->ch_layout, std::min(layout.nb_channels, getMaxAudioChannels(codec)));
    m_encoderContext->time_base = {1, kTimeScale};
    m_encoderContext->sample_rate = m_dstSampleRate > 0
        ? m_dstSampleRate : getDefaultDstSampleRate(sampleRate, codec);

    m_encoderContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    m_encoderContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    m_encoderContext->bit_rate = bitrate > 0 ? bitrate : getDefaultBitrate(m_encoderContext);
    m_bitrate = m_encoderContext->bit_rate;

    auto status = avcodec_open2(m_encoderContext, codec, nullptr);
    if (status < 0)
    {
        NX_WARNING(this, "Failed to open audio encoder: %1",
            avErrorToString(status));
        return false;
    }

    if (m_encoderContext->frame_size == 0)
        m_encoderContext->frame_size = (dstFrameSize > 0 ? dstFrameSize : getDefaultFrameSize(codec));

    m_codecParams.reset(new CodecParameters(m_encoderContext));

    FfmpegAudioResampler::Config config {
        sampleRate,
        layout,
        format,
        m_encoderContext->sample_rate,
        m_encoderContext->ch_layout,
        m_encoderContext->sample_fmt,
        static_cast<uint32_t>(m_encoderContext->frame_size)};

    m_resampler = std::make_unique<FfmpegAudioResampler>();
    return m_resampler->init(config);
}

bool AudioEncoder::sendFrame(uint8_t* data, int size)
{
    if (!data)
    {
        sendFrame(nullptr);
        return true;
    }
    m_inputFrame->data[0] = data;
    m_inputFrame->extended_data = m_inputFrame->data;
    m_inputFrame->nb_samples = size;
    m_inputFrame->sample_rate = m_encoderContext->sample_rate;
    m_inputFrame->format = m_sourceformat;
    m_inputFrame->ch_layout = m_encoderContext->ch_layout;
    m_inputFrame->pts = m_ptsUs;
    m_ptsUs += (kTimeScale * m_inputFrame->nb_samples) / m_inputFrame->sample_rate;
    return sendFrame(m_inputFrame);
}

bool AudioEncoder::sendFrame(AVFrame* inputFrame)
{
    if (!inputFrame)
    {
        m_flushMode = true;
        return true;
    }
    if (!m_resampler->pushFrame(inputFrame))
    {
        NX_WARNING(this, "Could not allocate sample buffers");
        return false;
    }
    return true;
}

bool AudioEncoder::receivePacket(QnWritableCompressedAudioDataPtr& result)
{
    result.reset();
    while (true)
    {
        // 1. Try to get media from encoder.
        AvPacket avPacket;
        auto packet = avPacket.get();
        int status = avcodec_receive_packet(m_encoderContext, packet);
        if (status == 0)
        {
            result = createMediaDataFromAVPacket(*packet);
            return true;
        }
        if (status == AVERROR_EOF)
            return true;

        if (status && status != AVERROR(EAGAIN))
        {
            NX_WARNING(this, "Could not receive audio packet from encoder, Error code: %1.",
                avErrorToString(status));
            return false;
        }

        // 2. Send data to the encoder.
        AVFrame* resampledFrame = m_resampler->nextFrame();
        if (!resampledFrame && !m_flushMode)
            return true;

        status = avcodec_send_frame(m_encoderContext, resampledFrame);
        if (status && status != AVERROR_EOF)
        {
            NX_WARNING(this, "Could not send audio frame to encoder, Error code: %1.",
                avErrorToString(status));
            return false;
        }
    }
    return true;
}

QnWritableCompressedAudioDataPtr AudioEncoder::createMediaDataFromAVPacket(const AVPacket &packet)
{
    auto resultAudioData = new QnWritableCompressedAudioData(packet.size, m_codecParams);
    resultAudioData->compressionType = m_encoderContext->codec_id;
    resultAudioData->timestamp = packet.pts;
    resultAudioData->m_data.write((const char*)packet.data, packet.size);
    return  QnWritableCompressedAudioDataPtr(resultAudioData);
}

} // namespace nx::media::ffmpeg
