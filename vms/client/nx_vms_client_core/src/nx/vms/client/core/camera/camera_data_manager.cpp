// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_data_manager.h"

#include <chrono>

#include <QtCore/QHash>
#include <QtCore/QTimer>

#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/resource/camera.h>
#include <nx/vms/client/core/resource/data_loaders/caching_camera_data_loader.h>
#include <nx/vms/client/core/server_runtime_events/server_runtime_event_connector.h>
#include <nx/vms/client/core/system_context.h>
#include <storage/server_storage_manager.h>

using namespace std::chrono;

using StorageLocation = nx::vms::api::StorageLocation;

namespace {

constexpr auto kDiscardCacheInterval = 1h;

} // namespace

namespace nx::vms::client::core {

struct CameraDataManager::Private
{
    QHash<QnMediaResourcePtr, CachingCameraDataLoaderPtr> loaderByResource;
    StorageLocation storageLocation = StorageLocation::both;
};

CameraDataManager::CameraDataManager(SystemContext* systemContext, QObject* parent):
    QObject(parent),
    SystemContextAware(systemContext),
    d(new Private())
{
    connect(systemContext->resourcePool(), &QnResourcePool::resourcesRemoved, this,
        [this](const QnResourceList& resources)
        {
            for (const auto& resource: resources)
            {
                if (auto mediaResource = resource.dynamicCast<QnMediaResource>())
                {
                    NX_VERBOSE(this, "Removing loader %1", resource);
                    d->loaderByResource.remove(mediaResource);
                }
            }
        });

    connect(cameraHistoryPool(), &QnCameraHistoryPool::cameraFootageChanged, this,
        [this](const QnVirtualCameraResourcePtr& camera)
        {
            if (const auto loader = this->loader(camera, /*createIfNotExists*/ false))
                loader->discardCachedData();
        });

    // Cross-system contexts do not have Server Storage Manager.
    if (auto serverStorageManager = systemContext->serverStorageManager())
    {
        connect(serverStorageManager, &QnServerStorageManager::serverRebuildArchiveFinished,
            this, &CameraDataManager::clearCache);

        connect(serverStorageManager, &QnServerStorageManager::activeMetadataStorageChanged, this,
            [this](const QnMediaServerResourcePtr& server)
            {
                const auto serverFootageCameras =
                    cameraHistoryPool()->getServerFootageCameras(server);
                for (const auto& camera: serverFootageCameras)
                {
                    if (const auto loader = this->loader(camera, /*createIfNotExists*/ false))
                        loader->discardCachedDataType(Qn::AnalyticsContent);
                }
            });
    }

    // Cross-system contexts do not support runtime events.
    if (auto serverRuntimeEventConnector = systemContext->serverRuntimeEventConnector())
    {
        connect(
            serverRuntimeEventConnector, &ServerRuntimeEventConnector::deviceFootageChanged, this,
            [this](const std::vector<nx::Uuid>& deviceIds)
            {
                const auto devices =
                    resourcePool()->getResourcesByIds<QnVirtualCameraResource>(deviceIds);
                for (const auto& device: devices)
                {
                    if (const auto loader = this->loader(device, /*createIfNotExists*/ false))
                        loader->discardCachedData();
                }
            });
    }

    // TODO: #sivanov Temporary fix. Correct change would be: expand getTimePeriods query with
    // Region data, then truncate cached chunks by this region and synchronize the cache.
    auto discardCacheTimer = new QTimer(this);
    discardCacheTimer->setInterval(kDiscardCacheInterval);
    discardCacheTimer->setSingleShot(false);
    connect(discardCacheTimer, &QTimer::timeout, this, &CameraDataManager::clearCache);
    discardCacheTimer->start();
}

CameraDataManager::~CameraDataManager() {}

CachingCameraDataLoaderPtr CameraDataManager::loader(
    const QnMediaResourcePtr& resource,
    bool createIfNotExists)
{
    auto pos = d->loaderByResource.find(resource);
    if (pos != d->loaderByResource.end())
        return *pos;

    if (ini().disableChunksLoading)
        return {};

    if (!createIfNotExists)
        return {};

    if (!CachingCameraDataLoader::supportedResource(resource))
        return {};

    NX_ASSERT(resource->systemContext() == systemContext(),
        "Resource belongs to another System Context");

    CachingCameraDataLoaderPtr loader(new CachingCameraDataLoader(resource));

    const auto camera = resource.dynamicCast<QnClientCameraResource>();
    if (camera)
    {
        connect(camera.get(), &QnClientCameraResource::footageAdded,
            loader.data(), &CachingCameraDataLoader::discardCachedData);
    }

    connect(loader.data(), &CachingCameraDataLoader::periodsChanged, this,
        [this, resource](Qn::TimePeriodContent type, qint64 startTimeMs)
        {
            emit periodsChanged(resource, type, startTimeMs);
        });

    d->loaderByResource[resource] = loader;
    return loader;
}

void CameraDataManager::clearCache()
{
    for (auto loader: d->loaderByResource)
    {
        if (loader)
            loader->discardCachedData();
    }
}

StorageLocation CameraDataManager::storageLocation() const
{
    return d->storageLocation;
}

void CameraDataManager::setStorageLocation(StorageLocation value)
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

} // namespace nx::vms::client::core
