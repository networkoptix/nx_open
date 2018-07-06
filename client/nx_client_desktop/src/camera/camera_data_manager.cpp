#include "camera_data_manager.h"

#include <common/common_module.h>

#include <core/resource/media_resource.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource_management/resource_pool.h>

#include <camera/loaders/caching_camera_data_loader.h>

#include <nx/utils/log/log.h>

QnCameraDataManager::QnCameraDataManager(QnCommonModule* commonModule, QObject* parent):
    QObject(parent),
    QnCommonModuleAware(commonModule)
{
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            if (QnMediaResourcePtr mediaResource = resource.dynamicCast<QnMediaResource>())
            {
                NX_LOG(lit("removing loader %1").arg(resource->getName()), cl_logDEBUG1);
                m_loaderByResource.remove(mediaResource);
            }

            if (resource->getId() == this->commonModule()->remoteGUID())
                updateCurrentServer();
        });

    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            if (resource->getId() == this->commonModule()->remoteGUID())
                updateCurrentServer();
        });

    connect(commonModule, &QnCommonModule::remoteIdChanged, this,
        &QnCameraDataManager::updateCurrentServer);
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

    QnCachingCameraDataLoaderPtr loader(new QnCachingCameraDataLoader(resource,
        commonModule()->currentServer()));

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

void QnCameraDataManager::updateCurrentServer()
{
    auto server = commonModule()->currentServer();
    for (auto loader: m_loaderByResource)
    {
        if (loader)
            loader->updateServer(server);
    }

}
