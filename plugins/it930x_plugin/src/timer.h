#ifndef ITE_TIMER_H
#define ITE_TIMER_H

#include <chrono>
#include <thread>
#include <atomic>

#include "pts_to_clock_mapper.h"

#if 0
#include <ratio>

namespace
{
    struct ClockRDTSC
    {
        typedef unsigned long long                  rep;
        typedef std::ratio<1, 3300000000>           period; // CPU 3.3 GHz
        typedef std::chrono::duration<rep, period>  duration;
        typedef std::chrono::time_point<ClockRDTSC> time_point;
        static const bool is_steady =               true;

        static time_point now() noexcept
        {
            unsigned lo, hi;
            asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
            return time_point(duration(static_cast<rep>(hi) << 32 | lo));
        }
    };
}
#endif

namespace ite
{
    ///
    class Timer
    {
    public:
        typedef std::chrono::high_resolution_clock Clock;

        Timer(bool start = false)
        :   m_started(false)
        {
            if (start)
                restart();
        }

        void restart()
        {
            m_start = Clock::now();
            m_started = true;
        }

        void stop() { m_started = false; }
        bool isStarted() const { return m_started; }

        int64_t elapsedUS() const
        {
            return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - m_start).count();
        }

        int64_t elapsedMS() const
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - m_start).count();
        }

        static void sleep(unsigned msec)
        {
            std::chrono::milliseconds duration(msec);
            std::this_thread::sleep_for(duration);
        }

        static uint64_t usecNow()
        {
            return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now().time_since_epoch()).count();
        }

    private:
        std::chrono::time_point<Clock> m_start;
        std::atomic_bool m_started;

        Timer(const Timer&);
        const Timer& operator = (const Timer&);
    };

    ///
    class ContextTimer
    {
    public:
        ContextTimer(const char * msg = "", bool start = true)
        :   m_timer(start),
            m_msg(msg)
        {
        }

        ~ContextTimer()
        {
            if (isStarted())
                print();
        }

        void restart()
        {
            m_timer.restart();
        }

        bool isStarted() const { return m_timer.isStarted(); }

        void print()
        {
            printf("%s. Context timer %ld\n", m_msg, m_timer.elapsedUS());
        }

    private:
        Timer m_timer;
        const char * m_msg;
    };

    ///
    class PtsTime
    {
    public:
        PtsTime() :
            m_ptsMapper(90000, &m_syncData),
            m_allowedGap(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::milliseconds(150)
                ).count()
            )
        {}

        uint64_t pts2usec(PtsToClockMapper::pts_type pts)
        {
            auto curTimeStamp = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
            ).count();

            auto ret = m_ptsMapper.getTimestamp(pts);
#if 0
            printf("[Timer gap] real: %lu allowed: %lu\n", qAbs(ret - curTimeStamp), m_allowedGap);
#endif
            if (qAbs(ret - curTimeStamp) > m_allowedGap)
            {
#if 0
                printf("[Timer update] cur: %lu  ret: %lu", curTimeStamp, ret);
#endif
                m_ptsMapper.updateTimeMapping(
                    pts,
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::high_resolution_clock::now().time_since_epoch()
                    ).count()
                );
            }

            return ret;
        }

    private:
        PtsToClockMapper::TimeSynchronizationData   m_syncData;
        PtsToClockMapper                            m_ptsMapper;
        const int64_t                               m_allowedGap;
    };
}

#endif
