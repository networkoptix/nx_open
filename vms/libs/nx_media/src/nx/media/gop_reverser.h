// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>

#include <nx/media/media_fwd.h>
#include <nx/media/video_data_packet.h>

namespace nx::media {

class NX_MEDIA_API GopReverser
{
    class Queue
    {
    public:
        void push(VideoFramePtr frame);
        VideoFramePtr pop();
        void reset(int index);
        int64_t sizeBytes() const { return m_sizeBytes; }

        bool empty() const { return m_queue.empty(); }
        size_t size() const { return m_queue.size(); }
        auto begin() { return m_queue.begin(); }
        auto end() { return m_queue.end(); }
        VideoFramePtr& operator[](size_t index) { return m_queue[index]; }

        int actualSize() const { return m_actualSize; }
    private:
        std::deque<VideoFramePtr> m_queue;
        int64_t m_sizeBytes = 0;
        int m_actualSize = 0;
    };

public:
    GopReverser(int64_t maxBufferSizeBytes, int maxFrames);

    void push(VideoFramePtr decodedFrame);
    VideoFramePtr pop();

private:
    void removeOneElement();

private:
    const int64_t m_maxBufferSize;
    const int m_maxFrames;
    Queue m_queue;
    VideoFramePtr m_nextFrame;
};

} //namespace nx::media
