#include "helpers.h"

namespace nx::vms::server::metrics {

class Timer
{
public:
    Timer(
        nx::utils::TimerManager* timerManager,
        std::chrono::milliseconds timeout, utils::metrics::OnChange change):
        m_timeout(timeout),
        m_change(std::move(change)),
        m_timerManager(timerManager)
    {
    }

    void start()
    {
        m_guard = m_timerManager->addTimerEx(
            [this](auto /*timeId*/)
            {
                m_change();
                if (!m_isStopped)
                    start();
            },
            m_timeout);
    }

    void stop()
    {
        m_isStopped = true;
        m_guard.reset();
    }

private:
    const std::chrono::milliseconds m_timeout;
    const utils::metrics::OnChange m_change;
    nx::utils::StandaloneTimerManager::TimerGuard m_guard;
    std::atomic<bool> m_isStopped{false};
    nx::utils::TimerManager* m_timerManager = nullptr;
};

// Used for tests to speedup all timers
static double m_multiplier = 1.0;

void setTimerMultiplier(int speedRate)
{
    m_multiplier = (double) speedRate;
}

nx::utils::SharedGuardPtr makeTimer(
    nx::utils::TimerManager* timerManager,
    std::chrono::milliseconds timeout,
    utils::metrics::OnChange change)
{
    if (timeout.count() > 0)
        timeout = std::chrono::milliseconds(std::max(1, (int)(timeout.count() / m_multiplier)));

    auto timer = std::make_unique<Timer>(timerManager, timeout, std::move(change));
    timer->start();
    return nx::utils::makeSharedGuard([timer = std::move(timer)] { timer->stop(); });
}

} // namespace nx::vms::server::metrics


