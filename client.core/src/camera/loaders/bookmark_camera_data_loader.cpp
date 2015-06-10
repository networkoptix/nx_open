#include "bookmark_camera_data_loader.h"

#include <api/helpers/chunks_request_data.h>

#include <common/common_module.h>

#include <camera/data/abstract_camera_data.h>
#include <camera/data/time_period_camera_data.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <recording/time_period_list.h>

#include <utils/common/warnings.h>

//#define QN_BOOKMARK_CAMERA_DATA_LOADER_DEBUG

namespace {
    /** Fake handle for simultaneous load request. Initial value is big enough to not conflict with real request handles. */
    QAtomicInt qn_fakeHandle(INT_MAX / 2);

    /** Minimum time (in milliseconds) for overlapping time periods requests.  */
    const int minOverlapDuration = 120*1000;

    qint64 bookmarkResolution(qint64 periodDuration) {
        const int maxPeriodsPerTimeline = 1024; // magic const, thus visible periods can be edited through right-click
        qint64 maxPeriodLength = periodDuration / maxPeriodsPerTimeline;

        static const std::vector<qint64> steps = [maxPeriodLength](){ 
            std::vector<qint64> result;
            for (int i = 0; i < 40; ++i)
                result.push_back(1ll << i);

            result.push_back(maxPeriodLength);  /// To avoid end() result from lower_bound
            return result; 
        }();

        auto step = std::lower_bound(steps.cbegin(), steps.cend(), maxPeriodLength);
        return *step;

    }
}

QnBookmarkCameraDataLoader::QnBookmarkCameraDataLoader(const QnVirtualCameraResourcePtr &camera, QObject *parent):
    QObject(parent),
    m_camera(camera)
{
    Q_ASSERT_X(m_camera, Q_FUNC_INFO, "Camera must exist here");
}


int QnBookmarkCameraDataLoader::load(const QString &filter, const qint64 resolutionMs) {
    if (!m_camera)
        return -1;

    Q_UNUSED(resolutionMs);

    if (filter != m_filter)
        discardCachedData();
    m_filter = filter;

    /* Check whether data is currently being loaded. */
    if (m_loading.handle > 0) {
#ifdef QN_BOOKMARK_CAMERA_DATA_LOADER_DEBUG
        qDebug() << "QnBookmarkCameraDataLoader::" << "data is already being loaded";
#endif
        auto handle = qn_fakeHandle.fetchAndAddAcquire(1);
        m_loading.waitingHandles << handle;
        return handle;
    }

    /* We need to load all data after the already loaded piece, assuming there were no periods before already loaded. */
    qint64 startTimeMs = 0;
    if (m_loadedData && !m_loadedData->dataSource().isEmpty()) {
        auto last = m_loadedData->dataSource().last();
        if (last.isInfinite())
            startTimeMs = last.startTimeMs;
        else
            startTimeMs = last.endTimeMs() - minOverlapDuration;
    }

#ifdef QN_BOOKMARK_CAMERA_DATA_LOADER_DEBUG
    qDebug() << "QnBookmarkCameraDataLoader::" << "loading period from" << startTimeMs;
#endif

    m_loading.clear(); /* Just in case. */
    m_loading.startTimeMs = startTimeMs;
    m_loading.handle = sendRequest(startTimeMs);
    return m_loading.handle;
}

void QnBookmarkCameraDataLoader::discardCachedData(const qint64 resolutionMs) {
#ifdef QN_BOOKMARK_CAMERA_DATA_LOADER_DEBUG
    qDebug() << "QnBookmarkCameraDataLoader::" << "discarding cached data";
#endif

    Q_UNUSED(resolutionMs);
    m_loading.clear();
    m_loadedData.clear();
}

int QnBookmarkCameraDataLoader::sendRequest(qint64 startTimeMs) {
    auto server = qnCommon->currentServer();
    if (!server)
        return 0;   //TODO: #GDM #bookmarks make sure invalid value is handled

    auto connection = server->apiConnection();
    if (!connection)
        return 0;   //TODO: #GDM #bookmarks make sure invalid value is handled

    QnChunksRequestData requestData;
    requestData.resList << m_camera;
    requestData.startTimeMs = startTimeMs;
    requestData.endTimeMs = DATETIME_NOW,   /* Always load data to the end. */ 
    requestData.filter = m_filter;
    //requestData.periodsType = ;

    //TODO: #GDM #Bookmarks #IMPLEMENT ME
    return connection->recordedTimePeriods(requestData, this, SLOT(at_timePeriodsReceived(int, const MultiServerPeriodDataList &, int)));
}

void QnBookmarkCameraDataLoader::handleDataLoaded(int status, const QnAbstractCameraDataPtr &data, int requestHandle) {
    if (m_loading.handle != requestHandle)
        return;

    if (status != 0) {
        for(auto handle: m_loading.waitingHandles)
            emit failed(status, handle);
        emit failed(status, requestHandle);
        m_loading.clear();
        return;
    }

#ifdef QN_BOOKMARK_CAMERA_DATA_LOADER_DEBUG
    qDebug() << "QnBookmarkCameraDataLoader::" << "loaded data for" << m_loading.startTimeMs;
#endif

    QnTimePeriod loadedPeriod(m_loading.startTimeMs, QnTimePeriod::infiniteDuration());

    if (data) {
        if (!m_loadedData) {
            m_loadedData = data;
        }
        else if (!data->isEmpty()) {
            m_loadedData->update(data, loadedPeriod);
        }
    }

    for(auto handle: m_loading.waitingHandles)
        emit ready(m_loadedData, loadedPeriod, handle);
    emit ready(m_loadedData, loadedPeriod, requestHandle);
    m_loading.clear();
}

QnBookmarkCameraDataLoader::LoadingInfo::LoadingInfo():
    handle(0),
    startTimeMs(0)
{}

void QnBookmarkCameraDataLoader::LoadingInfo::clear() {
    handle = 0;
    startTimeMs = 0;
    waitingHandles.clear();
}