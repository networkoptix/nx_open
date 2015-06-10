#include "bookmark_camera_data_loader.h"

#include <api/helpers/bookmark_request_data.h>

#include <common/common_module.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <recording/time_period_list.h>

#include <utils/common/warnings.h>

//#define QN_BOOKMARK_CAMERA_DATA_LOADER_DEBUG

namespace {
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

QnBookmarksLoader::QnBookmarksLoader(const QnVirtualCameraResourcePtr &camera, QObject *parent):
    QObject(parent),
    m_camera(camera)
{
    Q_ASSERT_X(m_camera, Q_FUNC_INFO, "Camera must exist here");
}


void QnBookmarksLoader::load(const QnTimePeriod &period) {
    if (!m_camera)
        return;

    sendRequest(period);
    //TODO: #GDM #Bookmarks think about optimization

    /* Check whether data is currently being loaded. */
 /*   if (m_loading.handle > 0 && m_loading.period.contains(period)) {
        auto handle = qn_fakeHandle.fetchAndAddAcquire(1);
        m_loading.waitingHandles << handle;
        return handle;
    }



    qint64 startTimeMs = 0;
    if (m_loadedData && !m_loadedData->dataSource().isEmpty()) {
        auto last = m_loadedData->dataSource().last();
        if (last.isInfinite())
            startTimeMs = last.startTimeMs;
        else
            startTimeMs = last.endTimeMs() - minOverlapDuration;
    }

#ifdef QN_BOOKMARK_CAMERA_DATA_LOADER_DEBUG
    qDebug() << "QnBookmarksLoader::" << "loading period from" << startTimeMs;
#endif

    m_loading.clear(); 
    m_loading.startTimeMs = startTimeMs;
    m_loading.handle = sendRequest(startTimeMs);
    return m_loading.handle;*/
}

QnCameraBookmarkList QnBookmarksLoader::bookmarks() const {
    return m_loadedData;
}

void QnBookmarksLoader::discardCachedData() {
#ifdef QN_BOOKMARK_CAMERA_DATA_LOADER_DEBUG
    qDebug() << "QnBookmarksLoader::" << "discarding cached data";
#endif
    m_loadedData.clear();
}

void QnBookmarksLoader::sendRequest(const QnTimePeriod &period) {
    auto server = qnCommon->currentServer();
    if (!server)
        return;

    auto connection = server->apiConnection();
    if (!connection)
        return;

    QnBookmarkRequestData requestData;
    requestData.cameras << m_camera;
    requestData.filter.startTimeMs = period.startTimeMs;
    requestData.filter.endTimeMs = period.isInfinite()
        ? DATETIME_NOW
        : period.endTimeMs();
    requestData.filter.strategy = Qn::LongestFirst;
    requestData.format = Qn::JsonFormat;

    //TODO: #GDM #Bookmarks #IMPLEMENT ME
    connection->getBookmarksAsync(requestData, this, SLOT(handleDataLoaded(int, const QnCameraBookmarkList &, int)));
}

void QnBookmarksLoader::handleDataLoaded(int status, const QnCameraBookmarkList &data, int requestHandle) {
    if (status != 0)
        return;

#ifdef QN_BOOKMARK_CAMERA_DATA_LOADER_DEBUG
    qDebug() << "QnBookmarksLoader::" << "loaded data for" << m_loading.startTimeMs;
#endif

    //TODO: #GDM #bookmarks merge

    m_loadedData = data;
    emit bookmarksChanged(m_loadedData);
}
