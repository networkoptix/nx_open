#include "ptz_availability_watcher.h"

#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/ptz/client_ptz_controller_pool.h>
#include <client_core/client_core_module.h>
#include <nx/utils/uuid.h>

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

    updateAvailability();
}

void PtzAvailabilityWatcher::updateAvailability()
{
    if (!m_camera)
    {
        setAvailable(false);
        return;
    }

    const auto cameraStatus = m_camera->getStatus();
    const bool correctStatus = cameraStatus == Qn::Online || cameraStatus == Qn::Recording;
    const auto controller = qnClientCoreModule->ptzControllerPool()->controller(m_camera);
    const auto server = m_camera->getParentServer();
    const bool validServerVersion = server && server->getVersion() >= QnSoftwareVersion(2, 6);

    setAvailable(kSupportMobilePtz && correctStatus && validServerVersion && !controller.isNull());
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

