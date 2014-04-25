#include "time_period_loader.h"

#include <utils/common/warnings.h>
#include <utils/common/synctime.h>
#include <utils/math/math.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/network_resource.h>
#include <core/resource/media_server_resource.h>

//#define QN_TIME_PERIOD_LOADER_DEBUG

#ifdef QN_TIME_PERIOD_LOADER_DEBUG
#   define TRACE(...) qDebug() << __VA_ARGS__
#else
#   define TRACE(...)
#endif

namespace {
    QAtomicInt qn_fakeHandle(INT_MAX / 2);
}

QnTimePeriodLoader::QnTimePeriodLoader(const QnMediaServerConnectionPtr &connection, QnNetworkResourcePtr resource, QObject *parent):
    QnAbstractTimePeriodLoader(resource, parent),
    m_connection(connection)
{
    if(!connection)
        qnNullWarning(connection);

    if(!resource)
        qnNullWarning(resource);
}

QnTimePeriodLoader *QnTimePeriodLoader::newInstance(QnMediaServerResourcePtr serverResource, QnResourcePtr resource, QObject *parent) {
    QnNetworkResourcePtr networkResource = qSharedPointerDynamicCast<QnNetworkResource>(resource);
    if (!networkResource || !serverResource)
        return NULL;
    
    QnMediaServerConnectionPtr serverConnection = serverResource->apiConnection();
    if (!serverConnection)
        return NULL;

    return new QnTimePeriodLoader(serverConnection, networkResource, parent);
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

        if (qBetween(itr->startTimeMs, periodToLoad.startTimeMs, itr->startTimeMs + itr->durationMs))
        {
            qint64 endPoint = periodToLoad.startTimeMs + periodToLoad.durationMs;
            periodToLoad.startTimeMs = itr->startTimeMs + itr->durationMs - 40*1000; // add addition 40 sec (may server does not flush data e.t.c)
            periodToLoad.durationMs = endPoint - periodToLoad.startTimeMs;
            ++itr;
            if (itr != m_loadedPeriods.end()) {
                if (itr->startTimeMs < periodToLoad.startTimeMs + periodToLoad.durationMs)
                    periodToLoad.durationMs = itr->startTimeMs - periodToLoad.startTimeMs;
            }
        }
        else if (qBetween(itr->startTimeMs, periodToLoad.startTimeMs + periodToLoad.durationMs, itr->startTimeMs + itr->durationMs))
        {
            periodToLoad.durationMs = itr->startTimeMs - periodToLoad.startTimeMs;
        }
    }

    int handle = sendRequest(periodToLoad);

    m_loading << LoadingInfo(periodToLoad, handle);
    return handle;
}

void QnTimePeriodLoader::discardCachedData() {
    QMutexLocker lock(&m_mutex);
    m_loading.clear();
    m_loadedData.clear();
    m_loadedPeriods.clear();
}

int QnTimePeriodLoader::sendRequest(const QnTimePeriod &periodToLoad)
{
    return m_connection->getTimePeriodsAsync(
        QnNetworkResourceList() << m_resource.dynamicCast<QnNetworkResource>(),
        periodToLoad.startTimeMs, 
        periodToLoad.startTimeMs + periodToLoad.durationMs, 
        1, 
        m_motionRegions,
        this, 
        SLOT(at_replyReceived(int, const QnTimePeriodList &, int))
    );
}

void QnTimePeriodLoader::at_replyReceived(int status, const QnTimePeriodList &timePeriods, int requstHandle)
{
    // TODO: #Elric I don't see a mutex here.

    for (int i = 0; i < m_loading.size(); ++i)
    {
        if (m_loading[i].handle == requstHandle)
        {
            if (status == 0) {
                if (!timePeriods.isEmpty()) 
                {
                    QVector<QnTimePeriodList> allPeriods;
                    if (!timePeriods.isEmpty() && !m_loadedData.isEmpty() && m_loadedData.last().durationMs == -1) 
                        if (timePeriods.last().startTimeMs >= m_loadedData.last().startTimeMs) // TODO: #Elric should be timePeriods.last().startTimeMs?
                            m_loadedData.last().durationMs = 0;
                    allPeriods << m_loadedData << timePeriods;
                    m_loadedData = QnTimePeriod::mergeTimePeriods(allPeriods); // union data

                    QnTimePeriod loadedPeriod = m_loading[i].period;
                    loadedPeriod.durationMs -= 60 * 1000; /* Cut off the last one minute as it may not contain the valid data yet. */ // TODO: #Elric cut off near live only
                    if(!m_loadedData.isEmpty())
                        loadedPeriod.durationMs = qMin(loadedPeriod.durationMs, m_loadedData.back().startTimeMs - loadedPeriod.startTimeMs);
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

    TRACE(
        "CHUNKS LOADED FOR" << resource()->getName() << 
        "LOADED END =" << (m_loadedPeriods.isEmpty() ? 0 : m_loadedPeriods.back().endTimeMs()) <<
        "DATA END =" << (m_loadedData.isEmpty() ? 0 : m_loadedData.back().endTimeMs())
    );
}

