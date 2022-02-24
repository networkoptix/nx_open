// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_pool_p.h"

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

QnResourcePool::Private::Private(QnResourcePool* owner):
    q(owner)
{
}

void QnResourcePool::Private::handleResourceAdded(const QnResourcePtr& resource)
{
    if (const auto networkResource = resource.dynamicCast<QnNetworkResource>())
        resourcesByPhysicalId.insert(networkResource->getPhysicalId(), networkResource);
    else if (const auto server = resource.dynamicCast<QnMediaServerResource>())
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
    if (const auto networkResource = resource.dynamicCast<QnNetworkResource>())
    {
        resourcesByPhysicalId.remove(networkResource->getPhysicalId());
    }
    else if (const auto server = resource.dynamicCast<QnMediaServerResource>())
    {
        mediaServers.remove(server);
    }
    else if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        camera->disconnect(q);
        ioModules.remove(camera);
    }
    hasIoModules = !ioModules.isEmpty();
}

void QnResourcePool::Private::updateIsIOModule(const QnVirtualCameraResourcePtr& camera)
{
    if (camera->isIOModule())
        ioModules.insert(camera);
    else
        ioModules.remove(camera);
    hasIoModules = !ioModules.isEmpty();
}
