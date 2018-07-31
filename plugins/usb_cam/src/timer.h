#pragma once

#include <chrono>

namespace nx {
namespace usb_cam {
    
struct Timer
{
    typedef std::chrono::high_resolution_clock::time_point timepoint;
    timepoint startTime;
    timepoint stopTime;

    Timer()
    {
        startTime = stopTime = now();
    }

    timepoint now()
    {
        return std::chrono::high_resolution_clock::now();
    }

    void start()
    {
        startTime = now();
    }

    void stop()
    {
        stopTime = now();
    }

    std::chrono::nanoseconds elapsed()
    {
        return stopTime - startTime;
    }

    int64_t countMsec()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed()).count();
    }

    int64_t countUsec()
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(elapsed()).count();
    }

    int64_t countNsec()
    {
        return elapsed().count();
    }
};  

} // namespace usb_cam
} // namespace nx