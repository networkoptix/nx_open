// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "gop_reverser.h"

#include "frame_metadata.h"

extern "C" {
#include <libavcodec/avcodec.h>
} // extern "C"

#include <nx/media/video_frame.h>

namespace nx::media {

namespace {

int getFrameBytes(const VideoFramePtr& frame)
{
    if (!frame)
        return 0;

    float bytesPerPixel;
    switch (frame->pixelFormat())
    {
        case QVideoFrameFormat::Format_YUV420P:
            [[fallthrough]];
        case QVideoFrameFormat::Format_NV12:
            bytesPerPixel = 1.5;
            break;

        case QVideoFrameFormat::Format_YUV422P:
            [[fallthrough]];
        case QVideoFrameFormat::Format_YUYV:
            bytesPerPixel = 2.0;
            break;

        default:
            bytesPerPixel = 4.0;
            break;
    }
    return frame->width() * frame->height() * bytesPerPixel;
}

} // namespace

// ------------- GopReverser::Queue -------------

void GopReverser::Queue::push(VideoFramePtr frame)
{
    m_sizeBytes += getFrameBytes(frame);
    m_queue.push_back(std::move(frame));
}

VideoFramePtr GopReverser::Queue::pop()
{
    auto result = std::move(m_queue.front());
    m_queue.pop_front();
    m_sizeBytes -= getFrameBytes(result);
    return result;
}

void GopReverser::Queue::reset(int index)
{
    m_sizeBytes -= getFrameBytes(m_queue[index]);
    m_queue[index].reset();
}

// ------------- GopReverser -------------

GopReverser::GopReverser(int64_t maxBufferSizeBytes):
    m_maxBufferSize(maxBufferSizeBytes)
{
}

void GopReverser::push(VideoFramePtr frame)
{
    FrameMetadata metadata = FrameMetadata::deserialize(frame);
    bool isStart = metadata.flags.testFlag(QnAbstractMediaData::MediaFlags_ReverseBlockStart);
    if (isStart && !m_queue.empty())
    {
        std::reverse(m_queue.begin(), m_queue.end());
        m_nextFrame = std::move(frame);
    }
    else
    {
        m_queue.push(std::move(frame));
        while (m_queue.sizeBytes() > m_maxBufferSize)
            removeOneElement();
    }
}

VideoFramePtr GopReverser::pop()
{
    while (!m_queue.empty() && m_nextFrame)
    {
        auto result = m_queue.pop();
        if (m_queue.empty() && m_nextFrame)
            m_queue.push(std::move(m_nextFrame));
        if (result)
            return result;
    }
    return VideoFramePtr();
}

void GopReverser::removeOneElement()
{
    std::pair<int, int> filled, empty; //< first - pos, second - count.

    // 1. Gather statistics with minimal empty and maximum non-empty elements in a row.
    for (int i = 0; i < (int) m_queue.size() - 1;)
    {
        const bool hasElement = (bool) m_queue[i];
        int j = i + 1;
        for (; j < (int) m_queue.size() && (bool) m_queue[j] == hasElement; ++j);
        const int count = j - i;
        if (hasElement && count > filled.second)
            filled = { i, count };
        else if (!hasElement && (count < empty.second || empty.second == 0))
            empty = { i, count };
        i = j;
    }
    // 2. Remove element.
    int pos = filled.second > 1 ? filled.first + filled.second / 2 : empty.first + empty.second;
    m_queue.reset(pos);
}

} //namespace nx::media
