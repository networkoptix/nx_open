
#include "camera_names_watcher.h"

#include <core/resource/resource_name.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

utils::QnCameraNamesWatcher::QnCameraNamesWatcher(bool showCameraIp
    , QObject *parent)
    : base_type(parent)
    , m_showCameraIp(showCameraIp)
    , m_names()
{}

utils::QnCameraNamesWatcher::~QnCameraNamesWatcher()
{}

QString utils::QnCameraNamesWatcher::getCameraName(const QString &cameraUuid)
{
    const auto it = m_names.find(cameraUuid);
    if (it != m_names.end())
        return *it;

    const auto cameraResource = qnResPool->getResourceByUniqueId<QnVirtualCameraResource>(cameraUuid);
    if (!cameraResource)
        return lit("<%1>").arg(tr("Removed camera"));

    connect(qnResPool, &QnResourcePool::resourceRemoved, this
        , [this](const QnResourcePtr &resource)
    {
        const auto cameraResource = resource.dynamicCast<QnVirtualCameraResource>();
        if (!cameraResource)
            return;

        m_names.remove(cameraResource->getUniqueId());
        disconnect(cameraResource.data(), nullptr, this, nullptr);
    });

    connect(cameraResource.data(), &QnVirtualCameraResource::nameChanged, this
        , [this, cameraUuid](const QnResourcePtr &resource)
    {
        const auto newName = getFullResourceName(resource, m_showCameraIp);
        m_names[cameraUuid] = newName;
        emit cameraNameChanged(cameraUuid);
    });

    const auto cameraName = getFullResourceName(cameraResource, m_showCameraIp);
    m_names[cameraUuid] = cameraName;
    return cameraName;
}
