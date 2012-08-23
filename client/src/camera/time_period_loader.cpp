#include "time_period_loader.h"
#include <utils/common/warnings.h>
#include <core/resourcemanagment/resource_pool.h>
#include <core/resource/video_server_resource.h>

namespace {
    QAtomicInt qn_fakeHandle(INT_MAX / 2);
}

QnTimePeriodLoader::QnTimePeriodLoader(const QnVideoServerConnectionPtr &connection, QnNetworkResourcePtr resource, QObject *parent):
    QObject(parent), 
    m_connection(connection), 
    m_resource(resource)
{
    if(!connection)
        qnNullWarning(connection);

    if(!resource)
        qnNullWarning(resource);

    qRegisterMetaType<QnTimePeriodList>("QnTimePeriodList");

    connect(this, SIGNAL(delayedReady(const QnTimePeriodList &, int)), this, SIGNAL(ready(const QnTimePeriodList &, int)), Qt::QueuedConnection);
}

QnTimePeriodLoader *QnTimePeriodLoader::newInstance(QnResourcePtr resource, QObject *parent) {
    QnNetworkResourcePtr networkResource = qSharedPointerDynamicCast<QnNetworkResource>(resource);
    if (!networkResource)
        return NULL;

    QnVideoServerResourcePtr serverResource = qSharedPointerDynamicCast<QnVideoServerResource>(qnResPool->getResourceById(resource->getParentId()));
    if (!serverResource)
        return NULL;

    QnVideoServerConnectionPtr serverConnection = serverResource->apiConnection();
    if (!serverConnection)
        return NULL;

    return new QnTimePeriodLoader(serverConnection, networkResource, parent);
}

QnNetworkResourcePtr QnTimePeriodLoader::resource() const 
{
    QMutexLocker lock(&m_mutex);

    return m_resource;
}

int QnTimePeriodLoader::load(const QnTimePeriod &timePeriod, const QList<QRegion> &motionRegions)
{
    QMutexLocker lock(&m_mutex);
    if (motionRegions != m_motionRegions) {
        m_loadedPeriods.clear();
        m_loadedData.clear();
    }
    m_motionRegions = motionRegions;

    /* Check whether the requested data is already loaded. */
    foreach(const QnTimePeriod &loadedPeriod, m_loadedPeriods) 
    {
        if (loadedPeriod.contains(timePeriod)) 
        {
            /* Data already loaded. */
            int handle = qn_fakeHandle.fetchAndAddAcquire(1);

            /* Must pass the ready signal through the event queue here as
             * the caller doesn't know request handle yet, and therefore 
             * cannot handle the signal. */
            emit delayedReady(m_loadedData, handle);
            return handle; 
        }
    }

    /* Check whether requested data is currently being loaded. */
    for (int i = 0; i < m_loading.size(); ++i)
    {
        if (m_loading[i].period.contains(timePeriod)) 
        {
            int handle = qn_fakeHandle.fetchAndAddAcquire(1);

            m_loading[i].waitingHandles << handle;
            return handle;
        }
    }

    /* Try to reduce duration of the period to load. */
    QnTimePeriod periodToLoad = timePeriod;
    if (!m_loadedPeriods.isEmpty())
    {
        QnTimePeriodList::iterator itr = qUpperBound(m_loadedPeriods.begin(), m_loadedPeriods.end(), periodToLoad.startTimeMs);
        if (itr != m_loadedPeriods.begin())
            itr--;

        if (qBetween(periodToLoad.startTimeMs, itr->startTimeMs, itr->startTimeMs + itr->durationMs))
        {
            qint64 endPoint = periodToLoad.startTimeMs + periodToLoad.durationMs;
            periodToLoad.startTimeMs = itr->startTimeMs + itr->durationMs - 20*1000; // add addition 20 sec (may server does not flush data e.t.c)
            periodToLoad.durationMs = endPoint - periodToLoad.startTimeMs;
            ++itr;
            if (itr != m_loadedPeriods.end()) {
                if (itr->startTimeMs < periodToLoad.startTimeMs + periodToLoad.durationMs)
                    periodToLoad.durationMs = itr->startTimeMs - periodToLoad.startTimeMs;
            }
        }
        else if (qBetween(periodToLoad.startTimeMs + periodToLoad.durationMs, itr->startTimeMs, itr->startTimeMs + itr->durationMs))
        {
            periodToLoad.durationMs = itr->startTimeMs - periodToLoad.startTimeMs;
        }
    }

    int handle = sendRequest(periodToLoad);

    m_loading << LoadingInfo(periodToLoad, handle);
    return handle;
}

void QnTimePeriodLoader::at_replyReceived(int status, const QnTimePeriodList &timePeriods, int requstHandle)
{
    for (int i = 0; i < m_loading.size(); ++i)
    {
        if (m_loading[i].handle == requstHandle)
        {
            if (status == 0) {
                QVector<QnTimePeriodList> allPeriods;
                if (!timePeriods.isEmpty() && !m_loadedData.isEmpty() && m_loadedData.last().durationMs == -1) 
                {
                    if (timePeriods.last().startTimeMs >= m_loadedData.last().startTimeMs)
                        m_loadedData.last().durationMs = 0;
                }
                allPeriods << m_loadedData << timePeriods;
                m_loadedData = QnTimePeriod::mergeTimePeriods(allPeriods); // union data

                QnTimePeriod loadedPeriod = m_loading[i].period;
                loadedPeriod.durationMs = qMax(0ll, loadedPeriod.durationMs - 60 * 1000); /* Cut off the last one minute as it may not contain the valid data yet. */ // TODO: cut off near live only
                if(loadedPeriod.durationMs > 0) {
                    QnTimePeriodList loadedPeriods;
                    loadedPeriods.push_back(loadedPeriod);

                    QVector<QnTimePeriodList> allLoadedPeriods;
                    allLoadedPeriods << m_loadedPeriods << loadedPeriods;

                    m_loadedPeriods = QnTimePeriod::mergeTimePeriods(allLoadedPeriods); // union loaded time range info 
                }

                // reduce right edge of loaded period info if last period under writing now
                if (!m_loadedPeriods.isEmpty() && !m_loadedData.isEmpty() && m_loadedData.last().durationMs == -1)
                {
                    qint64 lastDataTime = m_loadedData.last().startTimeMs;
                    while (!m_loadedPeriods.isEmpty() && m_loadedPeriods.last().startTimeMs > lastDataTime)
                        m_loadedPeriods.pop_back();
                    if (!m_loadedPeriods.isEmpty()) {
                        QnTimePeriod& lastPeriod = m_loadedPeriods.last();
                        lastPeriod.durationMs = qMin(lastPeriod.durationMs, lastDataTime - lastPeriod.startTimeMs);
                    }
                }

                foreach(int handle, m_loading[i].waitingHandles)
                    emit ready(m_loadedData, handle);
                emit ready(m_loadedData, requstHandle);
            }
            else {
                foreach(int handle, m_loading[i].waitingHandles)
                    emit failed(status, handle);
                emit failed(status, requstHandle);
            }
            m_loading.removeAt(i);
            break;
        }
    }
}

int QnTimePeriodLoader::sendRequest(const QnTimePeriod &periodToLoad)
{
    return m_connection->asyncRecordedTimePeriods(
        QnNetworkResourceList() << m_resource, 
        periodToLoad.startTimeMs, 
        periodToLoad.startTimeMs + periodToLoad.durationMs, 
        1, 
        m_motionRegions,
        this, 
        SLOT(at_replyReceived(int, const QnTimePeriodList &, int))
    );
}

