// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "tiled_image_filter.h"

#include <utils/common/util.h>
#include <utils/media/frame_info.h>

#include "crop_image_filter.h"

QnTiledImageFilter::QnTiledImageFilter(const QnConstResourceVideoLayoutPtr& videoLayout):
    m_layout(videoLayout)
{
}

CLVideoDecoderOutputPtr QnTiledImageFilter::updateImage(
    const CLVideoDecoderOutputPtr& frame,
    const QnAbstractCompressedMetadataPtr& metadata)
{
    if (m_layout->size().width() == 1 && m_layout->size().height() == 1)
        return frame;

    const QSize size(frame->width, frame->height);
    if (!m_tiledFrame || size != m_size)
    {
        m_size = size;
        const QSize newSize(frame->width * m_layout->size().width(),
            frame->height * m_layout->size().height());
        if (m_tiledFrame)
            m_tiledFrame = m_tiledFrame->scaled(newSize, static_cast<AVPixelFormat>(frame->format));

        // Handling first channel of the camera. Also CLVideoDecoderOutput::scaled() function can
        // return nullptr in case of error, handling it here.
        if (!m_tiledFrame)
        {
            m_tiledFrame = CLVideoDecoderOutputPtr(new CLVideoDecoderOutput());
            m_tiledFrame->reallocate(newSize, frame->format);
            m_tiledFrame->memZero();
        }
    }
    m_tiledFrame->assignMiscData(frame.data());
    m_prevFrameTime = m_tiledFrame->pts = qMax(static_cast<qint64>(m_tiledFrame->pts),
        m_prevFrameTime + MIN_FRAME_DURATION_USEC);

    const auto pos = m_layout->position(frame->channel);
    const QRect rect(pos.x() * m_size.width(), pos.y() * m_size.height(),
        m_size.width(), m_size.height());
    auto croppedFrame = QnCropImageFilter(rect).updateImage(m_tiledFrame, metadata);
    croppedFrame->copyFrom(frame.data());

    // Make sure overlays will be painter on frame copy, not on the combined frame itself.
    auto result = CLVideoDecoderOutputPtr(new CLVideoDecoderOutput());
    result->copyFrom(m_tiledFrame.data());
    return result;
}

QSize QnTiledImageFilter::updatedResolution(const QSize& srcSize)
{
    return QSize(srcSize.width() * m_layout->size().width(),
        srcSize.height() * m_layout->size().height());
}
