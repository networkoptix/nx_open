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
    size_t m_pitch = 0;
    int m_widthInBytes = 0;
    int m_bufferHeight = 0;

    std::vector<uint8_t*> m_frames; //< Keep them just for deleting.
    std::deque<uint8_t*> m_readyFrames;
    std::deque<uint8_t*> m_emptyFrames;

    std::set<uint8_t*> m_otherSizeFrames; // Frames allocated before the buffer size had changed.
    std::mutex m_mutex;
};

} // namespace nx::media::nvidia
