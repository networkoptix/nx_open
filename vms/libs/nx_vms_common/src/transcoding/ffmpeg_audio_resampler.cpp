// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ffmpeg_audio_resampler.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include <nx/media/ffmpeg/ffmpeg_utils.h>
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
    m_samplePts.clear();
    m_config = config;
    swr_alloc_set_opts2(
        &m_swrContext,
        &config.dstChannelLayout,
        config.dstSampleFormat,
        config.dstSampleRate,
        &config.srcChannelLayout,
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
            nx::media::ffmpeg::avErrorToString(status));
        swr_free(&m_swrContext);
        m_swrContext = nullptr;
        return false;
    }
    m_buffer.init(
        FfmpegAudioBuffer::Config{config.dstChannelLayout.nb_channels, config.dstSampleFormat});
    return true;
}

bool FfmpegAudioResampler::pushFrame(AVFrame* inputFrame)
{
    uint64_t outSampleCount = swr_get_out_samples(m_swrContext, inputFrame->nb_samples);

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
            nx::media::ffmpeg::avErrorToString(result));
        return false;
    }
    m_buffer.finishWriting(static_cast<uint64_t>(result));
    m_samplePts.push_back({inputFrame->pts, result});
    return true;
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
    m_frame->format = m_config.dstSampleFormat;
    m_frame->ch_layout = m_config.dstChannelLayout;

    if (m_samplePts.empty())
        return m_frame;
    m_frame->pts = m_samplePts.front().ptsUs;

    auto sampleCount = m_frame->nb_samples;
    while (!m_samplePts.empty() && sampleCount >= m_samplePts.front().sampleCount)
    {
        sampleCount -= m_samplePts.front().sampleCount;
        m_samplePts.pop_front();
    }
    if (m_samplePts.empty())
        return m_frame;

    m_samplePts.front().sampleCount -= sampleCount;
    NX_ASSERT(m_samplePts.front().sampleCount > 0);
    m_samplePts.front().ptsUs +=
        (1'000'000LL * sampleCount) / m_config.dstSampleRate;

    return m_frame;
}
