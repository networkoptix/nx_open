// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_data_manager.h"

#include <camera/loaders/caching_camera_data_loader.h>
#include <common/common_module.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/media_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/ini.h>

using namespace nx::vms::client::desktop;

QnCameraDataManager::QnCameraDataManager(QnCommonModule* commonModule, QObject* parent):
    QObject(parent),
    QnCommonModuleAware(commonModule)
{
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            if (auto mediaResource = resource.dynamicCast<QnMediaResource>())
            {
                NX_DEBUG(this, "Removing loader %1", resource);
                m_loaderByResource.remove(mediaResource);
            }
        });
}

QnCameraDataManager::~QnCameraDataManager() {}

QnCachingCameraDataLoaderPtr QnCameraDataManager::loader(
    const QnMediaResourcePtr& resource,
    bool createIfNotExists)
{
    auto pos = m_loaderByResource.find(resource);
    if(pos != m_loaderByResource.end())
        return *pos;

    if (ini().disableChunksLoading)
        return {};

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

    m_loaderByResource[resource] = loader;
    return loader;
}

void QnCameraDataManager::clearCache()
{
    for (auto loader: m_loaderByResource)
    {
        if (loader)
            loader->discardCachedData();
    }
}
