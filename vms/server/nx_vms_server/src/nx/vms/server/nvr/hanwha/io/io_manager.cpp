#include "io_manager.h"

#include <utils/common/synctime.h>

#include <nx/network/aio/timer.h>

#include <nx/vms/server/nvr/hanwha/utils.h>
#include <nx/vms/server/nvr/hanwha/common.h>
#include <nx/vms/server/nvr/hanwha/io/io_state_fetcher.h>
#include <nx/vms/server/nvr/hanwha/io/io_command_executor.h>
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

IoManager::IoManager(std::unique_ptr<IIoPlatformAbstraction> platformAbstraction):
    m_platformAbstraction(std::move(platformAbstraction)),
    m_stateFetcher(std::make_unique<IoStateFetcher>(
        m_platformAbstraction.get(),
        [this](const std::set<QnIOStateData>& states) { handleInputPortStates(states); })),
    m_commandExecutor(std::make_unique<IoCommandExecutor>(
        m_platformAbstraction.get(),
        [this](const QnIOStateDataList& states) { handleOutputPortStates(states); })),
    m_timer(std::make_unique<nx::network::aio::Timer>())
{
}

IoManager::~IoManager()
{
    m_timer->cancelSync();
    m_stateFetcher->stop();
    m_commandExecutor->stop();
}

void IoManager::start()
{
    for (int i = 0; i < kOutputCount; ++i)
        m_platformAbstraction->setOutputPortState(makeOutputId(i), IoPortState::inactive);

    m_commandExecutor->start();
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
    if (state != IoPortState::active)
    {
        --m_outputPortStates[portId].forciblyEnabledCounter;
    }
    else if (autoResetTimeout == milliseconds::zero())
    {
        ++m_outputPortStates[portId].forciblyEnabledCounter;
    }
    else
    {
        ++m_outputPortStates[portId].enabledCounter;
        m_timer->start(
            autoResetTimeout,
            [this, portId]()
            {
                NX_MUTEX_LOCKER lock(&m_mutex);
                NX_DEBUG(this, "");
                -- m_outputPortStates[portId].enabledCounter;
                m_commandExecutor->setOutputPortState(portId, calculateOutputPortState(portId));
            });
    }

    m_commandExecutor->setOutputPortState(portId, calculateOutputPortState(portId));
    return true; //< TODO: #dmishin wait for response from the command executor?
}

QnIOStateDataList IoManager::portStates() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    QnIOStateDataList result;
    const qint64 timestampMs = qnSyncTime->currentMSecsSinceEpoch();
    for (int i = 0 ; i < kOutputCount; ++i)
    {
        const QString outputPortId = makeOutputId(i);

        QnIOStateData outputPortState;
        outputPortState.id = outputPortId;
        outputPortState.isActive = (calculateOutputPortState(outputPortId) == IoPortState::active);
        outputPortState.timestamp = timestampMs;

        result.push_back(std::move(outputPortState));
    }

    for (const QnIOStateData& inputPortState: m_lastInputPortStates)
        result.push_back(inputPortState);

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

void IoManager::handleInputPortStates(const std::set<QnIOStateData>& inputPortStates)
{
    NX_VERBOSE(this, "Handling input port state, %1", containerString(inputPortStates));

    QnIOStateDataList changedPortStates;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (inputPortStates == m_lastInputPortStates)
            return;

        changedPortStates = calculateChangedPortStates(m_lastInputPortStates, inputPortStates);
        m_lastInputPortStates = inputPortStates;
    }

    if (changedPortStates.empty())
        return;    

    NX_DEBUG(this, "%1 input ports changed their state:", containerString(inputPortStates));

    {
        NX_MUTEX_LOCKER lock(&m_handlerMutex);
        for (const auto& [_, handler]: m_handlers)
            handler(changedPortStates);
    }
}

void IoManager::handleOutputPortStates(const QnIOStateDataList& outputPortStates)
{
    NX_MUTEX_LOCKER lock(&m_handlerMutex);
    NX_DEBUG(this, "Handling output port states: %1", containerString(outputPortStates));
    for (const auto& [_, handler]: m_handlers)
        handler(outputPortStates);
}

IoPortState IoManager::calculateOutputPortState(const QString& portId) const
{
    const auto it = m_outputPortStates.find(portId);
    if (it == m_outputPortStates.cend())
        return IoPortState::inactive;

    const auto& portContext = it->second;
    return (portContext.enabledCounter > 0 || portContext.forciblyEnabledCounter > 0)
        ? IoPortState::active
        : IoPortState::inactive;
}

} // namespace nx::vms::server::nvr::hanwha
