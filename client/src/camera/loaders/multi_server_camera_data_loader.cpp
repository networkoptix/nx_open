#include "multi_server_camera_data_loader.h"
#include "utils/common/warnings.h"

#include <camera/data/abstract_camera_data.h>
#include <camera/loaders/generic_camera_data_loader.h>

#include <core/resource_management/resource_pool.h>

#include <core/resource/network_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_history.h>

namespace {
    QAtomicInt qn_multiHandle(1);
}

// ------------------------------------------ QnMultiServerCameraDataLoader ----------------------------------------

QnMultiServerCameraDataLoader::QnMultiServerCameraDataLoader(const QnVirtualCameraResourcePtr &resource, Qn::CameraDataType dataType, QObject *parent):
    QnAbstractCameraDataLoader(resource, dataType, parent)
{}

QnMultiServerCameraDataLoader *QnMultiServerCameraDataLoader::newInstance(const QnVirtualCameraResourcePtr &resource, Qn::CameraDataType dataType, QObject *parent) {
    return new QnMultiServerCameraDataLoader(resource, dataType, parent);
}

int QnMultiServerCameraDataLoader::load(const QnTimePeriod &period, const QString &filter, const qint64 resolutionMs) {
    if (period.isNull()) {
        qnNullWarning(period);
        return -1;
    }

    QList<int> handles;
    
    // sometime camera moved between servers. Get all servers for requested time period
    QnVirtualCameraResourcePtr camera = m_resource.dynamicCast<QnVirtualCameraResource>();
    auto serverList = qnCameraHistoryPool->getCameraFootageData(camera, period);
    foreach(const QnMediaServerResourcePtr& server, serverList) {
        int handle = loadInternal(server, camera, period, filter, resolutionMs);
        if (handle > 0)
            handles << handle;
    }
    
    if (handles.isEmpty())
        return -1;

    int multiHandle = qn_multiHandle.fetchAndAddAcquire(1);
    m_multiLoadProgress.insert(multiHandle, handles);
    return multiHandle;
}

void QnMultiServerCameraDataLoader::discardCachedData(const qint64 resolutionMs) {
    foreach(QnAbstractCameraDataLoader *loader, m_cache)
        loader->discardCachedData(resolutionMs);
}

int QnMultiServerCameraDataLoader::loadInternal(const QnMediaServerResourcePtr &server, const QnVirtualCameraResourcePtr &camera, const QnTimePeriod &period, const QString &filter, const qint64 resolutionMs) {
    if (!server || server->getStatus() != Qn::Online)
        return -1;

    QnAbstractCameraDataLoader *loader;
    QString cacheKey = server->getId().toString() + camera->getId().toString();
    auto itr = m_cache.find(cacheKey);
    if (itr != m_cache.end()) {
        loader = itr.value();
    } else {
        loader = QnGenericCameraDataLoader::newInstance(server, camera, m_dataType, this);
        if (!loader)
            return -1;
        
        connect(loader, &QnAbstractCameraDataLoader::ready, this, &QnMultiServerCameraDataLoader::onDataLoaded);
        connect(loader, &QnAbstractCameraDataLoader::failed, this, &QnMultiServerCameraDataLoader::onLoadingFailed);
        m_cache.insert(cacheKey, loader);
    }
    return loader->load(period, filter, resolutionMs);
}

void QnMultiServerCameraDataLoader::onDataLoaded(const QnAbstractCameraDataPtr &data, int handle) {
    int multiHandle = 0;
    for (auto itr = m_multiLoadProgress.begin(); itr != m_multiLoadProgress.end(); ++itr) {
        if (!itr->contains(handle)) 
            continue;
        
        multiHandle = itr.key();
        itr->removeOne(handle);

        if (itr->isEmpty()) { // if that was the last piece then merge and notify...
            QnAbstractCameraDataPtr result(data->clone());
            result->append(m_multiLoadData[multiHandle]); //bulk append works faster than appending elements one-by-one
            m_multiLoadData.remove(multiHandle);
            m_multiLoadProgress.erase(itr);
            emit ready(result, multiHandle);
        } else { // ... else just store temporary result
            m_multiLoadData[multiHandle] << data;
        }
        break;
    }
}

void QnMultiServerCameraDataLoader::onLoadingFailed(int status, int handle) {
    int multiHandle = 0;
    for (auto itr = m_multiLoadProgress.begin(); itr != m_multiLoadProgress.end(); ++itr) {
        if (!itr->contains(handle)) 
            continue;
            
        multiHandle = itr.key();
        itr->removeOne(handle);
        if (itr->isEmpty()) {
            if (!m_multiLoadData[multiHandle].isEmpty())
            {
                QnAbstractCameraDataPtr result = m_multiLoadData[multiHandle].takeFirst()->clone();
                result->append(m_multiLoadData[multiHandle]);
                emit ready(result, multiHandle);
            }
            else {
                emit failed(status, multiHandle);
            }
            m_multiLoadData.remove(multiHandle);
            m_multiLoadProgress.erase(itr);
        } 
        break;

    }
}
