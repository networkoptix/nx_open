// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QSize>

#include <nx/media/ffmpeg/frame_info.h>

struct AVBufferRef;

namespace nx::core::transcoding { class FilterChain; }

namespace nx::transcoding {

/**
 * Applies a prepared filter chain to decoded frames. Runs it on the hardware device when given
 * one, every filter in the chain has a hardware counterpart and the device processes a trial
 * frame; in software otherwise, and from the first failure on. Takes frames in system or video
 * memory; returns NV12 surfaces of the target size from the device, YUV420P frames from software.
 */
class NX_VMS_COMMON_API VideoProcessor
{
public:
    // `filters` must be prepared for `sourceSize` and outlive the processor.
    VideoProcessor(const nx::core::transcoding::FilterChain& filters,
        const QSize& sourceSize,
        const QSize& targetSize,
        AVBufferRef* device);
    ~VideoProcessor();

    CLVideoDecoderOutputPtr process(const CLVideoDecoderOutputPtr& frame);

    bool isHardware() const { return (bool) m_hardware; }

private:
    CLVideoDecoderOutputPtr processInSoftware(const CLVideoDecoderOutputPtr& frame) const;

private:
    class HardwareGraph;

    const nx::core::transcoding::FilterChain& m_filters;
    const QSize m_sourceSize;
    std::unique_ptr<HardwareGraph> m_hardware;
};

} // namespace nx::transcoding
