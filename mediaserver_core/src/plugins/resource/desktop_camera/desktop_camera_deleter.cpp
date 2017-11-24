#include "desktop_camera_deleter.h"

#ifdef ENABLE_DESKTOP_CAMERA

#include <QtCore/QTimer>

#include <api/app_server_connection.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>

#include <nx_ec/ec_api.h>
#include <nx_ec/managers/abstract_layout_manager.h>
#include <nx_ec/managers/abstract_camera_manager.h>

namespace {

const int kDeletePeriodMs = 60 * 1000;    //check once a minute

} // namespace

QnDesktopCameraDeleter::QnDesktopCameraDeleter(QObject *parent):
    QObject(parent),
    QnCommonModuleAware(parent)
{
    auto timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]
        {
            deleteQueuedResources();
            updateQueue();
        });
    timer->start(kDeletePeriodMs);
}

void QnDesktopCameraDeleter::deleteQueuedResources()
{
    for (const auto& camera: m_queuedToDelete)
    {
        if (camera->getStatus() != Qn::Offline)
            continue;

        /* If the camera is placed on the layout, also remove the layout. */
        for (const auto& layout: resourcePool()->getLayoutsWithResource(camera->getId()))
        {
            commonModule()->ec2Connection()->getLayoutManager(Qn::kSystemAccess)->remove(
                layout->getId(), this, [] {});
        }

        commonModule()->ec2Connection()->getCameraManager(Qn::kSystemAccess)->remove(
            camera->getId(), this, [] {});
    }
    m_queuedToDelete.clear();
}

void QnDesktopCameraDeleter::updateQueue()
{
    const auto desktopCameras = resourcePool()->getResourcesWithFlag(Qn::desktop_camera);
    for (const auto& camera: desktopCameras)
    {
        if (camera->getStatus() == Qn::Offline)
            m_queuedToDelete << camera;
    }
}


#endif
