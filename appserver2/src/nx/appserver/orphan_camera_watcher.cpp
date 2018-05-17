#include "nx/appserver/orphan_camera_watcher.h"

#include <common/common_module.h>
#include <nx_ec/dummy_handler.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>

#include <algorithm>

namespace nx {
namespace appserver {

static const int kDefaultUpdateIntervalMs = 1000 * 60 * 30;

OrphanCameraWatcher::OrphanCameraWatcher(QnCommonModule* commonModule):
    base_type(commonModule), m_updateIntervalMs(kDefaultUpdateIntervalMs)
{
    connect(&m_timer, &QTimer::timeout, this, &OrphanCameraWatcher::update);
    start();

    connect(this, &OrphanCameraWatcher::doChangeInterval, this,  
        [&](int ms) 
        {
            m_updateIntervalMs = ms;
            m_timer.stop();
            this->start();
        },
        Qt::QueuedConnection);
}

void OrphanCameraWatcher::start()
{
    update();
    m_timer.start(m_updateIntervalMs);
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
            NX_WARNING(this, lm("Orphan camera found. URL = 1%. Parent id = %2.").args(cam->sourceUrl(Qn::CR_LiveVideo), parentId));
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
}

void OrphanCameraWatcher::changeIntervalAsync(int ms)
{
    emit doChangeInterval(ms);
}

} // namespace appserver
} // namespace nx
