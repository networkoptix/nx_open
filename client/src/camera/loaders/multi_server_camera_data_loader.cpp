#include "multi_server_camera_data_loader.h"
#include "utils/common/warnings.h"

#include <camera/data/abstract_camera_data.h>
#include <camera/loaders/flat_camera_data_loader.h>

#include <core/resource/network_resource.h>
#include "core/resource/media_server_resource.h"
#include "core/resource/camera_history.h"

namespace {
    QAtomicInt qn_multiHandle(1);
}

// ------------------------------------------ QnMultiServerCameraDataLoader ----------------------------------------

QnMultiServerCameraDataLoader::QnMultiServerCameraDataLoader(const QnResourcePtr &resource, Qn::CameraDataType dataType, QObject *parent):
    QnAbstractCameraDataLoader(resource, dataType, parent)
{}

QnMultiServerCameraDataLoader *QnMultiServerCameraDataLoader::newInstance(const QnResourcePtr &resource, Qn::CameraDataType dataType, QObject *parent) {
    QnNetworkResourcePtr netRes = qSharedPointerDynamicCast<QnNetworkResource> (resource);
    if (netRes == NULL)
        return NULL;
    return new QnMultiServerCameraDataLoader(netRes, dataType, parent);
}

int QnMultiServerCameraDataLoader::load(const QString &filter, const qint64 resolutionMs) {
    QList<int> handles;
    
    // sometime camera moved between servers. Get all servers for requested time period
    QnNetworkResourcePtr camera = m_resource.dynamicCast<QnNetworkResource>();
    QnResourceList serverList = QnCameraHistoryPool::instance()->getAllCameraServers(camera);
    foreach(const QnResourcePtr& server, serverList) {
        int handle = loadInternal(server.dynamicCast<QnMediaServerResource>(), camera, filter, resolutionMs);
        if (handle > 0)
            handles << handle;
    }
    
    if (handles.isEmpty())
        return -1;

    int multiHandle = qn_multiHandle.fetchAndAddAcquire(1);
    m_multiLoadProgress.insert(multiHandle, handles);
    m_multiLoadData.insert(multiHandle, LoadingInfo());
    return multiHandle;
}

void QnMultiServerCameraDataLoader::discardCachedData(const qint64 resolutionMs) {
    foreach(QnAbstractCameraDataLoader *loader, m_cache)
        loader->discardCachedData(resolutionMs);
}

int QnMultiServerCameraDataLoader::loadInternal(const QnMediaServerResourcePtr &server, const QnNetworkResourcePtr &camera, const QString &filter, const qint64 resolutionMs) {
    if (!server || server->getStatus() != Qn::Online)
        return -1;

    QnAbstractCameraDataLoader *loader = NULL;
    QString cacheKey = server->getId().toString() + camera->getId().toString();
    auto itr = m_cache.find(cacheKey);
    if (itr != m_cache.end()) {
        loader = itr.value();
    } else {
        if (m_dataType != Qn::BookmarkData)
            loader = QnFlatCameraDataLoader::newInstance(server, camera, m_dataType, this);
        if (!loader)
            return -1;
        
        connect(loader, &QnAbstractCameraDataLoader::ready, this, &QnMultiServerCameraDataLoader::onDataLoaded);
        connect(loader, &QnAbstractCameraDataLoader::failed, this, &QnMultiServerCameraDataLoader::onLoadingFailed);
        m_cache.insert(cacheKey, loader);
    }
    return loader->load(filter, resolutionMs);
}

void QnMultiServerCameraDataLoader::onDataLoaded(const QnAbstractCameraDataPtr &data, const QnTimePeriod &updatedPeriod, int handle) {
    Q_ASSERT_X(updatedPeriod.isInfinite(), Q_FUNC_INFO, "Now we are always loading till the very end.");

    auto multiHandle = 0;
    for (auto itr = m_multiLoadProgress.begin(); itr != m_multiLoadProgress.end(); ++itr) {
        if (!itr->contains(handle)) 
            continue;
        
        multiHandle = itr.key();
        itr->removeOne(handle);

        auto startTimeMs = m_multiLoadData[multiHandle].startTimeMs;
        if (data && !data->isEmpty())
            startTimeMs = std::min(startTimeMs, updatedPeriod.startTimeMs);

        if (itr->isEmpty()) { // if that was the last piece then merge and notify...
            QnAbstractCameraDataPtr result(data->clone());
            result->mergeInto(m_multiLoadData[multiHandle].data); //bulk merge works faster than appending elements one-by-one
            m_multiLoadData.remove(multiHandle);
            m_multiLoadProgress.erase(itr);
            emit ready(result, QnTimePeriod(startTimeMs, QnTimePeriod::infiniteDuration()), multiHandle);
        } else { // ... else just store temporary result
            m_multiLoadData[multiHandle].data << data;
            m_multiLoadData[multiHandle].startTimeMs = startTimeMs;
        }
        break;
    }
}

void QnMultiServerCameraDataLoader::onLoadingFailed(int status, int handle) {
    auto multiHandle = 0;
    for (auto itr = m_multiLoadProgress.begin(); itr != m_multiLoadProgress.end(); ++itr) {
        if (!itr->contains(handle)) 
            continue;
            
        multiHandle = itr.key();
        itr->removeOne(handle);
        if (itr->isEmpty()) {
            auto multiLoadData = m_multiLoadData.take(multiHandle);
            if (!multiLoadData.data.isEmpty())
            {
                QnAbstractCameraDataPtr result = multiLoadData.data.takeFirst()->clone();
                result->mergeInto(multiLoadData.data);
                emit ready(result, QnTimePeriod(multiLoadData.startTimeMs, QnTimePeriod::infiniteDuration()), multiHandle);
            }
            else {
                emit failed(status, multiHandle);
            }
            m_multiLoadProgress.erase(itr);
        } 
        break;

    }
}

QnMultiServerCameraDataLoader::LoadingInfo::LoadingInfo() :
    startTimeMs(DATETIME_NOW)
{

}
