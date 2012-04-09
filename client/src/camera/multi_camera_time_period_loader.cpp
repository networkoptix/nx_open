#include "multi_camera_time_period_loader.h"
#include "utils/common/warnings.h"
#include "core/resource/video_server.h"
#include "core/resourcemanagment/resource_pool.h"

QnMultiCameraTimePeriodLoader::QnMultiCameraTimePeriodLoader():
    m_mutex(QMutex::Recursive),
    m_multiRequestCount(0)
{
}

Q_GLOBAL_STATIC(QnMultiCameraTimePeriodLoader, inst);
QnMultiCameraTimePeriodLoader *QnMultiCameraTimePeriodLoader::instance()
{
    return inst();
}

int QnMultiCameraTimePeriodLoader::load(const QnNetworkResourceList &networkResources, const QnTimePeriod &period)
{
    if (period.isNull()) {
        qnNullWarning(period);
        return -1;
    }

    QMutexLocker lock(&m_mutex);
    QList<int> hList;
    foreach(const QnNetworkResourcePtr &networkResource, networkResources)
    {
        int handle = load(networkResource, period);
        if (handle >= 0)
            hList << handle;
    }
    if (hList.isEmpty())
        return -1;
    int multiHandle = ++m_multiRequestCount;
    m_multiLoadProgress.insert(multiHandle, hList);
    return multiHandle;
}

int QnMultiCameraTimePeriodLoader::load(QnNetworkResourcePtr networkResource, const QnTimePeriod &period)
{
    QMutexLocker lock(&m_mutex);
    QnTimePeriodLoaderPtr loader;
    NetResCache::iterator itr = m_cache.find(networkResource);
    if (itr != m_cache.end()) {
        loader = itr.value();
    } else {
        loader = QnTimePeriodLoader::newInstance(networkResource);
        if (!loader)
            return -1;
        connect(loader.data(), SIGNAL(ready(const QnTimePeriodList &, int)), this, SLOT(onDataLoaded(const QnTimePeriodList &, int)));
        connect(loader.data(), SIGNAL(failed(int, int)), this, SLOT(onLoadingFailed(int, int)));

        m_cache.insert(networkResource, loader);
    }
    return loader->load(period, QList<QRegion>());
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

