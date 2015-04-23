#include "multi_server_camera_data_loader.h"
#include "utils/common/warnings.h"

#include <camera/data/abstract_camera_data.h>
#include <camera/loaders/flat_camera_data_loader.h>
#include <camera/loaders/generic_camera_data_loader.h>

#include <core/resource/network_resource.h>
#include "core/resource/media_server_resource.h"
#include "core/resource_management/resource_pool.h"
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

int QnMultiServerCameraDataLoader::load(const QnTimePeriod &period, const QString &filter, const qint64 resolutionMs) {
    if (period.isNull()) {
        qnNullWarning(period);
        return -1;
    }

    QList<int> handles;
    
    // sometime camera moved between servers. Get all servers for requested time period
    QnNetworkResourcePtr camera = m_resource.dynamicCast<QnNetworkResource>();
    QnResourceList serverList = QnCameraHistoryPool::instance()->getAllCameraServers(camera, period);
    foreach(const QnResourcePtr& server, serverList) {
        int handle = loadInternal(server.dynamicCast<QnMediaServerResource>(), camera, period, filter, resolutionMs);
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

int QnMultiServerCameraDataLoader::loadInternal(const QnMediaServerResourcePtr &server, const QnNetworkResourcePtr &camera, const QnTimePeriod &period, const QString &filter, const qint64 resolutionMs) {
    if (!server || server->getStatus() != Qn::Online)
        return -1;

    QnAbstractCameraDataLoader *loader;
    QString cacheKey = server->getId().toString() + camera->getId().toString();
    auto itr = m_cache.find(cacheKey);
    if (itr != m_cache.end()) {
        loader = itr.value();
    } else {
        if (m_dataType == Qn::BookmarkData)
            loader = QnGenericCameraDataLoader::newInstance(server, camera, m_dataType, this);
        else
            loader = QnFlatCameraDataLoader::newInstance(server, camera, m_dataType, this);
        if (!loader)
            return -1;
        
        connect(loader, &QnAbstractCameraDataLoader::ready, this, &QnMultiServerCameraDataLoader::onDataLoaded);
        connect(loader, &QnAbstractCameraDataLoader::failed, this, &QnMultiServerCameraDataLoader::onLoadingFailed);
        m_cache.insert(cacheKey, loader);
    }
    return loader->load(period, filter, resolutionMs);
}

void QnMultiServerCameraDataLoader::onDataLoaded(const QnAbstractCameraDataPtr &data, const QnTimePeriod &updatedPeriod, int handle) {
    int multiHandle = 0;
    for (auto itr = m_multiLoadProgress.begin(); itr != m_multiLoadProgress.end(); ++itr) {
        if (!itr->contains(handle)) 
            continue;
        
        multiHandle = itr.key();
        itr->removeOne(handle);

        QnTimePeriod mergedPeriod = m_multiLoadData[multiHandle].period;
        mergedPeriod.addPeriod(updatedPeriod);
        Q_ASSERT_X(!mergedPeriod.isInfinite(), Q_FUNC_INFO, "infinite periods are not supposed to be loaded");

        if (itr->isEmpty()) { // if that was the last piece then merge and notify...
            QnAbstractCameraDataPtr result(data->clone());
            result->mergeInto(m_multiLoadData[multiHandle].data); //bulk merge works faster than appending elements one-by-one
            m_multiLoadData.remove(multiHandle);
            m_multiLoadProgress.erase(itr);
            emit ready(result, mergedPeriod, multiHandle);
        } else { // ... else just store temporary result
            m_multiLoadData[multiHandle].data << data;
            m_multiLoadData[multiHandle].period = mergedPeriod;
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
            if (!m_multiLoadData[multiHandle].data.isEmpty())
            {
                QnAbstractCameraDataPtr result = m_multiLoadData[multiHandle].data.takeFirst()->clone();
                result->mergeInto(m_multiLoadData[multiHandle].data);
                emit ready(result, m_multiLoadData[multiHandle].period, multiHandle);
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
