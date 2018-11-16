#include "default_password_cameras_watcher.h"

#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

namespace nx::vms::client::desktop {

struct DefaultPasswordCamerasWatcher::Private
{
    explicit Private(DefaultPasswordCamerasWatcher* owner):
        q(owner)
    {
    }

    void handleCameraChanged(const QnVirtualCameraResourcePtr& camera)
    {
        const auto status = camera->getStatus();
        const bool canChangePassword = (status == Qn::Online) || (status == Qn::Recording);

        bool changed = false;
        if (canChangePassword && camera->needsToChangeDefaultPassword())
        {
            changed = !camerasWithDefaultPassword.contains(camera);
            camerasWithDefaultPassword.insert(camera);
        }
        else
        {
            changed = camerasWithDefaultPassword.remove(camera);
        }

        if (changed)
            emit q->cameraSetChanged();
    }

    DefaultPasswordCamerasWatcher* const q;
    QnVirtualCameraResourceSet camerasWithDefaultPassword;
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

QnVirtualCameraResourceSet DefaultPasswordCamerasWatcher::camerasWithDefaultPassword() const
{
    return d->camerasWithDefaultPassword;
}

void DefaultPasswordCamerasWatcher::handleResourceAdded(const QnResourcePtr& resource)
{
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    auto handleCameraChanged =
        [this, camera](const QnResourcePtr& resource)
        {
            d->handleCameraChanged(camera);
        };

    connect(camera, &QnVirtualCameraResource::statusChanged, this, handleCameraChanged);
    connect(camera, &QnVirtualCameraResource::capabilitiesChanged, this, handleCameraChanged);

    d->handleCameraChanged(camera);
}

void DefaultPasswordCamerasWatcher::handleResourceRemoved(const QnResourcePtr& resource)
{
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    camera->disconnect(this);
    d->camerasWithDefaultPassword.remove(camera);
}

} // namespace nx::vms::client::desktop

