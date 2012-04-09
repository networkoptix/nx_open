#include "time_period_reader_helper.h"
#include "core/resource/video_server.h"
#include "core/resourcemanagment/resource_pool.h"

QnTimePeriodReaderHelper::QnTimePeriodReaderHelper():
    m_mutex(QMutex::Recursive),
    m_multiRequestCount(0)
{
}

Q_GLOBAL_STATIC(QnTimePeriodReaderHelper, inst);
QnTimePeriodReaderHelper *QnTimePeriodReaderHelper::instance()
{
    return inst();
}

int QnTimePeriodReaderHelper::load(const QnNetworkResourceList &netResList, const QnTimePeriod &period)
{
    if (period.startTimeMs == 0)
        return 0; // it is incorrect periods
    QMutexLocker lock(&m_mutex);
    QList<int> hList;
    foreach(const QnNetworkResourcePtr& netRes, netResList)
    {
        int handle = load(netRes, period);
        if (handle > 0)
            hList << handle;
    }
    if (hList.isEmpty())
        return -1;
    int multiHandle = ++m_multiRequestCount;
    m_multiLoadProgress.insert(multiHandle, hList);
    return multiHandle;
}

int QnTimePeriodReaderHelper::load(QnNetworkResourcePtr netRes, const QnTimePeriod &period)
{
    QMutexLocker lock(&m_mutex);
    QnTimePeriodReaderPtr updater;
    NetResCache::iterator itr = m_cache.find(netRes);
    if (itr != m_cache.end()) {
        updater = itr.value();
    } else {
        updater = createUpdater(netRes);
        if (!updater)
            return -1;
        connect(updater.data(), SIGNAL(ready(const QnTimePeriodList &, int)), this, SLOT(onDataLoaded(const QnTimePeriodList &, int)));
        connect(updater.data(), SIGNAL(failed(int, int)), this, SLOT(onLoadingFailed(int, int)));

        m_cache.insert(netRes, updater);
    }
    return updater->load(period, QList<QRegion>());
}

void QnTimePeriodReaderHelper::onDataLoaded(const QnTimePeriodList &periods, int handle)
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

void QnTimePeriodReaderHelper::onLoadingFailed(int status, int handle)
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

QnTimePeriodReaderPtr QnTimePeriodReaderHelper::createUpdater(QnResourcePtr resource)
{
    QnVideoServerResourcePtr serverResource = qSharedPointerDynamicCast<QnVideoServerResource>(qnResPool->getResourceById(resource->getParentId()));
    if (!serverResource)
        return QnTimePeriodReaderPtr();
    QnVideoServerConnectionPtr serverConnection = serverResource->apiConnection();
    if (!serverConnection)
        return QnTimePeriodReaderPtr();
    QnNetworkResourcePtr netRes = qSharedPointerDynamicCast<QnNetworkResource>(resource);
    if (!netRes)
        return QnTimePeriodReaderPtr();
    return QnTimePeriodReaderPtr(new QnTimePeriodReader(serverConnection, netRes));
}
