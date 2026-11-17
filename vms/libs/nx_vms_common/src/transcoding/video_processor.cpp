// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "video_processor.h"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/buffer.h>
} // extern "C"

#include <nx/core/transcoding/filters/filter_chain.h>
#include <nx/media/ffmpeg/ffmpeg_utils.h>
#include <nx/utils/log/log.h>
#include <transcoding/filters/scale_image_filter.h>

namespace nx::transcoding {

namespace {

// Filter name and arguments.
using Filter = std::pair<std::string, std::string>;

Filter scale(const QSize& size)
{
    return {"scale_vaapi",
        nx::format("w=%1:h=%2:format=nv12", size.width(), size.height()).toStdString()};
}

// The chain as VAAPI filters on frames of `size`, or nothing if a filter has no VAAPI counterpart.
// TODO: VMS-63450: Crop and rotation.
std::optional<std::vector<Filter>> toVaapi(
    const nx::core::transcoding::FilterChain& chain, QSize size, const QSize& targetSize)
{
    if (chain.hasMetadataFilters())
        return std::nullopt;

    std::vector<Filter> result;
    for (const auto& filter: chain.filters())
    {
        if (filter.dynamicCast<QnScaleImageFilter>())
        {
            result.push_back(scale(filter->updatedResolution(size)));
        }
        else
        {
            return std::nullopt;
        }
        size = filter->updatedResolution(size);
    }

    // Gives the encoder surfaces of its size.
    if (result.empty() || result.back() != scale(targetSize))
        result.push_back(scale(targetSize));
    return result;
}

} // namespace

/**
 * Hardware filter graphs, one per channel, as each channel's decoder has frames of its own; each
 * is rebuilt when its input changes. Frames that are not the source size are resized first, as the
 * software path does; system memory frames are uploaded.
 */
class VideoProcessor::HardwareGraph
{
public:
    HardwareGraph(AVBufferRef* device, const QSize& sourceSize, std::vector<Filter> filters):
        m_device(av_buffer_ref(device)),
        m_sourceSize(sourceSize),
        m_filters(std::move(filters))
    {
    }

    ~HardwareGraph() { av_buffer_unref(&m_device); }

    CLVideoDecoderOutputPtr process(const CLVideoDecoderOutputPtr& frame)
    {
        Channel& channel = m_channels[frame->channel];
        if (!channel.isCompatible(frame.data()) && !init(channel, frame.data()))
        {
            m_channels.erase(frame->channel);
            return {};
        }

        if (const int status = av_buffersrc_add_frame_flags(
                channel.source, frame.data(), AV_BUFFERSRC_FLAG_KEEP_REF);
            status < 0)
        {
            NX_WARNING(this,
                "Unable to send frame to filter graph: %1",
                nx::media::ffmpeg::avErrorToString(status));
            return {};
        }

        CLVideoDecoderOutputPtr result(new CLVideoDecoderOutput());
        if (const int status = av_buffersink_get_frame(channel.sink, result.data()); status < 0)
        {
            NX_WARNING(this,
                "Unable to get frame from filter graph: %1",
                nx::media::ffmpeg::avErrorToString(status));
            return {};
        }
        result->setMemoryType(MemoryType::VideoMemory);
        result->channel = frame->channel;
        result->pkt_dts = frame->pkt_dts;
        return result;
    }

    // Processes a blank frame, to find out before encoding whether the device can.
    bool probe()
    {
        CLVideoDecoderOutputPtr frame(new CLVideoDecoderOutput());
        if (!frame->reallocate(m_sourceSize, AV_PIX_FMT_NV12))
            return false;
        frame->memZero();
        return !process(frame).isNull();
    }

private:
    struct Channel
    {
        ~Channel() { avfilter_graph_free(&graph); }

        bool isCompatible(const AVFrame* frame) const
        {
            return graph && frame->width == inputWidth && frame->height == inputHeight
                && frame->format == inputFormat
                && (frame->hw_frames_ctx ? frame->hw_frames_ctx->data : nullptr) == inputFrames;
        }

        AVFilterGraph* graph = nullptr;
        AVFilterContext* source = nullptr;
        AVFilterContext* sink = nullptr;
        int inputWidth = 0;
        int inputHeight = 0;
        int inputFormat = -1;
        const void* inputFrames = nullptr;
    };

