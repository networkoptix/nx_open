#include "flat_camera_data_loader.h"

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

//#define QN_FLAT_CAMERA_DATA_LOADER_DEBUG

namespace {
    QAtomicInt qn_fakeHandle(INT_MAX / 2);

    /** Minimum time (in milliseconds) for overlapping time periods requests.  */
    const int minOverlapDuration = 40*1000;

    /** Detail level for data loading - most precise value is used. */
    const int detailLevel = 1;
}

QnFlatCameraDataLoader::QnFlatCameraDataLoader(const QnMediaServerConnectionPtr &connection, const QnNetworkResourcePtr &camera, Qn::CameraDataType dataType, QObject *parent):
    QnAbstractCameraDataLoader(camera, dataType, parent),
    m_connection(connection)
{
    if(!connection)
        qnNullWarning(connection);

    if(!camera)
        qnNullWarning(camera);
}

QnFlatCameraDataLoader *QnFlatCameraDataLoader::newInstance(const QnMediaServerResourcePtr &server, const QnNetworkResourcePtr &camera,  Qn::CameraDataType dataType, QObject *parent) {
    if (!server || !camera)
        return NULL;
    
    QnMediaServerConnectionPtr serverConnection = server->apiConnection();
    if (!serverConnection)
        return NULL;

    return new QnFlatCameraDataLoader(serverConnection, camera, dataType, parent);
}

int QnFlatCameraDataLoader::load(const QnTimePeriod &targetPeriod, const QString &filter, const qint64 resolutionMs) {
    Q_ASSERT_X(resolutionMs == detailLevel, Q_FUNC_INFO, "this loader is supposed to load only flat data");
    Q_ASSERT_X(!targetPeriod.isInfinite(), Q_FUNC_INFO, "requesting infinite period is invalid");
    Q_UNUSED(resolutionMs);

    if (filter != m_filter)
        discardCachedData();
    m_filter = filter;

#ifdef QN_FLAT_CAMERA_DATA_LOADER_DEBUG
    qDebug() << "QnFlatCameraDataLoader::" << "load" << targetPeriod;
#endif

    /* Check whether the requested data is already loaded. */
    if (m_loadedPeriod.contains(targetPeriod)) {
#ifdef QN_FLAT_CAMERA_DATA_LOADER_DEBUG
        qDebug() << "QnFlatCameraDataLoader::" << "data already loaded";
#endif

        /* Data already loaded. */
        int handle = qn_fakeHandle.fetchAndAddAcquire(1);

        /* Must pass the ready signal through the event queue here as
        * the caller doesn't know request handle yet, and therefore 
        * cannot handle the signal. */
        emit delayedReady(m_loadedData, handle);
        return handle; 
    }

    /* Check whether requested data is currently being loaded. */
    for (int i = 0; i < m_loading.size(); ++i)
    {
        if (m_loading[i].period.contains(targetPeriod)) 
        {
#ifdef QN_FLAT_CAMERA_DATA_LOADER_DEBUG
            qDebug() << "QnFlatCameraDataLoader::" << "data is already being loaded";
#endif

            int handle = qn_fakeHandle.fetchAndAddAcquire(1);

            m_loading[i].waitingHandles << handle;
            return handle;
        }
    }

    /* We need to load all data between the already loaded piece and target piece. */
    QnTimePeriod periodToLoad = targetPeriod;
    if (!m_loadedPeriod.isEmpty()) {
        if (targetPeriod < m_loadedPeriod) {
            periodToLoad.startTimeMs = targetPeriod.startTimeMs;
            periodToLoad.durationMs = m_loadedPeriod.startTimeMs - targetPeriod.startTimeMs + minOverlapDuration;
        } else {
            periodToLoad.startTimeMs = m_loadedPeriod.endTimeMs() - minOverlapDuration;
            periodToLoad.durationMs = targetPeriod.endTimeMs() - periodToLoad.startTimeMs;
        }
    }

#ifdef QN_FLAT_CAMERA_DATA_LOADER_DEBUG
    qDebug() << "QnFlatCameraDataLoader::" << "already loaded period" << m_loadedPeriod;
    qDebug() << "QnFlatCameraDataLoader::" << "loading period" << periodToLoad;
#endif

    int handle = sendRequest(periodToLoad);

    m_loading << LoadingInfo(periodToLoad, handle);
    return handle;
}

void QnFlatCameraDataLoader::discardCachedData(const qint64 resolutionMs) {
#ifdef QN_FLAT_CAMERA_DATA_LOADER_DEBUG
    qDebug() << "QnFlatCameraDataLoader::" << "discarding cached data";
#endif

    Q_UNUSED(resolutionMs);
    m_loading.clear();
    m_loadedData.clear();
    m_loadedPeriod.clear();
}

