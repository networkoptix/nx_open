// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "watermark_filter.h"

#include <nx/core/watermark/watermark_images.h>
#include <nx/media/ffmpeg/frame_info.h>

namespace nx::core::transcoding {

WatermarkImageFilter::WatermarkImageFilter(const Watermark& watermark): m_watermark(watermark)
{
}

CLVideoDecoderOutputPtr WatermarkImageFilter::updateImage(
    const CLVideoDecoderOutputPtr& frame,
    const QnAbstractCompressedMetadataPtr& metadata)
{
    const auto frameSize = QSize(frame->width, frame->height);
    if (frameSize != m_lastFrameSize)
    {
        setImage(retrieveWatermarkImage(m_watermark, frameSize).scaled(frameSize).toImage());
        m_lastFrameSize = frameSize;
    }

    return PaintImageFilter::updateImage(frame, metadata);
}

} // namespace nx::core::transcoding
