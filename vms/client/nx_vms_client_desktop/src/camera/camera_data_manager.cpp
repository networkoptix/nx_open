// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_data_manager.h"

#include <camera/loaders/caching_camera_data_loader.h>
#include <common/common_module.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/media_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/log.h>

using StorageLocation = nx::vms::api::StorageLocation;

struct QnCameraDataManager::Private
{
    QHash<QnMediaResourcePtr, QnCachingCameraDataLoaderPtr> loaderByResource;
    StorageLocation storageLocation = StorageLocation::both;
};

QnCameraDataManager::QnCameraDataManager(QnCommonModule* commonModule, QObject* parent):
    QObject(parent),
    QnCommonModuleAware(commonModule),
    d(new Private())
{
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            if (auto mediaResource = resource.dynamicCast<QnMediaResource>())
            {
                NX_DEBUG(this, "Removing loader %1", resource);
                d->loaderByResource.remove(mediaResource);
            }
        });
}

QnCameraDataManager::~QnCameraDataManager() {}

QnCachingCameraDataLoaderPtr QnCameraDataManager::loader(
    const QnMediaResourcePtr& resource,
    bool createIfNotExists)
{
    auto pos = d->loaderByResource.find(resource);
    if(pos != d->loaderByResource.end())
        return *pos;

    if (!createIfNotExists)
        return QnCachingCameraDataLoaderPtr();

    if (!QnCachingCameraDataLoader::supportedResource(resource))
        return QnCachingCameraDataLoaderPtr();

    QnCachingCameraDataLoaderPtr loader(new QnCachingCameraDataLoader(resource));

    connect(loader.data(), &QnCachingCameraDataLoader::periodsChanged, this,
        [this, resource](Qn::TimePeriodContent type, qint64 startTimeMs)
        {
            emit periodsChanged(resource, type, startTimeMs);
        });

    d->loaderByResource[resource] = loader;
    return loader;
}

void QnCameraDataManager::clearCache()
{
    for (auto loader: d->loaderByResource)
    {
        if (loader)
            loader->discardCachedData();
    }
}

StorageLocation QnCameraDataManager::storageLocation() const
{
    return d->storageLocation;
}

void QnCameraDataManager::setStorageLocation(StorageLocation value)
{
    if (d->storageLocation == value)
        return;

    d->storageLocation = value;
    for (auto loader: d->loaderByResource)
    {
        if (loader)
            loader->setStorageLocation(value);
    }
}
