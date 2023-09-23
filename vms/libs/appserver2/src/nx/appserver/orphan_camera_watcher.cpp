// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "orphan_camera_watcher.h"

#include <algorithm>
#include <chrono>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/kit/debug.h>
#include <nx/utils/log/log.h>
#include <nx/utils/metatypes.h>
#include <nx/vms/common/system_context.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_camera_manager.h>

namespace nx {
namespace appserver {

using namespace std::literals::chrono_literals;
static const std::chrono::milliseconds kDefaultUpdateInterval = 15min;

OrphanCameraWatcher::OrphanCameraWatcher(nx::vms::common::SystemContext* systemContext):
    SystemContextAware(systemContext),
    m_updateInterval(kDefaultUpdateInterval)
{
    qRegisterMetaType<std::chrono::milliseconds>();
    connect(&m_timer, &QTimer::timeout, this, &OrphanCameraWatcher::update);

    connect(this, &OrphanCameraWatcher::doChangeInterval, this,
        [&](std::chrono::milliseconds interval)
        {
            m_updateInterval = interval;
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

OrphanCameraWatcher::~OrphanCameraWatcher()
{
    stop();
}

void OrphanCameraWatcher::stop()
{
    m_timer.stop();
}

void OrphanCameraWatcher::start()
{
    m_timer.stop();
    update();
    m_timer.start(m_updateInterval.count());
}

void OrphanCameraWatcher::update()
{
    auto connectionPtr = messageBusConnection();
    if (!connectionPtr)
        return;

    auto pool = resourcePool();
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
            NX_DEBUG(this, nx::format("%01 x %2 | %3 | %4 | %5")
                .args(QString::number(idx).rightJustified(2, '0'),
                cam->getId().toString(), cam->getModel(), cam->getUrl(), parentId.toString()));
        }
        else
        {
            NX_DEBUG(this, nx::format("%1   %2 | %3 | %4 | %5")
                .args(QString::number(idx).rightJustified(2, '0'),
                cam->getId().toString(), cam->getModel(), cam->getUrl(), parentId.toString()));
        }
    }
    if (currentOrphanCameras.empty())
        NX_DEBUG(this, nx::format("No orphan cameras found."));
    else
        NX_DEBUG(this, nx::format("%1 orphan cameras found.").args(QString::number(currentOrphanCameras.size())));

    Uuids longlivedOrphanCameras;
    std::set_intersection(m_previousOrphanCameras.begin(), m_previousOrphanCameras.end(),
        currentOrphanCameras.begin(), currentOrphanCameras.end(),
        std::inserter(longlivedOrphanCameras, longlivedOrphanCameras.begin()));

    for (const QnUuid& CameraId: longlivedOrphanCameras)
    {
        ec2::AbstractCameraManagerPtr cameraManagerPtr = connectionPtr->getCameraManager(Qn::kSystemAccess);
        cameraManagerPtr->remove(CameraId, [](int /*requestId*/, ec2::ErrorCode) {});
        currentOrphanCameras.erase(CameraId);
    }

    m_previousOrphanCameras = currentOrphanCameras;
}

void OrphanCameraWatcher::changeIntervalAsync(std::chrono::milliseconds interval)
{
    emit doChangeInterval(interval);
}

} // namespace appserver
} // namespace nx
