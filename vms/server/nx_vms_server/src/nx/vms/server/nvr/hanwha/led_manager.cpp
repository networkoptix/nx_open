#include "led_manager.h"

#include <media_server/media_server_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/vms/server/nvr/i_service.h>
#include <nx/vms/server/nvr/hanwha/common.h>

namespace nx::vms::server::nvr::hanwha {

static ILedController::LedState calculateRecordingLedState(
    const QnMediaServerResourcePtr& server,
    QnResourcePool* resourcePool)
{
    if (!NX_ASSERT(server))
        return ILedController::LedState::disabled;

    const auto devices = resourcePool->getAllCameras(server, /*ignoreDesktopCameras*/ true);
    for (const auto& device: devices)
    {
        if (device->getStatus() == Qn::ResourceStatus::Recording)
            return ILedController::LedState::enabled;
    }

    return ILedController::LedState::disabled;
}

static bool isPowerLimitExceeded(const nx::vms::api::NetworkBlockData& state)
{
    // TODO: #dmishin implement.
    return true;
}

LedManager::LedManager(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule)
{
}

LedManager::~LedManager()
{
    if (m_ioController && !m_ioControllerHandlerId.isNull())
        m_ioController->unregisterStateChangeHandler(m_ioControllerHandlerId);

    if (m_networkBlockController && !m_networkBlockControllerHandlerId.isNull())
        m_networkBlockController->unregisterStateChangeHandler(m_networkBlockControllerHandlerId);
}

void LedManager::start()
{
    nvr::IService* const nvrService = serverModule()->nvrService();
    if (!NX_ASSERT(nvrService))
        return;

    m_ledController = nvrService->ledController();
    if (!NX_ASSERT(m_ledController))
        return;

    m_networkBlockController = nvrService->networkBlockController();
    if (!NX_ASSERT(m_networkBlockController))
        return;

    m_ioController = nvrService->ioController();
    if (!NX_ASSERT(m_ioController))
        return;

    const auto resourcePool = serverModule()->resourcePool();
    connect(
        resourcePool, &QnResourcePool::statusChanged,
        this, &LedManager::at_resourceStatusChanged);

    m_networkBlockControllerHandlerId = m_networkBlockController->registerStateChangeHandler(
        [this](const nx::vms::api::NetworkBlockData& state) { handleNetworkBlockState(state); });

    storeOutputPortIds(m_ioController->portDesriptiors());

    m_ioControllerHandlerId = m_ioController->registerStateChangeHandler(
        [this](const QnIOStateDataList& state) { handleIoState(state); });
}

void LedManager::at_resourceStatusChanged(
    const QnResourcePtr& resource, Qn::StatusChangeReason /*reason*/)
{
    if (resource->getParentId() != serverModule()->commonModule()->moduleGUID())
        return;

    if (!NX_ASSERT(m_ledController))
        return;

    const ILedController::LedState state = calculateRecordingLedState(
        serverModule()->commonModule()->currentServer(),
        serverModule()->resourcePool());

    m_ledController->setState(kRecLedId, state);
}

void LedManager::handleNetworkBlockState(const nx::vms::api::NetworkBlockData& state)
{
    if (!NX_ASSERT(m_ledController))
        return;

    const auto ledState = isPowerLimitExceeded(state)
        ? ILedController::LedState::enabled
        : ILedController::LedState::disabled;

    m_ledController->setState(kPoeOverBudgetLedId, ledState);
}

void LedManager::handleIoState(const QnIOStateDataList& state)
{
    if (!NX_ASSERT(m_ledController))
        return;

    const ILedController::LedState ledState = calculateAlarmOutputLedState(state);
    m_ledController->setState(kAlarmOutLedId, ledState);
}

void LedManager::storeOutputPortIds(const QnIOPortDataList& portDescriptors)
{
    for (const QnIOPortData& portDescriptor: portDescriptors)
    {
        if (portDescriptor.portType == Qn::IOPortType::PT_Output)
            m_outputPortIds.insert(portDescriptor.id);
    }
}

bool LedManager::isOutputPort(const QString& portId) const
{
    return m_outputPortIds.find(portId) != m_outputPortIds.cend();
}

ILedController::LedState LedManager::calculateAlarmOutputLedState(
    const QnIOStateDataList& state) const
{
    for (const QnIOStateData& ioPortState: state)
    {
        if (isOutputPort(ioPortState.id) && ioPortState.isActive)
            return ILedController::LedState::enabled;
    }

    return ILedController::LedState::disabled;
}


} // namespace nx::vms::server::nvr::hanwha
