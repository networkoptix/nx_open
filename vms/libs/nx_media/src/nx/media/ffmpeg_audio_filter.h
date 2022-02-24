// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/media/media_fwd.h>

struct AVFilterGraph;
struct AVFilterContext;
struct AVCodecContext;
struct AVRational;
struct AVFrame;

namespace nx {
namespace media {

class FfmpegAudioFilter
{
public:
    ~FfmpegAudioFilter();
    bool init(AVCodecContext* decContext, AVRational timeBase, const char* description);
    bool pushFrame(AVFrame* srcFrame);
    AVFrame* nextFrame();

private:
    void close();

private:
    bool m_initialized = false;
    AVFilterGraph* m_graph = nullptr;
    AVFilterContext* m_bufferSink = nullptr;
    AVFilterContext* m_bufferSrc = nullptr;
    AVFrame* m_filteredFrame = nullptr;
};

} // namespace media
} // namespace nx
