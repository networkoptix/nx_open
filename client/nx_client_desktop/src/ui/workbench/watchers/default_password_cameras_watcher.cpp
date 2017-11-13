#include "default_password_cameras_watcher.h"

#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

namespace nx {
namespace client {
namespace desktop {

struct DefaultPasswordCamerasWatcher::Private
{
    DefaultPasswordCamerasWatcher::Private(DefaultPasswordCamerasWatcher* owner):
        q(owner)
    {
    }

    void handleCameraChanged(const QnVirtualCameraResourcePtr& camera)
    {
        if (camera->needsToChangeDefaultPassword())
            camerasWithDefaultPassword.insert(camera);
        else
            camerasWithDefaultPassword.remove(camera);
        updateNotificationVisible();
    }

    void updateNotificationVisible()
    {
        setNotificationVisible(!camerasWithDefaultPassword.empty());
    }

    void setNotificationVisible(bool value)
    {
        if (notificationIsVisible == value)
            return;

        notificationIsVisible = value;
        emit q->notificationIsVisibleChanged();
    }

    DefaultPasswordCamerasWatcher* const q;
    bool notificationIsVisible = false;
    QSet<QnVirtualCameraResourcePtr> camerasWithDefaultPassword;
};


DefaultPasswordCamerasWatcher::DefaultPasswordCamerasWatcher(QObject* parent):
    base_type(parent),
    QnCommonModuleAware(parent),
    d(new Private(this))
{
    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        &DefaultPasswordCamerasWatcher::handleResourceAdded);
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        &DefaultPasswordCamerasWatcher::handleResourceRemoved);

    for (const auto& resource: resourcePool()->getResources())
        handleResourceAdded(resource);
}

DefaultPasswordCamerasWatcher::~DefaultPasswordCamerasWatcher()
{
}

bool DefaultPasswordCamerasWatcher::notificationIsVisible() const
{
    return d->notificationIsVisible;
}

QnVirtualCameraResourceList DefaultPasswordCamerasWatcher::camerasWithDefaultPassword() const
{
    return d->camerasWithDefaultPassword.values();
}

void DefaultPasswordCamerasWatcher::handleResourceAdded(const QnResourcePtr& resource)
{
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    connect(camera, &QnVirtualCameraResource::capabilitiesChanged, this,
        [this, camera](const QnResourcePtr& resource)
        {
            d->handleCameraChanged(camera);
        });

    d->handleCameraChanged(camera);
}

void DefaultPasswordCamerasWatcher::handleResourceRemoved(const QnResourcePtr& resource)
{
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    camera->disconnect(this);
    if (d->camerasWithDefaultPassword.remove(camera))
        d->updateNotificationVisible();
}

} // namespace desktop
} // namespace client
} // namespace nx

