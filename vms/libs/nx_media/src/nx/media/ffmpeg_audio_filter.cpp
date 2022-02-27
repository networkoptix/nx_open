// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ffmpeg_audio_filter.h"

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

#include <sstream>

#include <nx/utils/log/log.h>

namespace nx {
namespace media {

namespace {

static std::string ffmpegErrorMessage(int error)
{
    char errorStr[AV_ERROR_MAX_STRING_SIZE];
    return av_make_error_string(errorStr, AV_ERROR_MAX_STRING_SIZE, error);
}

struct InOutWrapper
{
    InOutWrapper(): ptr(avfilter_inout_alloc()) {}
    ~InOutWrapper() { avfilter_inout_free(&ptr); }

    void init(AVFilterContext* context, const char* name)
    {
        ptr->name = strdup(name);
        ptr->filter_ctx = context;
        ptr->pad_idx = 0;
        ptr->next = nullptr;
    }

    AVFilterInOut* ptr;
};

} // namespace

FfmpegAudioFilter::~FfmpegAudioFilter()
{
    close();
}

void FfmpegAudioFilter::close()
{
    if (m_graph)
        avfilter_graph_free(&m_graph);
    if (m_filteredFrame)
        av_frame_free(&m_filteredFrame);
    m_initialized = false;
}

bool FfmpegAudioFilter::init(
    AVCodecContext* decContext, AVRational timeBase, const char* description)
{
    close();

    int status = 0;
    const AVFilter* bufferSrc = avfilter_get_by_name("abuffer");
    const AVFilter* bufferSink = avfilter_get_by_name("abuffersink");
    InOutWrapper inputs;
    InOutWrapper outputs;
    m_graph = avfilter_graph_alloc();
    m_filteredFrame = av_frame_alloc();
    if (!outputs.ptr || !inputs.ptr || !m_graph || !m_filteredFrame)
    {
        close();
        return false;
    }

    if (!decContext->channel_layout)
        decContext->channel_layout = av_get_default_channel_layout(decContext->channels);

    static const enum AVSampleFormat out_sample_fmts[] = { decContext->sample_fmt, (AVSampleFormat)-1 };
    static const int64_t out_channel_layouts[] = { static_cast<int64_t>(decContext->channel_layout), -1 };
    static const int out_sample_rates[] = { decContext->sample_rate, -1 };

    std::stringstream args;
    args << "time_base=" << timeBase.num << "/" << timeBase.den << ":"
         << "sample_rate=" << decContext->sample_rate << ":"
         << "sample_fmt=" << av_get_sample_fmt_name(decContext->sample_fmt) << ":"
         << "channel_layout=0x" << decContext->channel_layout;

    status = avfilter_graph_create_filter(
        &m_bufferSrc, bufferSrc, "in", args.str().c_str(), NULL, m_graph);
    if (status < 0)
    {
        NX_ERROR(this, "Cannot create audio buffer source, arguments: [%1], error code: [%2]",
            args.str(), ffmpegErrorMessage(status));
        close();
        return false;
    }

    status = avfilter_graph_create_filter(&m_bufferSink, bufferSink, "out", NULL, NULL, m_graph);
    if (status < 0)
    {
        NX_ERROR(this, "Cannot create audio buffer sink");
        close();
        return false;
    }

    status = av_opt_set_int_list(m_bufferSink, "sample_fmts", out_sample_fmts, -1,
        AV_OPT_SEARCH_CHILDREN);
    if (status < 0)
    {
        NX_ERROR(this, "Cannot set output sample format");
        close();
        return false;
    }
    status = av_opt_set_int_list(m_bufferSink, "channel_layouts", out_channel_layouts, -1,
        AV_OPT_SEARCH_CHILDREN);
    if (status < 0)
    {
        NX_ERROR(this, "Cannot set output channel layout");
        close();
        return false;
    }
    status = av_opt_set_int_list(m_bufferSink, "sample_rates", out_sample_rates, -1,
        AV_OPT_SEARCH_CHILDREN);
    if (status < 0)
    {
        NX_ERROR(this, "Cannot set output sample rate");
        close();
        return false;
    }

    outputs.init(m_bufferSrc, "in");
    inputs.init(m_bufferSink, "out");
    status = avfilter_graph_parse_ptr(m_graph, description, &inputs.ptr, &outputs.ptr, NULL);
    if (status < 0)
    {
        close();
        return false;
    }

    status = avfilter_graph_config(m_graph, NULL);
    if (status < 0)
    {
        close();
        return false;
    }

    m_initialized = true;
    return true;
}

bool FfmpegAudioFilter::pushFrame(AVFrame* srcFrame)
{
    if (!m_initialized)
        return false;

    int status = av_buffersrc_add_frame_flags(m_bufferSrc, srcFrame, 0);
    if (status < 0)
    {
        NX_ERROR(this, "Software codec filter: Error while feeding the filtergraph: [%1]",
            ffmpegErrorMessage(status));
        return false;
    }
    return true;
}

AVFrame* FfmpegAudioFilter::nextFrame()
{
    if (!m_initialized)
        return nullptr;

    int status = av_buffersink_get_frame(m_bufferSink, m_filteredFrame);
    if (status == AVERROR(EAGAIN) || status == AVERROR_EOF)
        return nullptr;

    if (status < 0)
    {
        NX_ERROR(this, "Software codec filter: process error: [%1]", ffmpegErrorMessage(status));
        return nullptr;
    }
    return m_filteredFrame;
}

} // namespace media
} // namespace nx
