#pragma once

namespace nx {
namespace rpi_cam2 {
namespace utils {
    
struct TimeProfiler
{
    typedef std::chrono::high_resolution_clock::time_point timepoint;
    timepoint startTime;
    timepoint stopTime;

public:
    TimeProfiler()
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

} // namespace utils
} // namespace rpi_cam2
} // namespace nx