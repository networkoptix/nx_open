// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef RPI_TIMER_H
#define RPI_TIMER_H

#include <chrono>
#include <mutex>

namespace rpi_cam
{
    /// PTS to timestamps converter
    class TimeCorrection
    {
    public:
        typedef std::chrono::high_resolution_clock Clock;

        static constexpr uint64_t MAX_DRIFT_US() { return 500 * 1000; }

        TimeCorrection()
        :   m_baseUsec(0),
            m_basePTS(0)
        {}

        uint64_t fixTime(uint64_t& ptsOld, uint64_t pts)
        {
            if (pts <= ptsOld || (pts - ptsOld) > MAX_DRIFT_US())
            {
                std::lock_guard<std::mutex> lock(m_mutex);

                m_baseUsec = usecNow();
                m_basePTS = pts;
            }

            ptsOld = pts;
            return m_baseUsec + (pts - m_basePTS);
        }

    private:
        mutable std::mutex m_mutex;
        uint64_t m_baseUsec;
        uint64_t m_basePTS;

        static uint64_t usecNow()
        {
            return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now().time_since_epoch()).count();
        }
    };
}

#endif
