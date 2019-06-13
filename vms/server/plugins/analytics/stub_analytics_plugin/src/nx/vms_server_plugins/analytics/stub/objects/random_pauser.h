#pragma once

#include <chrono>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

class RandomPauser
{
public:
    RandomPauser(
        const std::chrono::milliseconds& pauseDuration = std::chrono::seconds(1),
        int maxNumberOfPauses = 3,
        int randModulus = 9)
        :
        m_duration(pauseDuration),
        m_maxNumberOfPauses(maxNumberOfPauses),
        m_randModulus(randModulus)
    {
    }

    void update()
    {
        if (m_paused && std::chrono::high_resolution_clock::now() - m_pauseStartTime < m_duration)
            return;

        if (m_paused)
        {
            --m_maxNumberOfPauses;
            m_pauseStartTime = std::chrono::high_resolution_clock::time_point();
            m_resuming = true;
        }
        else if(m_resuming)
        {
            m_resuming = false;
        }

        m_paused = m_maxNumberOfPauses > 0
            ? rand() % m_randModulus == 0
            : false;

        if (m_paused)
            m_pauseStartTime = std::chrono::high_resolution_clock::now();
    }

    bool paused() const
    {
        return m_paused;
    }

    bool resuming() const
    {
        return m_resuming;
    }

private:
    const std::chrono::milliseconds m_duration;
    int m_maxNumberOfPauses;
    const int m_randModulus;
    bool m_paused = false;
    std::chrono::high_resolution_clock::time_point m_pauseStartTime;
    bool m_resuming = false;
};

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

