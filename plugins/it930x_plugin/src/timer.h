#include <ctime>
#include <chrono>

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
    //!
    class Timer
    {
    public:
        typedef std::chrono::steady_clock Clock;
        //typedef ClockRDTSC Clock;

        Timer()
        :   m_finished(false)
        {
        }

        void reset(const std::chrono::milliseconds& duration)
        {
            m_stopAt = Clock::now() + duration;
            m_finished = false;
        }

        bool timeIsUp()
        {
            if (m_finished)
                return true;

            if (Clock::now() > m_stopAt)
            {
                m_finished = true;
                return true;
            }
            return false;
        }

    private:
        std::chrono::time_point<Clock> m_stopAt;
        bool m_finished;
    };
}
