// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <queue>

#include <stdint.h>

#include "ffmpeg_audio_buffer.h"

struct SwrContext;
struct AVFrame;

class FfmpegAudioResampler
{
public:
    struct Config
    {
        int32_t srcSampleRate = 0;
        AVChannelLayout srcChannelLayout;
        AVSampleFormat srcSampleFormat = AV_SAMPLE_FMT_NONE;
        int32_t dstSampleRate = 0;
        AVChannelLayout dstChannelLayout;
        AVSampleFormat dstSampleFormat = AV_SAMPLE_FMT_NONE;
        uint32_t dstFrameSize = 0;
    };

public:
    FfmpegAudioResampler();
    ~FfmpegAudioResampler();

    bool init(const Config& config);
    bool pushFrame(AVFrame* inputFrame);
    AVFrame* nextFrame();
    bool hasFrame() const;

private:
    bool allocSampleBuffers(uint64_t srcFrameSize);

private:
    struct PtsData
    {
        int64_t ptsUs = 0;
        int sampleCount = 0;
    };

    FfmpegAudioBuffer m_buffer;
    Config m_config;
    AVFrame* m_frame = nullptr;
    SwrContext* m_swrContext = nullptr;
    std::deque<PtsData> m_samplePts;
};