    bool init(Channel& channel, const AVFrame* frame)
    {
        avfilter_graph_free(&channel.graph);
        channel.graph = avfilter_graph_alloc();
        if (!channel.graph)
            return false;

        AVFilterContext* previous = nullptr;
        // Creates the filters one by one instead of parsing a graph description: hwupload needs
        // the device before init, and parsing initializes filters right away.
        const auto add = [&](const Filter& filterAndArgs, AVBufferSrcParameters* params = nullptr)
        {
            const auto& [name, args] = filterAndArgs;
            AVFilterContext* filter = avfilter_graph_alloc_filter(
                channel.graph, avfilter_get_by_name(name.c_str()), name.c_str());
            if (!filter)
            {
                NX_WARNING(this, "Unable to create filter %1", name);
                return (AVFilterContext*) nullptr;
            }
            filter->hw_device_ctx = av_buffer_ref(m_device);
            if (params && av_buffersrc_parameters_set(filter, params) < 0)
                return (AVFilterContext*) nullptr;
            if (const int status =
                    avfilter_init_str(filter, args.empty() ? nullptr : args.c_str());
                status < 0)
            {
                NX_WARNING(this,
                    "Unable to init filter %1(%2): %3",
                    name,
                    args,
                    nx::media::ffmpeg::avErrorToString(status));
                return (AVFilterContext*) nullptr;
            }
            if (previous && avfilter_link(previous, 0, filter, 0) < 0)
                return (AVFilterContext*) nullptr;
            previous = filter;
            return filter;
        };

        AVBufferSrcParameters* params = av_buffersrc_parameters_alloc();
        if (!params)
            return false;
        params->format = frame->format;
        params->width = frame->width;
        params->height = frame->height;
        params->time_base = AVRational{1, 1'000'000};
        params->sample_aspect_ratio = AVRational{1, 1};
        params->hw_frames_ctx = frame->hw_frames_ctx;
        channel.source = add({"buffer", ""}, params);
        av_free(params);
        if (!channel.source)
            return false;

        if (!frame->hw_frames_ctx && (!add({"format", "nv12"}) || !add({"hwupload", ""})))
            return false;

        if (QSize(frame->width, frame->height) != m_sourceSize && !add(scale(m_sourceSize)))
        {
            return false;
        }

        for (const auto& filter: m_filters)
        {
            if (!add(filter))
                return false;
        }

        channel.sink = add({"buffersink", ""});
        if (!channel.sink)
            return false;

        if (const int status = avfilter_graph_config(channel.graph, nullptr); status < 0)
        {
            NX_WARNING(this,
                "Unable to configure filter graph: %1",
                nx::media::ffmpeg::avErrorToString(status));
            return false;
        }

        channel.inputWidth = frame->width;
        channel.inputHeight = frame->height;
        channel.inputFormat = frame->format;
        channel.inputFrames = frame->hw_frames_ctx ? frame->hw_frames_ctx->data : nullptr;
        NX_DEBUG(this,
            "Filtering %1x%2 %3 frames on the device",
            frame->width,
            frame->height,
            frame->hw_frames_ctx ? "device" : "system memory");
        return true;
    }

private:
    AVBufferRef* m_device = nullptr;
    const QSize m_sourceSize;
    const std::vector<Filter> m_filters;
    std::map<int, Channel> m_channels;
};

VideoProcessor::VideoProcessor(const nx::core::transcoding::FilterChain& filters,
    const QSize& sourceSize,
    const QSize& targetSize,
    AVBufferRef* device):
    m_filters(filters),
    m_sourceSize(sourceSize)
{
    if (!device)
        return;

    // TODO: Run the filters that have no device counterpart in software between the device
    // ones, instead of the whole chain in software.
    auto vaapiFilters = toVaapi(filters, sourceSize, targetSize);
    if (!vaapiFilters)
    {
        NX_DEBUG(this, "Filters need software");
        return;
    }

    m_hardware = std::make_unique<HardwareGraph>(device, sourceSize, std::move(*vaapiFilters));
    if (!m_hardware->probe())
    {
        NX_WARNING(this, "Unable to filter on the device, filtering in software");
        m_hardware.reset();
    }
}

VideoProcessor::~VideoProcessor() = default;

CLVideoDecoderOutputPtr VideoProcessor::process(const CLVideoDecoderOutputPtr& frame)
{
    if (m_hardware)
    {
        if (auto result = m_hardware->process(frame))
            return result;
        NX_WARNING(this, "Filtering on the device failed, continuing in software");
        m_hardware.reset();
    }
    return processInSoftware(frame);
}

CLVideoDecoderOutputPtr VideoProcessor::processInSoftware(
    const CLVideoDecoderOutputPtr& frame) const
{
    auto result = frame->hw_frames_ctx ? frame->toSystemMemory() : frame;

    // Second stream must be upscaled to the first stream resolution before the filters apply.
    if (result && (result->size() != m_sourceSize || result->format != AV_PIX_FMT_YUV420P))
        result = result->scaled(m_sourceSize, AV_PIX_FMT_YUV420P);
    if (!result)
    {
        NX_ERROR(this, "Failed to bring video frame to %1 in system memory", m_sourceSize);
        return {};
    }
    return m_filters.apply(result);
}

} // namespace nx::transcoding
