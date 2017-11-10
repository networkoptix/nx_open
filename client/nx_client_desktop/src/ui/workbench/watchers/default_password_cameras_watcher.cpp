#include "default_password_cameras_watcher.h"

#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

namespace nx {
namespace client {
namespace desktop {

DefaultPasswordCamerasWatcher::DefaultPasswordCamerasWatcher(QObject* parent):
    Connective<QObject>(parent),
    base_type(parent)
{
    const auto setNotificationVisible =
        [this](bool value)
        {
            if (m_notificationIsVisible == value)
                return;

            m_notificationIsVisible = value;
            emit notificationIsVisibleChanged();
        };

    const auto resourceRemovedHandler =
        [this, setNotificationVisible](const QnResourcePtr& resource)
        {
            const auto resourceId = resource->getId().toQUuid();
            if (m_camerasWithDefaultPassword.remove(resourceId))
            {
                if (m_camerasWithDefaultPassword.isEmpty())
                    setNotificationVisible(false);
                emit camerasWithDefaultPasswordChanged();
            }
        };

    const auto updateResource=
        [this, resourceRemovedHandler, setNotificationVisible](const QnResourcePtr& resource)
        {
            const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
            if (!camera)
                return;

            if (!camera->needsToChangeDefaultPassword())
            {
                resourceRemovedHandler(resource);
                return;
            }

            const QUuid resourceId = resource->getId().toQUuid();
            if (m_camerasWithDefaultPassword.contains(resourceId))
                return;

            m_camerasWithDefaultPassword.insert(resourceId, camera);
            setNotificationVisible(true);
            emit camerasWithDefaultPasswordChanged();
        };

    const auto resourceAddedHandler =
        [this, updateResource](const QnResourcePtr& resource)
        {
            const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
            if (!camera)
                return;

            updateResource(resource);
            connect(camera.data(), &QnVirtualCameraResource::capabilitiesChanged,
                this, [resource, updateResource]() { updateResource(resource); });
        };

    connect(resourcePool(), &QnResourcePool::resourceAdded, this, resourceAddedHandler);
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this, resourceRemovedHandler);

    for (const auto& resource: resourcePool()->getResources<QnVirtualCameraResource>())
        updateResource(resource);
}

bool DefaultPasswordCamerasWatcher::notificationIsVisible() const
{
    return m_notificationIsVisible;
}

QnVirtualCameraResourceList DefaultPasswordCamerasWatcher::camerasWithDefaultPassword() const
{
    return m_camerasWithDefaultPassword.values();
}

} // namespace desktop
} // namespace client
} // namespace nx

