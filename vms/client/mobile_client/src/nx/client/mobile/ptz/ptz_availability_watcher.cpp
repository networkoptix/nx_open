#include "ptz_availability_watcher.h"

#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/ptz/client_ptz_controller_pool.h>
#include <client_core/client_core_module.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/uuid.h>
#include <nx/client/core/watchers/user_watcher.h>

namespace {

#if defined(NO_MOBILE_PTZ_SUPPORT)
    static constexpr bool kSupportMobilePtz = false;
#else
    static constexpr bool kSupportMobilePtz = true;
#endif

} // namespace

namespace nx {
namespace client {
namespace mobile {

PtzAvailabilityWatcher::PtzAvailabilityWatcher(QObject* parent):
    base_type(parent)
{
    const auto userWatcher = commonModule()->instance<nx::vms::client::core::UserWatcher>();
    connect(userWatcher, &nx::vms::client::core::UserWatcher::userChanged,
        this, &PtzAvailabilityWatcher::updateAvailability);

    const auto manager = globalPermissionsManager();
    connect(manager, &QnGlobalPermissionsManager::globalPermissionsChanged, this,
        [this, userWatcher]
            (const QnResourceAccessSubject& subject, GlobalPermissions /*permissions*/)
        {
            const auto user = userWatcher->user();
            if (subject != user)
                return;

            updateAvailability();
        });
}

void PtzAvailabilityWatcher::setResourceId(const QnUuid& uuid)
{
    if (m_camera)
        m_camera->disconnect(this);

    const auto resourcePool = qnClientCoreModule->commonModule()->resourcePool();
    m_camera = resourcePool->getResourceById<QnVirtualCameraResource>(uuid);
    if (!m_camera)
    {
        setAvailable(false);
        return;
    }

    connect(m_camera, &QnVirtualCameraResource::statusChanged,
        this, &PtzAvailabilityWatcher::updateAvailability);
    connect(m_camera, &QnVirtualCameraResource::mediaDewarpingParamsChanged,
        this, &PtzAvailabilityWatcher::updateAvailability);

    updateAvailability();
}

void PtzAvailabilityWatcher::updateAvailability()
{
    auto nonAvailableSetter = nx::utils::makeScopeGuard(
        [this]() { setAvailable(false); });

    if (!m_camera)
        return;

    const auto userWatcher = commonModule()->instance<nx::vms::client::core::UserWatcher>();
    const auto user = userWatcher->user();
    if (!user || !globalPermissionsManager()->hasGlobalPermission(user, GlobalPermission::userInput))
        return;

    const auto cameraStatus = m_camera->getStatus();
    const bool correctStatus = cameraStatus == Qn::Online || cameraStatus == Qn::Recording;
    if (!correctStatus)
        return;

    if (!qnClientCoreModule->ptzControllerPool()->controller(m_camera))
        return;

    const auto server = m_camera->getParentServer();
    const bool wrongServerVersion = !server
        || server->getVersion() < nx::utils::SoftwareVersion(2, 6);
    if (wrongServerVersion)
        return;

    if (m_camera->getDewarpingParams().enabled)
        return;

    nonAvailableSetter.disarm();
    setAvailable(true);
}

bool PtzAvailabilityWatcher::available() const
{
    return m_available;
}

void PtzAvailabilityWatcher::setAvailable(bool value)
{
    if (m_available == value)
        return;

    m_available = value;
    emit availabilityChanged();
}

} // namespace mobile
} // namespace client
} // namespace nx

