#include "helpers.h"

#include <nx/utils/timer_manager.h>

namespace nx::vms::server::metrics {

class Timer
{
public:
    Timer(std::chrono::milliseconds timeout, utils::metrics::OnChange change):
        m_timeout(timeout),
        m_change(std::move(change))
    {
    }

    void start()
    {
        m_guard = nx::utils::TimerManager::instance()->addTimerEx(
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
};

nx::utils::SharedGuardPtr makeTimer(
    std::chrono::milliseconds timeout,
    utils::metrics::OnChange change)
{
    auto timer = std::make_unique<Timer>(timeout, std::move(change));
    timer->start();
    return nx::utils::makeSharedGuard([timer = std::move(timer)] { timer->stop(); });
}

} // namespace nx::vms::server::metrics


