#include "camera_data_manager.h"

#include <core/resource/media_resource.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource_management/resource_pool.h>

#include <camera/loaders/caching_camera_data_loader.h>

QnCameraDataManager::QnCameraDataManager(QObject *parent /*= 0*/):
    QObject(parent)
{
    connect(qnResPool, &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            if (QnMediaResourcePtr mediaResource = resource.dynamicCast<QnMediaResource>())
            {
                qDebug() << "removing loader" << resource->getName();
                m_loaderByResource.remove(mediaResource);
            }
        });
}

QnCameraDataManager::~QnCameraDataManager() {}

QnCachingCameraDataLoaderPtr QnCameraDataManager::loader(const QnMediaResourcePtr &resource, bool createIfNotExists)
{
    auto pos = m_loaderByResource.find(resource);
    if(pos != m_loaderByResource.end())
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

    m_loaderByResource[resource] = loader;
    return loader;
}

void QnCameraDataManager::clearCache()
{
    for (auto loader: m_loaderByResource)
        if (loader)
            loader->discardCachedData();
}