int QnFlatCameraDataLoader::sendRequest(const QnTimePeriod &periodToLoad) {
    Q_ASSERT_X(m_dataType != Qn::BookmarkData, Q_FUNC_INFO, "this loader should NOT be used to load bookmarks");

    switch (m_dataType) {
    case Qn::RecordedTimePeriod:
    case Qn::MotionTimePeriod: 
    case Qn::BookmarkTimePeriod:
        return m_connection->getTimePeriodsAsync(
            QnNetworkResourceList() << m_resource.dynamicCast<QnNetworkResource>(),
            periodToLoad.startTimeMs, 
            periodToLoad.startTimeMs + periodToLoad.durationMs, 
            detailLevel, /* Always load data on most detailed level. */
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
            bookmarkFilter.minDurationMs = detailLevel;
            bookmarkFilter.text = m_filter;

            return m_connection->getBookmarksAsync( 
                m_resource.dynamicCast<QnNetworkResource>(),
                bookmarkFilter,
                this,
                SLOT(at_bookmarksReceived(int, const QnCameraBookmarkList &, int))
                );
        }
    default:
        Q_ASSERT_X(false, Q_FUNC_INFO, "should never get here");
        break;
    }
    return -1;
}

void QnFlatCameraDataLoader::at_timePeriodsReceived(int status, const QnTimePeriodList &timePeriods, int requestHandle) {
    QnAbstractCameraDataPtr data(new QnTimePeriodCameraData(timePeriods));
    handleDataLoaded(status, data, requestHandle);
}

void QnFlatCameraDataLoader::at_bookmarksReceived(int status, const QnCameraBookmarkList &bookmarks, int requestHandle) {
    QnAbstractCameraDataPtr data(new QnBookmarkCameraData(bookmarks));
    handleDataLoaded(status, data, requestHandle);
}

void QnFlatCameraDataLoader::updateLoadedPeriod(const QnTimePeriod &loadedPeriod) {
    Q_ASSERT_X(!loadedPeriod.isInfinite(), Q_FUNC_INFO, "we are not supposed to get infinite period here");

    if (m_loadedPeriod.isEmpty()) {
        m_loadedPeriod = loadedPeriod;
    } else {
        qint64 startTimeMs = qMin(m_loadedPeriod.startTimeMs, loadedPeriod.startTimeMs);
        qint64 endTimeMs = qMax(m_loadedPeriod.endTimeMs(), loadedPeriod.endTimeMs());
        m_loadedPeriod.startTimeMs = startTimeMs;
        m_loadedPeriod.durationMs = endTimeMs - startTimeMs;
    }

#ifdef QN_FLAT_CAMERA_DATA_LOADER_DEBUG
    qDebug() << "QnFlatCameraDataLoader::" << "loaded period updated to" << m_loadedPeriod;
#endif
}

void QnFlatCameraDataLoader::handleDataLoaded(int status, const QnAbstractCameraDataPtr &data, int requestHandle) {

    auto loadingInfoIter = std::find_if(m_loading.begin(), m_loading.end(), [requestHandle](const LoadingInfo &info) {return info.handle == requestHandle; } );
    if (loadingInfoIter == m_loading.end())
        return;

    auto loadingInfo = *loadingInfoIter;
    m_loading.erase(loadingInfoIter);

    if (status != 0) {
        foreach(int handle, loadingInfo.waitingHandles)
            emit failed(status, handle);
        emit failed(status, requestHandle);
        return;
    }

#ifdef QN_FLAT_CAMERA_DATA_LOADER_DEBUG
    QString debugName = m_connection->url().host();
    if (!data->dataSource().isEmpty()) {
        qDebug() << "---------------------------------------------------------------------";
        qDebug() << debugName << "CHUNK request" << loadingInfo.period;
        qDebug() << debugName << "CHUNK data" << data->dataSource();
        if (m_loadedData)
            qDebug() << debugName << "CHUNK total before" << m_loadedData->dataSource(); 
    }
#endif

    if (!m_loadedPeriod.contains(loadingInfo.period) && data) {
        if (!m_loadedData) {
            m_loadedData = data;
        }
        else if (!data->isEmpty()) {
            m_loadedData->append(data);
        }
        /* Check if already recorded period was deleted on the server. */
        else if (m_loadedData && data->isEmpty()) {
            m_loadedData->trim(loadingInfo.period.startTimeMs);
        }

#ifdef QN_FLAT_CAMERA_DATA_LOADER_DEBUG
        qDebug() << debugName << "CHUNK total after" << m_loadedData->dataSource(); 
#endif
        updateLoadedPeriod(loadingInfo.period);
    }

    foreach(int handle, loadingInfo.waitingHandles)
        emit ready(m_loadedData, handle);
    emit ready(m_loadedData, requestHandle);
}



