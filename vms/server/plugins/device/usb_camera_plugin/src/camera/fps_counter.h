#pragma once

#include <atomic>
#include <chrono>

namespace nx::usb_cam {

struct FpsCounter
{
    void update(std::chrono::milliseconds now)
    {
        ++ m_updatingFps;
        if (now - m_prevUpdateTime >= std::chrono::seconds(1))
        {
            actualFps = m_updatingFps;
            m_updatingFps = 0;
            m_prevUpdateTime = now;
        }
    }
    std::atomic_int actualFps {0};

private:
    int m_updatingFps {0};
    std::chrono::milliseconds m_prevUpdateTime {0};
};

} // namespace nx::usb_cam
