#pragma once

#include <memory>

#include <nx/network/aio/timer.h>
#include <nx/utils/thread/mutex.h>

#include <nx/vms/server/nvr/i_buzzer_manager.h>

namespace nx::utils { class TimerManager; }

namespace nx::vms::server::nvr::hanwha {

class IBuzzerPlatformAbstraction;

class BuzzerManager: public IBuzzerManager
{
public:
    BuzzerManager(std::unique_ptr<IBuzzerPlatformAbstraction> platformAbstraction);

    virtual ~BuzzerManager() override;

    virtual void start() override;

    virtual void stop() override;

    virtual bool setState(
        BuzzerState state,
        std::chrono::milliseconds duration = std::chrono::milliseconds::zero()) override;

private:
    BuzzerState calculateState() const;

private:
    nx::utils::Mutex m_mutex;

    std::unique_ptr<nx::utils::TimerManager> m_timerManager;
    std::unique_ptr<IBuzzerPlatformAbstraction> m_platformAbstraction;

    int m_enabledCounter = 0;

    bool m_isStopped = false;
};

} // namespace nx::vms::server::nvr::hanwha
