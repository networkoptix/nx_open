#include "connector.h"

#include <chrono>
#include <algorithm>

#include <utils/common/synctime.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/vms/server/nvr/i_network_block_manager.h>
#include <nx/vms/server/nvr/i_io_manager.h>
#include <nx/vms/server/nvr/i_buzzer_manager.h>
#include <nx/vms/server/nvr/i_fan_manager.h>
#include <nx/vms/server/nvr/i_led_manager.h>

#include <nx/vms/server/nvr/hanwha/common.h>
#include <nx/vms/server/nvr/hanwha/utils.h>
#include <nx/vms/server/event/event_connector.h>
#include <nx/vms/event/events/poe_over_budget_event.h>
#include <nx/vms/event/events/fan_error_event.h>

namespace nx::vms::server::nvr::hanwha {

Connector::Connector(
    const QnMediaServerResourcePtr server,
    nx::vms::server::event::EventConnector* eventConnector,
    INetworkBlockManager* networkBlockManager,
    IIoManager* ioManager,
    IBuzzerManager* buzzerManager,
    IFanManager* fanManager,
    ILedManager* ledManager)
    :
    m_currentServer(server),
    m_networkBlockManager(networkBlockManager),
    m_ioManager(ioManager),
    m_buzzerManager(buzzerManager),
    m_fanManager(fanManager),
    m_ledManager(ledManager)
{
    NX_DEBUG(this, "Creating connector");

    NX_DEBUG(this, "Connecting to the event connector");
    connect(this, &Connector::poeOverBudget,
        eventConnector, &nx::vms::server::event::EventConnector::at_poeOverBudget,
        Qt::QueuedConnection);

    connect(this, &Connector::fanError,
        eventConnector, &nx::vms::server::event::EventConnector::at_fanError,
        Qt::QueuedConnection);

    QnResourcePool* resourcePool = m_currentServer->resourcePool();
    if (!NX_ASSERT(resourcePool))
    {
        NX_ERROR(this, "Unable to access the resource pool");
        return;
    }

    NX_DEBUG(this, "Connecting to the resource pool");
    connect(resourcePool, &QnResourcePool::statusChanged,
        this, &Connector::at_resourceStatusChanged,
        Qt::QueuedConnection);

    connect(resourcePool, &QnResourcePool::resourceAdded,
        this, &Connector::at_resourceAddedOrRemoved,
        Qt::QueuedConnection);

    connect(resourcePool, &QnResourcePool::resourceRemoved,
        this, &Connector::at_resourceAddedOrRemoved,
        Qt::QueuedConnection);

    NX_DEBUG(this, "Registering a PoE over budget handler");
    m_poeOverBudgetHandlerId = m_networkBlockManager->registerPoeOverBudgetHandler(
        [this](const nx::vms::api::NetworkBlockData& networkBlockData)
        {
            handlePoeOverBudgetStateChanged(networkBlockData);
        });

    NX_DEBUG(this, "Registering an alarm output handler");
    m_alarmOutputHandlerId = m_ioManager->registerStateChangeHandler(
        [this](const QnIOStateDataList& portStates) { handleIoPortStatesChanged(portStates); });

    NX_DEBUG(this, "Registering a fan alarm handler");
    m_fanAlarmHandlerId = m_fanManager->registerStateChangeHandler(
        [this](FanState state) { handleFanStateChange(state);} );
}

Connector::~Connector()
{
    NX_DEBUG(this, "Destroying connector");
    m_networkBlockManager->unregisterPoeOverBudgetHandler(m_poeOverBudgetHandlerId);
    m_ioManager->unregisterStateChangeHandler(m_alarmOutputHandlerId);
    m_fanManager->unregisterStateChangeHandler(m_fanAlarmHandlerId);

    m_ledManager->setLedState(kRecordingLedId, LedState::disabled);
    m_ledManager->setLedState(kAlarmOutputLedId, LedState::disabled);
    m_ledManager->setLedState(kPoeOverBudgetLedId, LedState::disabled);
}

void Connector::handlePoeOverBudgetStateChanged(
    const nx::vms::api::NetworkBlockData& networkBlockData)
{
    NX_DEBUG(this,
        "Handling PoE over budget, isInPoeOverBudgetMode: %1, currentConsumption: %2,"
        "lowerLimit: %3, upperLimit: %4",
        networkBlockData.isInPoeOverBudgetMode,
        calculateCurrentConsumptionWatts(networkBlockData.portStates),
        networkBlockData.lowerPowerLimitWatts,
        networkBlockData.upperPowerLimitWatts);

    m_ledManager->setLedState(
        kPoeOverBudgetLedId,
        networkBlockData.isInPoeOverBudgetMode ? LedState::enabled : LedState::disabled);

    const nx::vms::event::PoeOverBudgetEventPtr poeOverBudgetEvent(
        new nx::vms::event::PoeOverBudgetEvent(
            m_currentServer,
            networkBlockData.isInPoeOverBudgetMode
                ? nx::vms::event::EventState::active
                : nx::vms::event::EventState::inactive,
            std::chrono::milliseconds(qnSyncTime->currentUSecsSinceEpoch()),
            calculateCurrentConsumptionWatts(networkBlockData.portStates),
            networkBlockData.upperPowerLimitWatts,
            networkBlockData.lowerPowerLimitWatts));

    emit poeOverBudget(poeOverBudgetEvent);
}

void Connector::handleIoPortStatesChanged(const QnIOStateDataList& portStates)
{
    NX_DEBUG(this, "Handling IO port states change, port states: %1", containerString(portStates));

    for (const QnIOStateData& portState: portStates)
        m_alarmOutputStates[portState.id] = portState.isActive;

    const bool needToEnableAlarmOutputLed = std::any_of(
        m_alarmOutputStates.cbegin(),
        m_alarmOutputStates.cend(),
        [](const auto& portIdAndState) { return portIdAndState.second; });


    NX_DEBUG(this, "Some alarm outputs are on, enabling the alarm output LED");
    m_ledManager->setLedState(
        kAlarmOutputLedId,
        needToEnableAlarmOutputLed ? LedState::enabled : LedState::disabled);
}

void Connector::handleFanStateChange(FanState fanState)
{
    NX_DEBUG(this, "Handling fan state change: %1", fanState);
    if (fanState != FanState::error)
        return;

    const nx::vms::event::FanErrorEventPtr fanErrorEvent(
        new nx::vms::event::FanErrorEvent(
            m_currentServer,
            std::chrono::milliseconds(qnSyncTime->currentUSecsSinceEpoch())));

    emit fanError(fanErrorEvent);
}

void Connector::handleResourcePoolChanges()
{
    NX_DEBUG(this, "Handling resource pool changes");
    const QnResourcePool* const resourcePool = m_currentServer->resourcePool();
    if (!NX_ASSERT(resourcePool))
        return;

    const auto devices =
        resourcePool->getAllCameras(m_currentServer, /*ignoreDesktopCameras*/ true);

    for (const QnVirtualCameraResourcePtr& device: devices)
    {
        if (device->getStatus() == Qn::Recording)
        {
            NX_DEBUG(this,
                "Some resources on the server are in the recording state, "
                "enabling the recording LED");

            m_ledManager->setLedState(kRecordingLedId, LedState::enabled);
            return;
        }
    }

    NX_DEBUG(this,
        "There is no resources in the recording state on the server, disabling the recording LED");
    m_ledManager->setLedState(kRecordingLedId, LedState::disabled);
}

void Connector::at_resourceStatusChanged(
    const QnResourcePtr& /*resource*/, Qn::StatusChangeReason /*reason*/)
{
    handleResourcePoolChanges();
}

void Connector::at_resourceAddedOrRemoved(const QnResourcePtr& /*resource*/)
{
    handleResourcePoolChanges();
}

} // namespace nx::vms::server::nvr::hanwha
