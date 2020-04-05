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

static IoPortState translatePortState(IoPortState portState, Qn::IODefaultState circuitType)
{
    if (circuitType == Qn::IODefaultState::IO_OpenCircuit)
        return portState;

    return invertedState(portState);
}

static bool isActiveTranslated(bool isActive, Qn::IODefaultState circuitType)
{
    return circuitType == Qn::IODefaultState::IO_OpenCircuit ? isActive : !isActive;
}

IoManager::IoManager(std::unique_ptr<IIoPlatformAbstraction> platformAbstraction):
    m_platformAbstraction(std::move(platformAbstraction)),
    m_stateFetcher(std::make_unique<IoStateFetcher>(
        m_platformAbstraction.get(),
        [this](const std::set<QnIOStateData>& states) { updatePortStates(states); })),
    m_timerManager(std::make_unique<nx::utils::TimerManager>())
{
    NX_DEBUG(this, "Creating the IO manager");
    initialize();
}

IoManager::~IoManager()
{
    NX_DEBUG(this, "Destroying the IO manager");
    stop();
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

void IoManager::stop()
{
    if (m_isStopped)
        return;

    NX_DEBUG(this, "Stopping the IO manager");
    m_timerManager->stop();
    m_stateFetcher->stop();

    for (int i = 0; i < kOutputCount; ++i)
        m_platformAbstraction->setOutputPortState(makeOutputId(i), IoPortState::inactive);

    m_isStopped = true;
}

QnIOPortDataList IoManager::portDesriptiors() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    QnIOPortDataList rawDescriptors = m_platformAbstraction->portDescriptors();

    for(QnIOPortData& portDescriptor: rawDescriptors)
    {
        const Qn::IODefaultState circuitType = m_portContexts[portDescriptor.id].circuitType;
        if (portDescriptor.portType == Qn::IOPortType::PT_Input)
            portDescriptor.iDefaultState = circuitType;
        else if (portDescriptor.portType == Qn::IOPortType::PT_Output)
            portDescriptor.oDefaultState = circuitType;
    }

    return rawDescriptors;
}

void IoManager::setPortCircuitTypes(const std::map<QString, Qn::IODefaultState>& circuitTypeByPort)
{
    NX_DEBUG(this, "Updating port circuit types, %1", containerString(circuitTypeByPort));
    NX_MUTEX_LOCKER lock(&m_mutex);
    for (const auto& [portId, circuitType]: circuitTypeByPort)
    {
        IoPortContext& portContext = m_portContexts[portId];
        portContext.circuitType = circuitType;
        if (portContext.portType == Qn::IOPortType::PT_Output)
            setOutputPortStateInternal(portId, m_portContexts[portId].currentState);
    }
}

bool IoManager::setOutputPortState(
    const QString& portId,
    IoPortState state,
    std::chrono::milliseconds autoResetTimeout)
{
    NX_DEBUG(this, "Got request: portId: %1, state: %2, autoResetTimeout: %3",
        portId, state, autoResetTimeout);

    NX_MUTEX_LOCKER lock(&m_mutex);

    IoPortContext& context = m_portContexts[portId];
    if (context.timerId != 0)
    {
        NX_DEBUG(this, "Canceling timer '%1'", context.timerId);
        m_timerManager->deleteTimer(context.timerId);
    }

    if (autoResetTimeout != std::chrono::milliseconds::zero())
    {
        context.timerId = m_timerManager->addTimer(
            [this, portId](nx::utils::TimerId timerId)
            {
                NX_MUTEX_LOCKER lock(&m_mutex);
                NX_DEBUG(this, "Timer '%1' callback is called for port '%2'", timerId, portId);

                if (m_portContexts[portId].timerId != timerId)
                {
                    NX_DEBUG(this, "The timer '%1' has been canceled, exiting", timerId);
                    return;
                }

                setOutputPortStateInternal(portId, IoPortState::inactive);
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

HandlerId IoManager::registerStateChangeHandler(IoStateChangeHandler handler)
{
    NX_MUTEX_LOCKER lock(&m_handlerMutex);
    ++m_maxHandlerId;
    NX_DEBUG(this, "Registering handler with id %1", m_maxHandlerId);
    m_handlerContexts.emplace(
        m_maxHandlerId,
        HandlerContext{/*initialStateHasBeenReported*/ false, std::move(handler)});

    return m_maxHandlerId;
}

void IoManager::unregisterStateChangeHandler(HandlerId handlerId)
{
    NX_DEBUG(this, "Unregistering handler with id %1", handlerId);

    NX_MUTEX_LOCKER lock(&m_handlerMutex);
    m_handlerContexts.erase(handlerId);
}

void IoManager::initialize()
{
    const QnIOPortDataList descriptors = m_platformAbstraction->portDescriptors();
    for (const QnIOPortData& portDescriptor: descriptors)
    {
        m_portContexts[portDescriptor.id] = {
            /*timerId*/ 0,
            IoPortState::inactive,
            portDescriptor.portType == Qn::IOPortType::PT_Input
                ? portDescriptor.iDefaultState
                : portDescriptor.oDefaultState,
            portDescriptor.portType
        };

        m_lastPortStates.insert({
            portDescriptor.id,
            /*isActive*/ false,
            qnSyncTime->currentUSecsSinceEpoch()
        });
    }
}

void IoManager::updatePortStates(const std::set<QnIOStateData>& portStates)
{
    NX_VERBOSE(this,
        "Handling port states received from the state fetcher, %1",
        containerString(portStates));

    {
        NX_MUTEX_LOCKER lock(&m_handlerMutex);
        std::optional<QnIOStateDataList> currentState;
        for (auto& [_, handlerContext]: m_handlerContexts)
        {
            if (!handlerContext.intialStateHasBeenReported)
            {
                if (!currentState)
                {
                    currentState = QnIOStateDataList();
                    for (const QnIOStateData& portState: m_lastPortStates)
                        currentState->push_back(portState);
                }

                handlerContext.handler(*currentState);
                handlerContext.intialStateHasBeenReported = true;
            }
        }
    }

    QnIOStateDataList changedPortStates;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        std::set<QnIOStateData> translatedPortStates;
        for (QnIOStateData portState: portStates)
        {
            const IoPortContext& portContext = m_portContexts[portState.id];
            portState.isActive = isActiveTranslated(portState.isActive, portContext.circuitType);
            translatedPortStates.insert(std::move(portState));
        }

        if (translatedPortStates == m_lastPortStates)
            return;

        changedPortStates = calculateChangedPortStates(m_lastPortStates, translatedPortStates);
        m_lastPortStates = translatedPortStates;
    }

    if (changedPortStates.empty())
        return;

    NX_DEBUG(this, "IO ports changed their state: %1", containerString(portStates));

    {
        NX_MUTEX_LOCKER lock(&m_handlerMutex);
        for (const auto& [_, handlerContext]: m_handlerContexts)
            handlerContext.handler(changedPortStates);
    }
}

bool IoManager::setOutputPortStateInternal(const QString& portId, IoPortState portState)
{
    const IoPortContext& portContext = m_portContexts[portId];
    const IoPortState translatedPortState = translatePortState(portState, portContext.circuitType);

    const bool success = m_platformAbstraction->setOutputPortState(portId, translatedPortState);
    if (success)
    {
        NX_DEBUG(this, "Succcesfully set output port '%1' state to %2", portId, portState);
        m_stateFetcher->updateOutputPortState(portId, translatedPortState);
    }
    else
    {
        NX_WARNING(this, "Unable to set output port '%1' state to %2", portId, portState);
    }

    return success;
}

} // namespace nx::vms::server::nvr::hanwha
