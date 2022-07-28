// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>
#include <vector>
#include <deque>
#include <mutex>
#include <cstdint>

namespace nx::media::nvidia {

class FrameQueue
{
public:
    ~FrameQueue();

    void configure(int widthInBytes, int bufferHeight);
    uint8_t* getFreeFrame();
    uint8_t* getNextFrame();
    void releaseFrame(uint8_t* frame);
    void clear();
    int getPitch() { return m_pitch; }

private:
    uint8_t* allocFrame();
    void freeFrame(uint8_t* frame);

private:
    size_t m_pitch;
    int m_widthInBytes;
    int m_bufferHeight;

    std::vector<uint8_t*> m_frames; //< keep them just for delete.
    std::deque<uint8_t*> m_readyFrames;
    std::deque<uint8_t*> m_emptyFrames;

    std::set<uint8_t*> m_otherSizeFrames; // Frames that allocated before buffer size changed.
    std::mutex m_mutex;

    int releaseCount = 0;
    int getCount = 0;
};

} // namespace nx::media::nvidia