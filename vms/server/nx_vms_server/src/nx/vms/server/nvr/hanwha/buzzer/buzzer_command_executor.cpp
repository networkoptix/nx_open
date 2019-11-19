#include "buzzer_command_executor.h"

#include <nx/vms/server/nvr/hanwha/buzzer/i_buzzer_platform_abstraction.h>

namespace nx::vms::server::nvr::hanwha {

BuzzerCommandExecutor::BuzzerCommandExecutor(IBuzzerPlatformAbstraction* platformAbstraction):
    m_platformAbstraction(platformAbstraction)
{
    setObjectName("BuzzerCommandExecutor");
    NX_DEBUG(this, "Creating the buzzer command executor");
}

BuzzerCommandExecutor::~BuzzerCommandExecutor()
{
    NX_DEBUG(this, "Destroying the buzzer command executor");
    stop();
}

void BuzzerCommandExecutor::setState(BuzzerState state)
{
    NX_DEBUG(this, "Got '%1 buzzer' command",
        (state == BuzzerState::enabled) ? "Enable" : "Disable");

    NX_MUTEX_LOCKER lock(&m_mutex);
    m_state = state;
    m_waitCondition.wakeOne();
}

void BuzzerCommandExecutor::stop()
{
    NX_DEBUG(this, "Stopping");
    NX_MUTEX_LOCKER lock(&m_mutex);
    pleaseStop();
    m_waitCondition.wakeOne();
    wait();

    m_platformAbstraction->setState(BuzzerState::disabled);
}

void BuzzerCommandExecutor::run()
{
    NX_DEBUG(this, "Starting the buzzer command executor");
    while(!needToStop())
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        while(!needToStop() && m_state == m_previousState)
            m_waitCondition.wait(&m_mutex);

        NX_DEBUG(this, "%1 the buzzer",
            (m_state == BuzzerState::enabled) ? "Enabling" : "Disabling");
        m_previousState = m_state;
        m_platformAbstraction->setState(m_state);
    }
}

} // namespace nx::vms::server::nvr::hanwha
