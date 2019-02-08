#pragma once

#include <cstdint>

extern "C" {
#include <libavformat/avformat.h>
}

#include "ffmpeg_audio_buffer.h"

struct SwrContext;
struct AVFrame;

class FfmpegAudioResampler
{
public:
    struct Config
    {
        int32_t srcSampleRate = 0;
        int64_t srcChannelLayout = 0;
        AVSampleFormat srcSampleFormat = AV_SAMPLE_FMT_NONE;
        int32_t dstSampleRate = 0;
        int64_t dstChannelLayout = 0;
        AVSampleFormat dstSampleFormat = AV_SAMPLE_FMT_NONE;
        uint32_t dstFrameSize = 0;
        int32_t dstChannelCount = 0;
    };

public:
    FfmpegAudioResampler();
    ~FfmpegAudioResampler();

    bool init(const Config& config);
    bool pushFrame(AVFrame* inputFrame);
    AVFrame* nextFrame();
    bool hasFrame() const;

private:
    uint64_t getOutputSampleCount(uint64_t inputSampleCount);
    bool allocSampleBuffers(uint64_t srcFrameSize);

private:
    FfmpegAudioBuffer m_buffer;
    Config m_config;
    AVFrame* m_frame = nullptr;
    SwrContext* m_swrContext = nullptr;
};
