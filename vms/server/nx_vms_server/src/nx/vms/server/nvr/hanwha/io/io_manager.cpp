#include "io_manager.h"

#include <utils/common/synctime.h>

#include <nx/utils/timer_manager.h>

#include <nx/vms/server/nvr/hanwha/utils.h>
#include <nx/vms/server/nvr/hanwha/common.h>
#include <nx/vms/server/nvr/hanwha/io/io_state_fetcher.h>
#include <nx/vms/server/nvr/hanwha/io/i_io_platform_abstraction.h>

namespace nx::vms::server::nvr::hanwha {

using namespace std::chrono;

static QnIOStateDataList calculateChangedPortStates(
    const std::set<QnIOStateData>& oldState,
    const std::set<QnIOStateData>& newState)
{
    QnIOStateDataList result;
    for (const auto& newPortState: newState)
    {
        if (const auto it = oldState.find(newPortState);
            it == oldState.cend() || it->isActive != newPortState.isActive)
        {
            result.push_back(newPortState);
        }
    }

    return result;
}

static IoPortState invertedState(IoPortState state)
{
    return state == IoPortState::inactive ? IoPortState::active : IoPortState::inactive;
}

IoManager::IoManager(std::unique_ptr<IIoPlatformAbstraction> platformAbstraction):
    m_platformAbstraction(std::move(platformAbstraction)),
    m_stateFetcher(std::make_unique<IoStateFetcher>(
        m_platformAbstraction.get(),
        [this](const std::set<QnIOStateData>& states) { updatePortStates(states); })),
    m_timerManager(std::make_unique<nx::utils::TimerManager>())
{
    NX_DEBUG(this, "Creating the IO manager");
}

IoManager::~IoManager()
{
    NX_DEBUG(this, "Destroying the IO manager");

    m_timerManager->stop();
    m_stateFetcher->stop();
}

void IoManager::start()
{
    NX_DEBUG(this, "Starting the IO manager");

    for (int i = 0; i < kOutputCount; ++i)
    {
        const QString outputId = makeOutputId(i);
        m_platformAbstraction->setOutputPortState(outputId, IoPortState::inactive);
        m_stateFetcher->updateOutputPortState(outputId, IoPortState::inactive);
    }

    m_stateFetcher->start();
}

QnIOPortDataList IoManager::portDesriptiors() const
{
    return m_platformAbstraction->portDescriptors();
}

bool IoManager::setOutputPortState(
    const QString& portId,
    IoPortState state,
    std::chrono::milliseconds autoResetTimeout)
{
    NX_DEBUG(this, "Got request: portId: %1, state: %2, autoResetTimeout: %3",
        portId, state, autoResetTimeout);

    NX_MUTEX_LOCKER lock(&m_mutex);
    auto& context = m_outputPortContexts[portId];
    if (context.timerId != 0)
    {
        NX_DEBUG(this, "Canceling timer '%1'", context.timerId);
        m_timerManager->deleteTimer(context.timerId);
    }

    if (autoResetTimeout != std::chrono::milliseconds::zero())
    {
        context.timerId = m_timerManager->addTimer(
            [this, portId, state](nx::utils::TimerId timerId)
            {
                NX_MUTEX_LOCKER lock(&m_mutex);
                NX_DEBUG(this,
                    "Timer '%1' callback is called for port '%2', state: %3",
                    timerId, portId, invertedState(state));

                if (m_outputPortContexts[portId].timerId != timerId)
                {
                    NX_DEBUG(this, "The timer '%1' has been canceled, exiting", timerId);
                    return;
                }

                setOutputPortStateInternal(portId, invertedState(state));
            },
            autoResetTimeout);
    }

    return setOutputPortStateInternal(portId, state);
}

QnIOStateDataList IoManager::portStates() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    QnIOStateDataList result;
    for (const QnIOStateData& portState: m_lastPortStates)
        result.push_back(portState);

    NX_DEBUG(this, "Port states have been requested: %1", containerString(result));

    return result;
}

QnUuid IoManager::registerStateChangeHandler(IoStateChangeHandler handler)
{
    NX_MUTEX_LOCKER lock(&m_handlerMutex);
    const QnUuid id = getId(m_handlers);
    NX_DEBUG(this, "Registering handler with id %1", id);

    m_handlers.emplace(id, std::move(handler));
    return id;
}

void IoManager::unregisterStateChangeHandler(QnUuid handlerId)
{
    NX_DEBUG(this, "Unregistering handler with id %1", handlerId);

    NX_MUTEX_LOCKER lock(&m_handlerMutex);
    m_handlers.erase(handlerId);
}

void IoManager::updatePortStates(const std::set<QnIOStateData>& portStates)
{
    NX_VERBOSE(this,
        "Handling port states received from the state fetcher, %1",
        containerString(portStates));

    QnIOStateDataList changedPortStates;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (portStates == m_lastPortStates)
            return;

        changedPortStates = calculateChangedPortStates(m_lastPortStates, portStates);
        m_lastPortStates = portStates;
    }

    if (changedPortStates.empty())
        return;    

    NX_DEBUG(this, "%1 IO ports changed their state:", containerString(portStates));

    {
        NX_MUTEX_LOCKER lock(&m_handlerMutex);
        for (const auto& [_, handler]: m_handlers)
            handler(changedPortStates);
    }
}

bool IoManager::setOutputPortStateInternal(const QString& portId, IoPortState portState)
{
    if (portState == m_outputPortContexts[portId].currentState)
    {
        NX_DEBUG(this, "Current state of the output port '%1' is equal to requested: %2, ignoring",
            portId, portState);
        return true;
    }

    const bool success = m_platformAbstraction->setOutputPortState(portId, portState);
    if (success)
    {
        NX_DEBUG(this, "Succcesfully set output port '%1' state to %2", portId, portState);
        m_stateFetcher->updateOutputPortState(portId, portState);
    }
    else
    {
        NX_WARNING(this, "Unable to set output port '%1' state to %2", portId, portState);
    }

    return success;
}

} // namespace nx::vms::server::nvr::hanwha
