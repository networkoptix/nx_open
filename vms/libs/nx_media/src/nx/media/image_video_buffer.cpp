// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "image_video_buffer.h"

#include <nx/media/video_frame.h>
#include <nx/utils/log/assert.h>

namespace nx::media {

QAbstractVideoBuffer::MapData ImageVideoBuffer::map(QVideoFrame::MapMode mode)
{
    MapData data;
    if (mode != QVideoFrame::ReadOnly)
        return data;
    m_mapMode = mode;

    data.nPlanes = 1;
    data.data[0] = const_cast<uchar *>(m_image.constBits());
    data.bytesPerLine[0] = m_image.bytesPerLine();
    data.size[0] = m_image.sizeInBytes();

    return data;
}

void ImageVideoBuffer::unmap()
{
    m_mapMode = QVideoFrame::NotMapped;
}

VideoFramePtr videoFrameFromImage(const QImage& image)
{
    const auto pixelFormat = QVideoFrameFormat::pixelFormatFromImageFormat(image.format());
    if (!NX_ASSERT(pixelFormat != QVideoFrameFormat::Format_Invalid))
        return {};

    const QVideoFrameFormat format(image.size(), pixelFormat);
    return std::make_shared<VideoFrame>(new ImageVideoBuffer(image), format);
}

} // namespace nx::media
