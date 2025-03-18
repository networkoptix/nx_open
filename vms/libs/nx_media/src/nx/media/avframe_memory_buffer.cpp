// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "avframe_memory_buffer.h"

namespace nx::media {

AvFrameMemoryBuffer::AvFrameMemoryBuffer(AVFrame* frame, bool ownsFrame):
    m_frame(frame),
    m_ownsFrame(ownsFrame)
{
}

AvFrameMemoryBuffer::~AvFrameMemoryBuffer()
{
    if (m_ownsFrame)
        av_frame_free(&m_frame);
}

AvFrameMemoryBuffer::MapData AvFrameMemoryBuffer::map(QVideoFrame::MapMode mode)
{
    MapData data;

    if (mode != QVideoFrame::ReadOnly)
        return data;

    const auto pixelFormat = toQtPixelFormat((AVPixelFormat)m_frame->format);

    int planes = 0;

    for (int i = 0; i < 4 && m_frame->data[i]; ++i)
    {
        ++planes;
        data.data[i] = m_frame->data[i];
        data.bytesPerLine[i] = m_frame->linesize[i];
        int bytesPerPlane = data.bytesPerLine[i] * m_frame->height;

        if (i > 0 && pixelFormat == QVideoFrameFormat::Format_YUV420P)
            bytesPerPlane /= 2;

        data.dataSize[i] = bytesPerPlane;
    }
    data.planeCount = planes;

    return data;
}

QVideoFrameFormat AvFrameMemoryBuffer::format() const
{
    if (!m_frame)
        return QVideoFrameFormat();

    return QVideoFrameFormat(
        QSize(m_frame->width, m_frame->height),
        toQtPixelFormat((AVPixelFormat)m_frame->format));
}

QVideoFrameFormat::PixelFormat AvFrameMemoryBuffer::toQtPixelFormat(AVPixelFormat pixFormat)
{
    switch (pixFormat)
    {
        case AV_PIX_FMT_YUV420P:
        case AV_PIX_FMT_YUVJ420P:
            return QVideoFrameFormat::Format_YUV420P;
        case AV_PIX_FMT_BGRA:
            return QVideoFrameFormat::Format_BGRA8888;
        case AV_PIX_FMT_NV12:
            return QVideoFrameFormat::Format_NV12;
        default:
            return QVideoFrameFormat::Format_Invalid;
    }
}

CLVideoDecoderOutputMemBuffer::CLVideoDecoderOutputMemBuffer(const CLVideoDecoderOutputPtr& frame):
    AvFrameMemoryBuffer(frame.data(), false),
    m_frame(frame)
{
}

} // namespace nx::media
