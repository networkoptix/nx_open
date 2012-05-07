#include "multi_camera_time_period_loader.h"
#include "utils/common/warnings.h"
#include "core/resource/video_server.h"
#include "core/resourcemanagment/resource_pool.h"
#include "core/resource/camera_history.h"

namespace {
    QAtomicInt qn_multiHandle(1);
}

QnMultiCameraTimePeriodLoader::QnMultiCameraTimePeriodLoader(QnNetworkResourcePtr resource, QObject *parent):
    QObject(parent),
    m_mutex(QMutex::Recursive)
{
    m_resource = resource;
}

QnMultiCameraTimePeriodLoader* QnMultiCameraTimePeriodLoader::newInstance(QnResourcePtr resource, QObject *parent)
{
    QnNetworkResourcePtr netRes = qSharedPointerDynamicCast<QnNetworkResource> (resource);
    if (netRes == NULL)
        return 0;
    return new QnMultiCameraTimePeriodLoader(netRes, parent);
}

int QnMultiCameraTimePeriodLoader::load(const QnTimePeriod &period, const QList<QRegion> &motionRegions)
{
    if (period.isNull()) {
        qnNullWarning(period);
        return -1;
    }

    QMutexLocker lock(&m_mutex);
    QList<int> handles;
    
    // sometime camera moved between media server. Get all servers for requested time period
    QList<QnNetworkResourcePtr> cameraList = QnCameraHistoryPool::instance()->getAllCamerasWithSameMac(m_resource, period);
    foreach(const QnNetworkResourcePtr& camera, cameraList)
    {
        int handle = loadInternal(camera, period, motionRegions);
        if (handle > 0)
            handles << handle;
    }
    
    if (handles.isEmpty())
        return -1;

    int multiHandle = qn_multiHandle.fetchAndAddAcquire(1);
    m_multiLoadProgress.insert(multiHandle, handles);
    return multiHandle;
}

int QnMultiCameraTimePeriodLoader::loadInternal(QnNetworkResourcePtr networkResource, const QnTimePeriod &period, const QList<QRegion> &motionRegions)
{
    QMutexLocker lock(&m_mutex);
    QnTimePeriodLoader *loader;
    QMap<QnNetworkResourcePtr, QnTimePeriodLoader *>::iterator itr = m_cache.find(networkResource);
    if (itr != m_cache.end()) {
        loader = itr.value();
    } else {
        loader = QnTimePeriodLoader::newInstance(networkResource);
        if (!loader)
            return -1;
        connect(loader, SIGNAL(ready(const QnTimePeriodList &, int)), this, SLOT(onDataLoaded(const QnTimePeriodList &, int)));
        connect(loader, SIGNAL(failed(int, int)), this, SLOT(onLoadingFailed(int, int)));

        m_cache.insert(networkResource, loader);
    }
    return loader->load(period, motionRegions);
}

void QnMultiCameraTimePeriodLoader::onDataLoaded(const QnTimePeriodList &periods, int handle)
{
    QnTimePeriodList result;
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
                    result = QnTimePeriod::mergeTimePeriods(m_multiLoadPeriod[multiHandle]);
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
                    emit failed(status, multiHandle);
                break;
            }
        }
    }
}

QnNetworkResourcePtr QnMultiCameraTimePeriodLoader::resource() const
{
    return m_resource;
}
