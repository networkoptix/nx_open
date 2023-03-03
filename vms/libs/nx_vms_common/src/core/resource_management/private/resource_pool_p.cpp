// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_pool_p.h"

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>

QnResourcePool::Private::Private(
    QnResourcePool* owner,
    nx::vms::common::SystemContext* systemContext)
    :
    q(owner),
    systemContext(systemContext)
{
}

void QnResourcePool::Private::handleResourceAdded(const QnResourcePtr& resource)
{
    if (const auto server = resource.dynamicCast<QnMediaServerResource>())
    {
        mediaServers.insert(server);
    }
    else if (const auto storage = resource.dynamicCast<QnStorageResource>())
    {
        storages.insert(storage);
    }
    else if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        resourcesByPhysicalId.insert(camera->getPhysicalId(), camera);
        QObject::connect(camera.data(), &QnVirtualCameraResource::isIOModuleChanged, q,
            [this, camera]() { updateIsIOModule(camera); });

        updateIsIOModule(camera);
    }
}

void QnResourcePool::Private::handleResourceRemoved(const QnResourcePtr& resource)
{
    if (const auto server = resource.dynamicCast<QnMediaServerResource>())
    {
        mediaServers.remove(server);
    }
    else if (const auto storage = resource.dynamicCast<QnStorageResource>())
    {
        storages.remove(storage);
    }
    else if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        resourcesByPhysicalId.remove(camera->getPhysicalId());
        camera->disconnect(q);
        ioModules.remove(camera);
        hasIoModules = !ioModules.isEmpty();
    }
}

void QnResourcePool::Private::updateIsIOModule(const QnVirtualCameraResourcePtr& camera)
{
    if (camera->isIOModule())
        ioModules.insert(camera);
    else
        ioModules.remove(camera);
    hasIoModules = !ioModules.isEmpty();
}
