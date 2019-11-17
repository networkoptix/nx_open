#pragma once

#include <memory>

#include <nx/network/aio/timer.h>

#include <nx/vms/server/nvr/i_buzzer_manager.h>

namespace nx::vms::server::nvr::hanwha {

class BuzzerCommandExecutor;
class IBuzzerPlatformAbstraction;

class BuzzerManager: public IBuzzerManager
{
public:
    BuzzerManager(std::unique_ptr<IBuzzerPlatformAbstraction> platformAbstraction);

    virtual ~BuzzerManager() override;

    virtual void start() override;

    virtual bool setState(
        BuzzerState state,
        std::chrono::milliseconds duration = std::chrono::milliseconds::zero()) override;

private:
    BuzzerState calculateState() const;

private:
    nx::utils::Mutex m_mutex;

    std::unique_ptr<nx::network::aio::Timer> m_timer;
    std::unique_ptr<IBuzzerPlatformAbstraction> m_platformAbstraction;
    std::unique_ptr<BuzzerCommandExecutor> m_commandExecutor;

    int m_enabledCounter = 0;
    int m_forciblyEnabledCounter = 0;
};

} // namespace nx::vms::server::nvr::hanwha
