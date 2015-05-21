#include "camera_data_manager.h"

#include <core/resource/camera_bookmark.h>

#include <camera/loaders/caching_camera_data_loader.h>

QnCameraDataManager::QnCameraDataManager(QObject *parent /*= 0*/):
    QObject(parent)
{

}

QnCameraDataManager::~QnCameraDataManager() {}

QnCachingCameraDataLoader* QnCameraDataManager::loader(const QnMediaResourcePtr &resource, bool createIfNotExists) {
    QHash<QnMediaResourcePtr, QnCachingCameraDataLoader *>::const_iterator pos = m_loaderByResource.find(resource);
    if(pos != m_loaderByResource.end())
        return *pos;

    if (!createIfNotExists)
        return nullptr;

    if (!QnCachingCameraDataLoader::supportedResource(resource))
        return nullptr;

    QnCachingCameraDataLoader *loader = new QnCachingCameraDataLoader(resource, this);
    connect(loader, &QnCachingCameraDataLoader::periodsChanged, this, [this, resource](Qn::TimePeriodContent type, qint64 startTimeMs) {
        emit periodsChanged(resource, type, startTimeMs);
    });
    connect(loader, &QnCachingCameraDataLoader::bookmarksChanged, this, [this, resource]() {
        emit bookmarksChanged(resource);
    });

    m_loaderByResource[resource] = loader;
    return loader;
}

QnCameraBookmarkList QnCameraDataManager::bookmarks(const QnMediaResourcePtr &resource) const {
    if (!m_loaderByResource.contains(resource))    
        return QnCameraBookmarkList();
    return m_loaderByResource[resource]->bookmarks();
}

void QnCameraDataManager::clearCache() {
    for (QnCachingCameraDataLoader* loader: m_loaderByResource)
        if (loader)
            loader->discardCachedData();
}
