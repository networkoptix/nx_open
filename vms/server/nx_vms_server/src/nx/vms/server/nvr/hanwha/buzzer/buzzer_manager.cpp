#include "buzzer_manager.h"

#include <nx/utils/log/log.h>

#include <nx/vms/server/nvr/hanwha/buzzer/i_buzzer_platform_abstraction.h>

#include <nx/vms/server/nvr/hanwha/buzzer/buzzer_command_executor.h>

namespace nx::vms::server::nvr::hanwha {

using namespace std::chrono;

BuzzerManager::BuzzerManager(std::unique_ptr<IBuzzerPlatformAbstraction> platformAbstraction):
    m_timer(std::make_unique<nx::network::aio::Timer>()),
    m_platformAbstraction(std::move(platformAbstraction)),
    m_commandExecutor(std::make_unique<BuzzerCommandExecutor>(m_platformAbstraction.get()))
{
    NX_DEBUG(this, "Creating the buzzer manager");
}

BuzzerManager::~BuzzerManager()
{
    NX_DEBUG(this, "Destroying the buzzer manager");
    m_timer->cancelSync();
    m_commandExecutor->stop();
    m_platformAbstraction->setState(BuzzerState::disabled);
}

BuzzerState BuzzerManager::calculateState() const
{
    const BuzzerState state = (m_forciblyEnabledCounter || m_enabledCounter)
        ? BuzzerState::enabled
        : BuzzerState::disabled;

    NX_DEBUG(this, "Calculating buzzer state, forcibly enabled counter value: %1, "
        "enabled counter value: %2, result: %3",
        m_forciblyEnabledCounter, m_enabledCounter, state);

    return state;
}

void BuzzerManager::start()
{
    NX_DEBUG(this, "Starting the buzzer manager");
    m_commandExecutor->start();
}

bool BuzzerManager::setState(BuzzerState state, milliseconds duration)
{
    NX_DEBUG(this, "Got request: state: %1, duration: %2",
        (state == BuzzerState::enabled ? "enabled" : "disabled"),
        duration);

    NX_MUTEX_LOCKER lock(&m_mutex);
    if (state != BuzzerState::enabled)
    {
        --m_forciblyEnabledCounter;
    }
    else if (duration == milliseconds::zero())
    {
        ++m_forciblyEnabledCounter;
    }
    else
    {
        ++m_enabledCounter;
        m_timer->start(
            duration,
            [this]()
            {
                NX_MUTEX_LOCKER lock(&m_mutex);
                NX_DEBUG(this, "");
                --m_enabledCounter;
                m_commandExecutor->setState(calculateState());
            });
    }

    m_commandExecutor->setState(calculateState());
    return true; //< TODO: #dmishin wait for the command executor response?
}

} // namespace nx::vms::server::nvr::hanwha
