#include "buzzer_manager.h"

#include <nx/utils/log/log.h>
#include <nx/utils/timer_manager.h>

#include <nx/vms/server/nvr/hanwha/buzzer/i_buzzer_platform_abstraction.h>

namespace nx::vms::server::nvr::hanwha {

using namespace std::chrono;

BuzzerManager::BuzzerManager(std::unique_ptr<IBuzzerPlatformAbstraction> platformAbstraction):
    m_timerManager(std::make_unique<nx::utils::TimerManager>()),
    m_platformAbstraction(std::move(platformAbstraction))
{
    NX_DEBUG(this, "Creating the buzzer manager");
}

BuzzerManager::~BuzzerManager()
{
    NX_DEBUG(this, "Destroying the buzzer manager");
    m_timerManager->stop();
    m_platformAbstraction->setState(BuzzerState::disabled);
}

BuzzerState BuzzerManager::calculateState() const
{
    const BuzzerState state = m_enabledCounter ? BuzzerState::enabled : BuzzerState::disabled;
    NX_DEBUG(this, "Calculating buzzer state, forcibly enabled counter value: %1, result: %2",
        m_enabledCounter, state);

    return state;
}

void BuzzerManager::start()
{
    // TODO: #dmishin remove this method.
    NX_DEBUG(this, "Starting the buzzer manager");
}

bool BuzzerManager::setState(BuzzerState state, milliseconds duration)
{
    NX_DEBUG(this, "Got request: state: %1, duration: %2",
        (state == BuzzerState::enabled ? "enabled" : "disabled"),
        duration);

    NX_MUTEX_LOCKER lock(&m_mutex);
    if (state != BuzzerState::enabled)
    {
        --m_enabledCounter;
    }
    else if (duration == milliseconds::zero())
    {
        ++m_enabledCounter;
    }
    else
    {
        ++m_enabledCounter;
        m_timerManager->addTimer(
            [this](nx::utils::TimerId /*timerId*/)
            {
                NX_DEBUG(this, "Buzzer callback is called");
                NX_MUTEX_LOCKER lock(&m_mutex);
                --m_enabledCounter;
                m_platformAbstraction->setState(calculateState());
            },
            duration);
    }

    return m_platformAbstraction->setState(calculateState());
}

} // namespace nx::vms::server::nvr::hanwha
