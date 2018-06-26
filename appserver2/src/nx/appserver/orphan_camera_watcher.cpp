#include "nx/appserver/orphan_camera_watcher.h"

#include <common/common_module.h>
#include <nx_ec/dummy_handler.h>
#include "ec2_connection.h"

#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>

#include <algorithm>
#include <chrono>

namespace nx {
namespace appserver {

using namespace std::literals::chrono_literals;
static const std::chrono::milliseconds kDefaultUpdateInterval = 15min;
static const std::chrono::milliseconds kFirstUpdateInterval = 5s;

OrphanCameraWatcher::OrphanCameraWatcher(QnCommonModule* commonModule)
    :
    base_type(commonModule),
    m_updateInterval(kFirstUpdateInterval),
    m_setDefaultUpdateInterval(true)
{
    qRegisterMetaType<std::chrono::milliseconds>();

    connect(&m_timer, &QTimer::timeout, this, &OrphanCameraWatcher::update);
    start();

    connect(this, &OrphanCameraWatcher::doChangeInterval, this,
        [&](std::chrono::milliseconds interval)
        {
            m_setDefaultUpdateInterval = false;
            m_updateInterval = interval;
            m_timer.stop();
            this->start();
        },
        Qt::QueuedConnection);
}

void OrphanCameraWatcher::start()
{
    update();
    m_timer.start(m_updateInterval.count());
}

void OrphanCameraWatcher::update()
{
    auto connectionPtr = commonModule()->ec2Connection();
    if (!connectionPtr)
        return;

    auto pool = commonModule()->resourcePool();
    if (!pool)
        return;

    QnVirtualCameraResourceList cameraResourceList = pool->getAllCameras(QnResourcePtr());

    Uuids currentOrphanCameras;
    for (const QnVirtualCameraResourcePtr &cam: cameraResourceList)
    {
        QnUuid parentId = cam->getParentId();
        auto res = pool->getResourceById(parentId);
        if (res == nullptr)
        {
            QnUuid uuid = cam->getId();
            currentOrphanCameras.insert(uuid);
            NX_WARNING(this, lm("Orphan camera found. URL = %1. Parent id = %2.").args(cam->sourceUrl(Qn::CR_LiveVideo), parentId.toString()));
        }
    }

    Uuids longlivedOrphanCameras;
    std::set_intersection(m_previousOrphanCameras.begin(), m_previousOrphanCameras.end(),
        currentOrphanCameras.begin(), currentOrphanCameras.end(),
        std::inserter(longlivedOrphanCameras, longlivedOrphanCameras.begin()));

    for (const QnUuid& CameraId: longlivedOrphanCameras)
    {
        ec2::AbstractCameraManagerPtr cameraManagerPtr = connectionPtr->getCameraManager(Qn::kSystemAccess);
        cameraManagerPtr->remove(CameraId, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
        currentOrphanCameras.erase(CameraId);
    }

    m_previousOrphanCameras = currentOrphanCameras;

    if (m_setDefaultUpdateInterval)
    {
        m_setDefaultUpdateInterval = false;
        m_updateInterval = kDefaultUpdateInterval;
        m_timer.setInterval(m_updateInterval.count());
    }
}

void OrphanCameraWatcher::changeIntervalAsync(std::chrono::milliseconds interval)
{
    emit doChangeInterval(interval);
}

} // namespace appserver
} // namespace nx
