// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_location_camera_controller.h"

#include <QtCore/QList>

#include <camera/resource_display.h>
#include <nx/streaming/abstract_archive_stream_reader.h>

using StorageLocation = nx::vms::api::StorageLocation;

namespace nx::vms::client::desktop {

struct StorageLocationCameraController::Private
{
    StorageLocation storageLocation = StorageLocation::both;
    QList<QnResourceDisplayPtr> consumers;

    void update(QnResourceDisplayPtr consumer)
    {
        if (auto reader = consumer->archiveReader())
            reader->setStorageLocationFilter(storageLocation);
    }
};

StorageLocationCameraController::StorageLocationCameraController(QObject* parent):
    base_type(parent),
    d(new Private())
{
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

void StorageLocationCameraController::registerConsumer(QnResourceDisplayPtr display)
{
    d->consumers.push_back(display);
    d->update(display);
}

void StorageLocationCameraController::unregisterConsumer(QnResourceDisplayPtr display)
{
    d->consumers.removeOne(display);
}

} // namespace nx::vms::client::desktop
