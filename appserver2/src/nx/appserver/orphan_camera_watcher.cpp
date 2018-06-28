#include "nx/appserver/orphan_camera_watcher.h"

#include <common/common_module.h>
#include <nx_ec/dummy_handler.h>
#include "ec2_connection.h"

#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>

#include <nx/kit/debug.h>

#include <algorithm>
#include <chrono>

namespace nx {
namespace appserver {

using namespace std::literals::chrono_literals;
static const std::chrono::milliseconds kDefaultUpdateInterval = 1min;
static const std::chrono::milliseconds kFirstUpdateInterval = 15s;

OrphanCameraWatcher::OrphanCameraWatcher(QnCommonModule* commonModule)
    :
    base_type(commonModule),
    m_updateInterval(kFirstUpdateInterval),
    m_updateIntervalType(UpdateIntervalType::shortInterval1)
{
    qRegisterMetaType<std::chrono::milliseconds>();

    connect(&m_timer, &QTimer::timeout, this, &OrphanCameraWatcher::update);
    //start();

    connect(this, &OrphanCameraWatcher::doChangeInterval, this,
        [&](std::chrono::milliseconds interval)
        {
            m_updateIntervalType = UpdateIntervalType::manualInterval;
            m_updateInterval = interval;
            m_timer.stop();
            this->start();
        },
        Qt::QueuedConnection);

    connect(this, &OrphanCameraWatcher::doStart, this,
        [&]()
        {
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
    int idx = 0;
    NX_DEBUG(this, "Detected camera list. Orphan cameras are marked with 'x'."
        " Format: Number * Camera Id | Model | Camera Url | Parent Id");
    for (const QnVirtualCameraResourcePtr &cam: cameraResourceList)
    {
        ++idx;
        QnUuid parentId = cam->getParentId();
        auto res = pool->getResourceById(parentId);
        if (res == nullptr)
        {
            QnUuid uuid = cam->getId();
            currentOrphanCameras.insert(uuid);
            NX_WARNING(this, lm("Orphan camera found. URL = %1. Parent id = %2.")
                .args(cam->sourceUrl(Qn::CR_LiveVideo), parentId.toString()));

            NX_DEBUG(this, lm("%01 x %2 | %3 | %4 | %5")
                .args(QString::number(idx).rightJustified(2, '0'),
                cam->getId().toString(), cam->getModel(), cam->getUrl(), parentId.toString()));
        }
        else
        {
            NX_DEBUG(this, lm("%1   %2 | %3 | %4 | %5")
                .args(QString::number(idx).rightJustified(2, '0'),
                cam->getId().toString(), cam->getModel(), cam->getUrl(), parentId.toString()));
        }
    }
    if (currentOrphanCameras.empty())
        NX_DEBUG(this, lm("No orphan cameras found."));
    else
        NX_DEBUG(this, lm("%1 orphan camera(s) found.").args(QString::number(currentOrphanCameras.size())));

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

    switch (m_updateIntervalType)
    {
        case UpdateIntervalType::manualInterval:
            break; // Do nothing.
        case UpdateIntervalType::shortInterval1:
            m_updateIntervalType = UpdateIntervalType::shortInterval2;
            break;
        case UpdateIntervalType::shortInterval2:
            m_updateInterval = kDefaultUpdateInterval;
            m_timer.setInterval(m_updateInterval.count());
            m_updateIntervalType = UpdateIntervalType::longInterval;
            break;
        case UpdateIntervalType::longInterval:
            break; // Do nothing.
        default:
            break; // Do nothing.
    }
}

void OrphanCameraWatcher::changeIntervalAsync(std::chrono::milliseconds interval)
{
    emit doChangeInterval(interval);
}

} // namespace appserver
} // namespace nx
