#include "multi_camera_time_period_loader.h"
#include "utils/common/warnings.h"

#include <core/resource/network_resource.h>
#include "core/resource/media_server_resource.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/camera_history.h"

namespace {
    QAtomicInt qn_multiHandle(1);
}

// ------------------------------------------ QnMultiCameraTimePeriodLoader ----------------------------------------

QnMultiCameraTimePeriodLoader::QnMultiCameraTimePeriodLoader(const QnResourcePtr &resource, Qn::TimePeriodContent periodsType, QObject *parent):
    QnAbstractTimePeriodLoader(resource, periodsType, parent),
    m_mutex(QMutex::Recursive)
{}

QnMultiCameraTimePeriodLoader *QnMultiCameraTimePeriodLoader::newInstance(const QnResourcePtr &resource, Qn::TimePeriodContent periodsType, QObject *parent)
{
    QnNetworkResourcePtr netRes = qSharedPointerDynamicCast<QnNetworkResource> (resource);
    if (netRes == NULL)
        return NULL;
    return new QnMultiCameraTimePeriodLoader(netRes, periodsType, parent);
}

int QnMultiCameraTimePeriodLoader::load(const QnTimePeriod &period, const QString &filter)
{
    if (period.isNull()) {
        qnNullWarning(period);
        return -1;
    }

    QMutexLocker lock(&m_mutex);
    QList<int> handles;
    
    // sometime camera moved between media server. Get all servers for requested time period
    QnNetworkResourcePtr camera = m_resource.dynamicCast<QnNetworkResource>();
    QnResourceList serverList = QnCameraHistoryPool::instance()->getAllCameraServers(camera, period);
    foreach(const QnResourcePtr& server, serverList)
    {
        int handle = loadInternal(server.dynamicCast<QnMediaServerResource>(), camera, period, filter);
        if (handle > 0)
            handles << handle;
    }
    
    if (handles.isEmpty())
        return -1;

    int multiHandle = qn_multiHandle.fetchAndAddAcquire(1);
    m_multiLoadProgress.insert(multiHandle, handles);
    return multiHandle;
}

void QnMultiCameraTimePeriodLoader::discardCachedData() {
    foreach(QnTimePeriodLoader *loader, m_cache)
        loader->discardCachedData();
}

int QnMultiCameraTimePeriodLoader::loadInternal(const QnMediaServerResourcePtr &mServer, const QnNetworkResourcePtr &networkResource, const QnTimePeriod &period, const QString &filter)
{
    QMutexLocker lock(&m_mutex);
    QnTimePeriodLoader *loader;
    QString cacheKey = mServer->getId().toString() + networkResource->getId().toString();
    QMap<QString, QnTimePeriodLoader *>::iterator itr = m_cache.find(cacheKey);
    if (itr != m_cache.end()) {
        loader = itr.value();
    } else {
        loader = QnTimePeriodLoader::newInstance(mServer, networkResource, m_periodsType, this);
        if (!loader)
            return -1;
        
        connect(loader, &QnAbstractTimePeriodLoader::ready, this, &QnMultiCameraTimePeriodLoader::onDataLoaded);
        connect(loader, &QnAbstractTimePeriodLoader::failed, this, &QnMultiCameraTimePeriodLoader::onLoadingFailed);
        m_cache.insert(cacheKey, loader);
    }
    return loader->load(period, filter);
}

void QnMultiCameraTimePeriodLoader::onDataLoaded(const QnAbstractTimePeriodListPtr &periods, int handle)
{
    QnAbstractTimePeriodListPtr result;
    int multiHandle = 0;
    {
        QMutexLocker lock(&m_mutex);
        for (QMap<int, QList<int> >::iterator itr = m_multiLoadProgress.begin(); itr != m_multiLoadProgress.end(); ++itr)
        {
            if (itr.value().contains(handle))
            {
                multiHandle = itr.key();
                m_multiLoadPeriod[multiHandle] << periods;
                itr.value().removeOne(handle);
                if (itr.value().isEmpty()) 
                {
                    if (!m_multiLoadPeriod[multiHandle].isEmpty())
                        result = m_multiLoadPeriod[multiHandle].first()->merged(m_multiLoadPeriod[multiHandle]);
                    m_multiLoadPeriod.remove(multiHandle);
                    m_multiLoadProgress.erase(itr);
                    emit ready(result, multiHandle);
                }
                break;
            }
        }
    }
}

void QnMultiCameraTimePeriodLoader::onLoadingFailed(int status, int handle)
{
    int multiHandle = 0;
    {
        QMutexLocker lock(&m_mutex);
        for (QMap<int, QList<int> >::iterator itr = m_multiLoadProgress.begin(); itr != m_multiLoadProgress.end(); ++itr)
        {
            if (itr.value().contains(handle))
            {
                multiHandle = itr.key();
                itr.value().removeOne(handle);
                if (itr.value().isEmpty()) 
                {
                    if (!m_multiLoadPeriod[multiHandle].isEmpty())
                    {
                        QnAbstractTimePeriodListPtr result = m_multiLoadPeriod[multiHandle].first()->merged(m_multiLoadPeriod[multiHandle]);
                        m_multiLoadPeriod.remove(multiHandle);
                        m_multiLoadProgress.erase(itr);
                        emit ready(result, multiHandle);
                    }
                    else {
                        emit failed(status, multiHandle);
                    }
                }
                break;
            }
        }
    }
}
