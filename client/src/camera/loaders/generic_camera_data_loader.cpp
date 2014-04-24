#include "generic_camera_data_loader.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/network_resource.h>
#include <core/resource/media_server_resource.h>

#include <utils/common/warnings.h>
#include <utils/common/synctime.h>
#include <utils/math/math.h>

namespace {
    QAtomicInt qn_fakeHandle(INT_MAX / 2);
}

QnGenericCameraDataLoader::QnGenericCameraDataLoader(const QnMediaServerConnectionPtr &connection, const QnNetworkResourcePtr &resource, Qn::CameraDataType dataType, QObject *parent):
    QnAbstractCameraDataLoader(resource, dataType, parent),
    m_connection(connection)
{
    if(!connection)
        qnNullWarning(connection);

    if(!resource)
        qnNullWarning(resource);
}

QnGenericCameraDataLoader *QnGenericCameraDataLoader::newInstance(const QnMediaServerResourcePtr &serverResource, const QnResourcePtr &resource,  Qn::CameraDataType dataType, QObject *parent) {
    QnNetworkResourcePtr networkResource = qSharedPointerDynamicCast<QnNetworkResource>(resource);
    if (!networkResource || !serverResource)
        return NULL;
    
    QnMediaServerConnectionPtr serverConnection = serverResource->apiConnection();
    if (!serverConnection)
        return NULL;

    return new QnGenericCameraDataLoader(serverConnection, networkResource, dataType, parent);
}

int QnGenericCameraDataLoader::load(const QnTimePeriod &timePeriod, const QString &filter) {
    if (filter != m_filter)
        discardCachedData();
    m_filter = filter;

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

void QnGenericCameraDataLoader::discardCachedData() {
    m_loading.clear();
    m_loadedData.clear();
    m_loadedPeriods.clear();
}

int QnGenericCameraDataLoader::sendRequest(const QnTimePeriod &periodToLoad) {
    switch (m_dataType) {
    case Qn::RecordedTimePeriod:
    case Qn::MotionTimePeriod: 
    case Qn::BookmarkTimePeriod:
        return m_connection->getTimePeriodsAsync(
            QnNetworkResourceList() << m_resource.dynamicCast<QnNetworkResource>(),
            periodToLoad.startTimeMs, 
            periodToLoad.startTimeMs + periodToLoad.durationMs, 
            1, 
            dataTypeToPeriod(m_dataType),
            m_filter,
            this, 
            SLOT(at_timePeriodsReceived(int, const QnTimePeriodList &, int))
        );
    default:
        assert(false); //TODO: #GDM implement me!
        break;
    }
    return -1;
}

void QnGenericCameraDataLoader::at_timePeriodsReceived(int status, const QnTimePeriodList &timePeriods, int requstHandle) {
   QnAbstractCameraDataPtr data(new QnTimePeriodCameraData(m_dataType, timePeriods));
   handleDataLoaded(status, data, requstHandle);
}

void QnGenericCameraDataLoader::updateLoadedPeriods(QnTimePeriod loadedPeriod) {
    loadedPeriod.durationMs -= 60 * 1000; /* Cut off the last one minute as it may not contain the valid data yet. */ // TODO: #Elric cut off near live only

    // limit the loaded period to the right edge of the loaded data
    if(!m_loadedData->isEmpty())
        loadedPeriod.durationMs = qMin(loadedPeriod.durationMs, m_loadedData->dataSource().last().endTimeMs() - loadedPeriod.startTimeMs);

    // union loaded time range info 
    if(loadedPeriod.durationMs > 0) {
        QnTimePeriodList loadedPeriods;
        loadedPeriods.push_back(loadedPeriod);

        QVector<QnTimePeriodList> allLoadedPeriods;
        allLoadedPeriods << m_loadedPeriods << loadedPeriods;

        m_loadedPeriods = QnTimePeriodList::mergeTimePeriods(allLoadedPeriods); 
    }

    // reduce right edge of loaded period info if last period under writing now
    if (!m_loadedPeriods.isEmpty() && !m_loadedData->isEmpty() && m_loadedData->dataSource().last().durationMs == -1)
    {
        qint64 lastDataTime = m_loadedData->dataSource().last().startTimeMs;
        while (!m_loadedPeriods.isEmpty() && m_loadedPeriods.last().startTimeMs > lastDataTime)
            m_loadedPeriods.pop_back();
        if (!m_loadedPeriods.isEmpty()) {
            QnTimePeriod& lastPeriod = m_loadedPeriods.last();
            lastPeriod.durationMs = qMin(lastPeriod.durationMs, lastDataTime - lastPeriod.startTimeMs);
        }
    }
}

void QnGenericCameraDataLoader::handleDataLoaded(int status, const QnAbstractCameraDataPtr &data, int requstHandle) {
    for (int i = 0; i < m_loading.size(); ++i) {
        if (m_loading[i].handle != requstHandle)
            continue;
        
        if (status == 0) {
            if (!m_loadedData && data) {
                m_loadedData = data;
                updateLoadedPeriods(m_loading[i].period);
            }
            else
            if (data && !data->isEmpty()) {
                m_loadedData->append(data);
                updateLoadedPeriods(m_loading[i].period);
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



