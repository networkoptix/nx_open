#include "io_command_executor.h"

#include <utils/common/synctime.h>

#include <nx/vms/server/nvr/hanwha/io/i_io_platform_abstraction.h>

namespace nx::vms::server::nvr::hanwha {

IoCommandExecutor::IoCommandExecutor(
    IIoPlatformAbstraction* platformAbstraction,
    IoStateChangeHandler stateChangeHandler)
    :
    m_platformAbstraction(platformAbstraction),
    m_stateChangeHandler(std::move(stateChangeHandler))
{
    NX_DEBUG(this, "Creating the IO command executor");
}

IoCommandExecutor::~IoCommandExecutor()
{
    NX_DEBUG(this, "Destroying the IO command executor");
    stop();
}

void IoCommandExecutor::setOutputPortState(const QString& portId, IoPortState state)
{
    NX_DEBUG(this, "Got '%1 output port \"%2\"' command",
        (state == IoPortState::active) ? "enable" : "disable",
        portId);

    NX_MUTEX_LOCKER lock(&m_mutex);
    m_portStates[portId].state = state;
    m_waitCondition.wakeOne();
}

void IoCommandExecutor::stop()
{
    NX_DEBUG(this, "Stopping");
    NX_MUTEX_LOCKER lock(&m_mutex);
    pleaseStop();
    m_waitCondition.wakeOne();
    wait();
}

void IoCommandExecutor::run()
{
    NX_DEBUG(this, "Starting the IO command executor");
    while(!needToStop())
    {
        QnIOStateDataList changedPortStates;
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            while(!needToStop() && !hasChangedPortStates())
                m_waitCondition.wait(&m_mutex);

            for (auto& [portId, portContext]: m_portStates)
            {
                if (portContext.state == portContext.previousState)
                    continue;

                NX_DEBUG(this, "%1 the '%2' output port",
                    (portContext.state == IoPortState::active) ? "Enabling" : "Disabling",
                    portId);

                portContext.previousState = portContext.state;
                m_platformAbstraction->setOutputPortState(portId, portContext.state);

                QnIOStateData portState;
                portState.id = portId;
                portState.isActive = portContext.state == IoPortState::active;
                portState.timestamp = qnSyncTime->currentMSecsSinceEpoch();

                changedPortStates.push_back(portState);
            }
        }

        if (!changedPortStates.empty())
            m_stateChangeHandler(changedPortStates);
    }
}

bool IoCommandExecutor::hasChangedPortStates() const
{
    for (const auto& [portId, portContext]: m_portStates)
    {
        if (portContext.state != portContext.previousState)
            return true;
    }

    return false;
}

} // namespace nx::vms::server::nvr::hanwha
