#include "bookmark_camera_data_loader.h"

#include <api/media_server_connection.h>
#include <api/helpers/bookmark_request_data.h>

#include <common/common_module.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <recording/time_period_list.h>

#include <utils/common/warnings.h>

//#define QN_BOOKMARK_CAMERA_DATA_LOADER_DEBUG

namespace {
    const qint64 requestIntervalMs = 5 * 1000;
    const qint64 processQueueIntervalMs = requestIntervalMs / 5;

    const int bookmarksLimitPerRequest = 1024;
}

QnBookmarksLoader::QnBookmarksLoader(const QnVirtualCameraResourcePtr &camera, QObject *parent):
    QObject(parent),
    m_camera(camera)
{
    Q_ASSERT_X(m_camera, Q_FUNC_INFO, "Camera must exist here");

    //TODO: #GDM #Bookmarks we have a lot of timers here and in CachingBookmarksLoader's and they all are enabled all the time
    QTimer *loadTimer = new QTimer(this);
    loadTimer->setSingleShot(false);
    loadTimer->setInterval(processQueueIntervalMs);
    connect(loadTimer, &QTimer::timeout, this, &QnBookmarksLoader::processLoadQueue);
    loadTimer->start();
}

void QnBookmarksLoader::queueToLoad(const QnTimePeriod &period) {   
    m_queuedPeriod = period;
    /* Check if we already ready to load. */
    processLoadQueue();
}

void QnBookmarksLoader::processLoadQueue() {
    if (m_queuedPeriod.isNull())
        return;

    if (!m_timer.hasExpired(requestIntervalMs))
        return;

    sendRequest(m_queuedPeriod);
    m_queuedPeriod = QnTimePeriod();
}

void QnBookmarksLoader::load(const QnTimePeriod &period) {
    if (!m_camera || !period.isValid())
        return;

    queueToLoad(period);
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

    m_timer.restart();

    QnBookmarkRequestData requestData;
    requestData.cameras << m_camera;
    requestData.format = Qn::JsonFormat;
    requestData.filter.startTimeMs = period.startTimeMs;
    requestData.filter.endTimeMs = period.isInfinite()
        ? DATETIME_NOW
        : period.endTimeMs();
    requestData.filter.strategy = Qn::LongestFirst;
    requestData.filter.limit = bookmarksLimitPerRequest;

    //TODO: #GDM #Bookmarks #IMPLEMENT ME
    connection->getBookmarksAsync(requestData, this, SLOT(handleDataLoaded(int, const QnCameraBookmarkList &, int)));
}

void QnBookmarksLoader::handleDataLoaded(int status, const QnCameraBookmarkList &data, int requestHandle) {
    Q_UNUSED(requestHandle);
    if (status != 0)
        return;

#ifdef QN_BOOKMARK_CAMERA_DATA_LOADER_DEBUG
    qDebug() << "QnBookmarksLoader::" << "loaded data for" << m_loading.startTimeMs;
#endif

    if (m_loadedData.isEmpty()) {
        m_loadedData = data;
    } else {
        MultiServerCameraBookmarkList mergeSource;
        mergeSource.push_back(m_loadedData);
        mergeSource.push_back(data);
        m_loadedData = QnCameraBookmark::mergeCameraBookmarks(mergeSource);
    }
   
    emit bookmarksChanged(m_loadedData);
}
