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

LedManager::LedManager(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule)
{
    const auto resourcePool = serverModule->resourcePool();
    connect(
        resourcePool, &QnResourcePool::statusChanged,
        this, &LedManager::at_resourceStatusChanged);

    nvr::IService* const nvrService = serverModule->nvrService();
    if (!NX_ASSERT(nvrService))
        return;

    nvr::INetworkBlockController* networkBlockController = nvrService->networkBlockController();
    if (!NX_ASSERT(networkBlockController))
        return;

    m_networkBlockControllerHandlerId = networkBlockController->registerStateChangeHandler(
        [this](const nx::vms::api::NetworkBlockData& state) { handleNetworkBlockState(state); });

    nvr::IIoController* ioController = nvrService->ioController();
    if (!NX_ASSERT(ioController))
        return;

    m_ioControllerHandlerId = ioController->registerStateChangeHandler(
        [this](const QnIOStateDataList& state) { handleIoState(state); });
}

LedManager::~LedManager()
{
    // TODO: #dmishin implement.
}

void LedManager::at_resourceStatusChanged(
    const QnResourcePtr& resource, Qn::StatusChangeReason /*reason*/)
{
    if (resource->getParentId() != serverModule()->commonModule()->moduleGUID())
        return;

    nvr::IService* const nvrService = serverModule()->nvrService();
    if (!NX_ASSERT(nvrService))
        return;

    nvr::ILedController* const ledController = nvrService->ledController();
    if (!NX_ASSERT(ledController))
        return;

    const ILedController::LedState state = calculateRecordingLedState(
        serverModule()->commonModule()->currentServer(),
        serverModule()->resourcePool());

    ledController->setState(kRecLedId, state);
}

void LedManager::handleNetworkBlockState(const nx::vms::api::NetworkBlockData& state)
{
    // TODO: #dmishin implement.
}

void LedManager::handleIoState(const QnIOStateDataList& state)
{
    // TODO: #dmishin implement.
}

} // namespace nx::vms::server::nvr::hanwha
