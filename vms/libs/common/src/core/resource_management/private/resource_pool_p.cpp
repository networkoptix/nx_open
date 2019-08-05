#include "resource_pool_p.h"

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

QnResourcePool::Private::Private(QnResourcePool* owner):
    q(owner)
{
}

void QnResourcePool::Private::handleResourceAdded(const QnResourcePtr& resource)
{
    resourcesByUniqueId.insert(resource->getUniqueId(), resource);

    if (const auto server = resource.dynamicCast<QnMediaServerResource>())
    {
        mediaServers.insert(server);
    }
    else if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        QObject::connect(camera.data(), &QnVirtualCameraResource::isIOModuleChanged, q,
            [this, camera]() { updateIsIOModule(camera); });

        updateIsIOModule(camera);
    }
}

void QnResourcePool::Private::handleResourceRemoved(const QnResourcePtr& resource)
{
    resourcesByUniqueId.remove(resource->getUniqueId());

    if (const auto server = resource.dynamicCast<QnMediaServerResource>())
    {
        mediaServers.remove(server);
    }
    else if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        camera->disconnect(q);
        ioModules.remove(camera);
    }
}

void QnResourcePool::Private::updateIsIOModule(const QnVirtualCameraResourcePtr& camera)
{
    if (camera->isIOModule())
        ioModules.insert(camera);
    else
        ioModules.remove(camera);
}
