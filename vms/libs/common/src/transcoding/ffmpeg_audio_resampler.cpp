#include "ffmpeg_audio_resampler.h"

extern "C" {
#include <libswresample/swresample.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

#include "transcoding_utils.h"
#include <nx/utils/log/log.h>

FfmpegAudioResampler::FfmpegAudioResampler()
    : m_frame(av_frame_alloc())
{
}

FfmpegAudioResampler::~FfmpegAudioResampler()
{
    if (m_swrContext)
        swr_free(&m_swrContext);
    av_frame_free(&m_frame);
}

bool FfmpegAudioResampler::init(const Config& config)
{
    m_config = config;
    m_swrContext = swr_alloc_set_opts(
        NULL,
        config.dstChannelLayout,
        config.dstSampleFormat,
        config.dstSampleRate,
        config.srcChannelLayout,
        config.srcSampleFormat,
        config.srcSampleRate,
        0,
        NULL);
    if (!m_swrContext)
    {
        NX_ERROR(this, "Could not allocate resample context");
        return false;
    }
    int status = swr_init(m_swrContext);
    if (status < 0)
    {
        NX_ERROR(this, "Could not open resample context, error: %1",
            nx::transcoding::ffmpegErrorText(status));
        swr_free(&m_swrContext);
        m_swrContext = nullptr;
        return false;
    }
    m_buffer.init(
        FfmpegAudioBuffer::Config{config.dstChannelCount, config.dstSampleFormat});
    return true;
}

bool FfmpegAudioResampler::pushFrame(AVFrame* inputFrame)
{
    uint64_t outSampleCount = getOutputSampleCount(inputFrame->nb_samples);
    uint8_t** sampleBuffer = m_buffer.startWriting(outSampleCount);
    if (!sampleBuffer)
    {
        NX_ERROR(this, "Failed to get sample buffer");
        return false;
    }
    int result = swr_convert(
        m_swrContext,
        sampleBuffer,
        outSampleCount,
        const_cast<const uint8_t**>(inputFrame->extended_data),
        inputFrame->nb_samples);

    if (result < 0)
    {
        NX_ERROR(this, "Failed to resample audio data %1",
            nx::transcoding::ffmpegErrorText(result));
        return false;
    }
    m_buffer.finishWriting(static_cast<uint64_t>(result));
    return true;
}

uint64_t FfmpegAudioResampler::getOutputSampleCount(uint64_t inputSampleCount)
{
    return av_rescale_rnd(
        swr_get_delay(m_swrContext, m_config.srcSampleRate) + inputSampleCount,
        m_config.dstSampleRate,
        m_config.srcSampleRate,
        AV_ROUND_UP);
}

bool FfmpegAudioResampler::hasFrame() const
{
    return m_buffer.sampleCount() >= m_config.dstFrameSize;
}

AVFrame* FfmpegAudioResampler::nextFrame()
{
    if (!m_buffer.popData(m_config.dstFrameSize, m_frame->data))
        return nullptr;

    m_frame->extended_data = m_frame->data;
    m_frame->nb_samples = m_config.dstFrameSize;
    m_frame->sample_rate = m_config.dstSampleRate;
    return m_frame;
}
