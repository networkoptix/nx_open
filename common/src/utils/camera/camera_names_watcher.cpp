#include "camera_names_watcher.h"

#include <core/resource/resource_display_info.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

utils::QnCameraNamesWatcher::QnCameraNamesWatcher(QObject* parent):
    base_type(parent),
    m_names()
{
}

utils::QnCameraNamesWatcher::~QnCameraNamesWatcher()
{
}

QString utils::QnCameraNamesWatcher::getCameraName(const QnUuid& cameraId)
{
    const auto it = m_names.find(cameraId);
    if (it != m_names.end())
        return *it;

    const auto cameraResource = qnResPool->getResourceById<QnVirtualCameraResource>(cameraId);
    if (!cameraResource)
        return lit("<%1>").arg(tr("Removed camera"));

    connect(qnResPool, &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            const auto cameraResource = resource.dynamicCast<QnVirtualCameraResource>();
            if (!cameraResource)
                return;

            m_names.remove(cameraResource->getId());
            disconnect(cameraResource.data(), nullptr, this, nullptr);
        });

    connect(cameraResource.data(), &QnVirtualCameraResource::nameChanged, this,
        [this, cameraId](const QnResourcePtr &resource)
        {
            const auto newName = QnResourceDisplayInfo(resource).toString(Qn::RI_NameOnly);
            m_names[cameraId] = newName;
            emit cameraNameChanged(cameraId);
        });

    const auto cameraName = QnResourceDisplayInfo(cameraResource).toString(Qn::RI_NameOnly);
    m_names[cameraId] = cameraName;
    return cameraName;
}
