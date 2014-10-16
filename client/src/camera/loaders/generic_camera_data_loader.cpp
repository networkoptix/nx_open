#include "generic_camera_data_loader.h"

#include <camera/data/abstract_camera_data.h>
#include <camera/data/time_period_camera_data.h>
#include <camera/data/bookmark_camera_data.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/network_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_bookmark.h>

#include <recording/time_period_list.h>

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

int QnGenericCameraDataLoader::load(const QnTimePeriod &timePeriod, const QString &filter, const qint64 resolutionMs) {
    if (filter != m_filter)
        discardCachedData(resolutionMs);
    m_filter = filter;

    const QnTimePeriodList &loadedPeriodList = m_loadedPeriods[resolutionMs];

    /* Check whether the requested data is already loaded. */
    foreach(const QnTimePeriod &loadedPeriod, loadedPeriodList) {
        if (loadedPeriod.contains(timePeriod)) {
            /* Data already loaded. */
            int handle = qn_fakeHandle.fetchAndAddAcquire(1);

            /* Must pass the ready signal through the event queue here as
             * the caller doesn't know request handle yet, and therefore 
             * cannot handle the signal. */
            emit delayedReady(m_loadedData[resolutionMs], handle);
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
    if (!loadedPeriodList.isEmpty())
    {
        QnTimePeriodList::const_iterator itr = qUpperBound(loadedPeriodList.cbegin(), loadedPeriodList.cend(), periodToLoad.startTimeMs);
        if (itr != loadedPeriodList.cbegin())
            itr--;

        if (qBetween(itr->startTimeMs, periodToLoad.startTimeMs, itr->startTimeMs + itr->durationMs))
        {
            qint64 endPoint = periodToLoad.startTimeMs + periodToLoad.durationMs;
            periodToLoad.startTimeMs = itr->startTimeMs + itr->durationMs - 40*1000; // add addition 40 sec (may server does not flush data e.t.c)
            periodToLoad.durationMs = endPoint - periodToLoad.startTimeMs;
            ++itr;
            if (itr != loadedPeriodList.cend()) {
                if (itr->startTimeMs < periodToLoad.startTimeMs + periodToLoad.durationMs)
                    periodToLoad.durationMs = itr->startTimeMs - periodToLoad.startTimeMs;
            }
        }
        else if (qBetween(itr->startTimeMs, periodToLoad.startTimeMs + periodToLoad.durationMs, itr->startTimeMs + itr->durationMs))
        {
            periodToLoad.durationMs = itr->startTimeMs - periodToLoad.startTimeMs;
        }
    }

    int handle = sendRequest(periodToLoad, resolutionMs);

    m_loading << LoadingInfo(periodToLoad, resolutionMs, handle);
    return handle;
}

void QnGenericCameraDataLoader::discardCachedData(const qint64 resolutionMs) {
    if (resolutionMs <= 0) {
        m_loading.clear();
        m_loadedData.clear();
        m_loadedPeriods.clear();
    } else {
        m_loading.erase(std::remove_if(m_loading.begin(), m_loading.end(), [resolutionMs](const LoadingInfo &info) { return info.resolutionMs == resolutionMs; }), m_loading.end());
        m_loadedData[resolutionMs].clear();
        m_loadedPeriods[resolutionMs].clear();
    }
}

int QnGenericCameraDataLoader::sendRequest(const QnTimePeriod &periodToLoad, const qint64 resolutionMs) {
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
    case Qn::BookmarkData:
        {
            QnCameraBookmarkSearchFilter bookmarkFilter;
            bookmarkFilter.minStartTimeMs = periodToLoad.startTimeMs;
            bookmarkFilter.maxStartTimeMs = periodToLoad.startTimeMs + periodToLoad.durationMs;
            bookmarkFilter.minDurationMs = resolutionMs;
            bookmarkFilter.text = m_filter;

            return m_connection->getBookmarksAsync( 
                m_resource.dynamicCast<QnNetworkResource>(),
                bookmarkFilter,
                this,
                SLOT(at_bookmarksReceived(int, const QnCameraBookmarkList &, int))
                );
        }
    default:
        assert(false); //should never get here
        break;
    }
    return -1;
}

void QnGenericCameraDataLoader::at_timePeriodsReceived(int status, const QnTimePeriodList &timePeriods, int requestHandle) {
    QnAbstractCameraDataPtr data(new QnTimePeriodCameraData(timePeriods));
    handleDataLoaded(status, data, requestHandle);
}

void QnGenericCameraDataLoader::at_bookmarksReceived(int status, const QnCameraBookmarkList &bookmarks, int requestHandle) {
    QnAbstractCameraDataPtr data(new QnBookmarkCameraData(bookmarks));
    handleDataLoaded(status, data, requestHandle);
}

void QnGenericCameraDataLoader::updateLoadedPeriods(const QnTimePeriod &loadedPeriod, const qint64 resolutionMs) {
    QnTimePeriod newPeriod(loadedPeriod);
    const QnAbstractCameraDataPtr &loadedData = m_loadedData[resolutionMs];
    QnTimePeriodList &loadedPeriods = m_loadedPeriods[resolutionMs];

    /* Cut off the last one minute as it may not contain the valid data yet. */ // TODO: #Elric cut off near live only
    newPeriod.durationMs -= 60 * 1000; 

    // limit the loaded period to the right edge of the loaded data
    if(!loadedData->isEmpty())
        newPeriod.durationMs = qMin(newPeriod.durationMs, loadedData->dataSource().last().endTimeMs() - newPeriod.startTimeMs);

    // union loaded time range info 
    if(newPeriod.durationMs > 0) {
        QnTimePeriodList newPeriods;
        newPeriods.push_back(newPeriod);

        QVector<QnTimePeriodList> allLoadedPeriods;
        allLoadedPeriods << loadedPeriods << newPeriods;

        loadedPeriods = QnTimePeriodList::mergeTimePeriods(allLoadedPeriods); 
    }

    // reduce right edge of loaded period info if last period under writing now
    if (!loadedPeriods.isEmpty() && !loadedData->isEmpty() && loadedData->dataSource().last().durationMs == -1)
    {
        qint64 lastDataTime = loadedData->dataSource().last().startTimeMs;
        while (!loadedPeriods.isEmpty() && loadedPeriods.last().startTimeMs > lastDataTime)
            loadedPeriods.pop_back();
        if (!m_loadedPeriods.isEmpty()) {
            QnTimePeriod& lastPeriod = loadedPeriods.last();
            lastPeriod.durationMs = qMin(lastPeriod.durationMs, lastDataTime - lastPeriod.startTimeMs);
        }
    }

}

//#define CHUNKS_LOADER_DEBUG

void QnGenericCameraDataLoader::handleDataLoaded(int status, const QnAbstractCameraDataPtr &data, int requstHandle) {
    for (int i = 0; i < m_loading.size(); ++i) {
        if (m_loading[i].handle != requstHandle)
            continue;
#ifdef CHUNKS_LOADER_DEBUG
        QString debugName = m_connection->url().host();
#endif

        qint64 resolutionMs = m_loading[i].resolutionMs;
        if (status == 0) {

#ifdef CHUNKS_LOADER_DEBUG
            if (!data->dataSource().isEmpty()) {
                qDebug() << "---------------------------------------------------------------------";
                qDebug() << debugName << "CHUNK request" << m_loading[i].period;
                qDebug() << debugName << "CHUNK data" << data->dataSource();
                if (m_loadedData[resolutionMs])
                    qDebug() << debugName << "CHUNK total before" << m_loadedData[resolutionMs]->dataSource(); 
            }
#endif

            if (!m_loadedData[resolutionMs] && data) {
                m_loadedData[resolutionMs] = data;
#ifdef CHUNKS_LOADER_DEBUG
                qDebug() << debugName << "CHUNK total after" << m_loadedData[resolutionMs]->dataSource(); 
#endif
                updateLoadedPeriods(m_loading[i].period, resolutionMs);
            }
            else
            if (data && !data->isEmpty()) {
                m_loadedData[resolutionMs]->append(data);
#ifdef CHUNKS_LOADER_DEBUG
                qDebug() << debugName << "CHUNK total after" << m_loadedData[resolutionMs]->dataSource(); 
#endif
                updateLoadedPeriods(m_loading[i].period, resolutionMs);
            }

            foreach(int handle, m_loading[i].waitingHandles)
                emit ready(m_loadedData[resolutionMs], handle);
            emit ready(m_loadedData[resolutionMs], requstHandle);
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



