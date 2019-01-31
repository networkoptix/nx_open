#pragma once

#include <chrono>
#include <atomic>

namespace nx {
    ///
    class ElapsedTimer
    {
    public:
        typedef std::chrono::high_resolution_clock Clock;
        
        enum class State
        {
            Stopped,
            Started,
            Paused
        };

        ElapsedTimer(bool start = false):
            m_state(State::Stopped)
        {
            if (start)
                restart();
        }
        
        void stop() 
        { 
            m_state = State::Stopped;
        }

        State state() const { 
            return m_state; 
        }
        
        void suspend() 
        { 
            m_previousInterval = elapsedUS();
            m_state = State::Paused;
        }
        
        void resume() 
        {
            m_start = Clock::now();
            m_state = State::Started;
        }

        void restart()
        {
            m_previousInterval = 0;
            resume();
        }

        qint64 elapsedUS() const
        {
            if (m_state == State::Stopped)
                return 0;
            else if (m_state == State::Paused)
                return m_previousInterval;
            
            qint64 interval = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - m_start).count();
            return interval + m_previousInterval;
        }

    private:
        std::chrono::time_point<Clock> m_start;
        State m_state;
        qint64 m_previousInterval;
    };
} // namespace nx
