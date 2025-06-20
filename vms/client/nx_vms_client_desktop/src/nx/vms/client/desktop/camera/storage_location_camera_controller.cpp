// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_location_camera_controller.h"

#include <ranges>

#include <QtCore/QList>

#include <camera/resource_display.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/streaming/abstract_archive_stream_reader.h>
#include <nx/vms/client/core/camera/camera_data_manager.h>
#include <nx/vms/client/desktop/system_context.h>

using StorageLocation = nx::vms::api::StorageLocation;

namespace nx::vms::client::desktop {

struct StorageLocationCameraController::Private
{
    StorageLocation storageLocation = StorageLocation::both;
    QList<QnResourceDisplayPtr> consumers;
    std::multiset<QnMediaServerResourcePtr> servers;

    void update(QnResourceDisplayPtr consumer)
    {
        if (auto reader = consumer->archiveReader())
            reader->setStorageLocationFilter(storageLocation);
    }
};

StorageLocationCameraController::StorageLocationCameraController(
    SystemContext* systemContext,
    QObject* parent)
    :
    base_type(parent),
    SystemContextAware(systemContext),
    d(new Private())
{
    connect(resourcePool(), &QnResourcePool::resourcesRemoved, this,
        [this](const QnResourceList& resources)
        {
           for (const auto& resource: resources)
           {
               if (const auto server = resource.dynamicCast<QnMediaServerResource>())
                   d->servers.erase(server);
           }
        });
}

StorageLocationCameraController::~StorageLocationCameraController()
{
}

StorageLocation StorageLocationCameraController::storageLocation() const
{
    return d->storageLocation;
}

void StorageLocationCameraController::setStorageLocation(StorageLocation value)
{
    if (d->storageLocation == value)
        return;

    d->storageLocation = value;
    for (auto consumer: d->consumers)
        d->update(consumer);
}

void StorageLocationCameraController::updateStorageLocationByConsumers()
{
    if (storageLocation() == StorageLocation::backup)
    {
        bool backupEnabled = std::ranges::any_of(d->servers,
            [](const auto& server) { return server->hasActiveBackupStorages(); });
        if (!backupEnabled)
        {
            setStorageLocation(StorageLocation::both);
            systemContext()->cameraDataManager()->setStorageLocation(StorageLocation::both);
        }
    }
}

void StorageLocationCameraController::registerConsumer(QnResourceDisplayPtr display)
{
    d->consumers.push_back(display);
    d->update(display); //< Should be updated before potential updateStorageLocationByConsumers() call.

    if (auto server = display->resource()->getParentResource().dynamicCast<QnMediaServerResource>())
    {
        const bool needUpdate = !d->servers.contains(server);
        d->servers.insert(server);
        if (needUpdate)
            updateStorageLocationByConsumers();
    }

}

void StorageLocationCameraController::unregisterConsumer(QnResourceDisplayPtr display)
{
    d->consumers.removeOne(display);

    if (auto server = display->resource()->getParentResource().dynamicCast<QnMediaServerResource>())
    {
        d->servers.erase(server);
        if (!d->servers.contains(server))
            updateStorageLocationByConsumers();
    }
}

} // namespace nx::vms::client::desktop
