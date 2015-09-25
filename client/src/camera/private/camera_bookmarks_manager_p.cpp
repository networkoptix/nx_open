#include "camera_bookmarks_manager_p.h"

#include <api/helpers/bookmark_request_data.h>

#include <camera/camera_bookmarks_manager.h>
#include <camera/camera_bookmarks_query.h>

#include <common/common_module.h>

#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <camera/loaders/bookmark_camera_data_loader.h>

QnCameraBookmarksManagerPrivate::QnCameraBookmarksManagerPrivate(QnCameraBookmarksManager *parent)
    : QObject(parent)
    , q_ptr(parent)
    , m_requests()
{
    /*
     Bookmarks updating strategy:
     * clear cache or removing camera from pool
     * once a time update all auto-updating queries independently
        - return results at once, without local filtering (?)
        - make sure queries are listening only in bookmarks mode
     * implement full-text search on the client side
     * make sure bookmark periods exists before requesting to avoid a lot of empty requests
     * filter request period by chunks and by already loaded data
     * update chunks on history change and bookmarks adding/deleting, then forcefully re-request required periods
     */


}

QnCameraBookmarksManagerPrivate::~QnCameraBookmarksManagerPrivate()
{}

void QnCameraBookmarksManagerPrivate::getBookmarksAsync(const QnVirtualCameraResourceList &cameras
                                                        , const QnCameraBookmarkSearchFilter &filter
                                                        , BookmarksCallbackType callback) {
    auto server = qnCommon->currentServer();
    if (!server)
        return;

    auto connection = server->apiConnection();
    if (!connection)
        return;

    QnBookmarkRequestData requestData;
    requestData.cameras = cameras;
    requestData.format = Qn::JsonFormat;
    requestData.filter = filter;

    int handle = connection->getBookmarksAsync(requestData, this, SLOT(handleDataLoaded(int, const QnCameraBookmarkList &, int)));
    if (callback)
        m_requests[handle] = callback;
}

void QnCameraBookmarksManagerPrivate::addCameraBookmark(const QnVirtualCameraResourcePtr &camera, const QnCameraBookmark &bookmark, OperationCallbackType callback) {
    //TODO: #GDM #Bookmarks #IMPLEMENT_ME
}

void QnCameraBookmarksManagerPrivate::updateCameraBookmark(const QnVirtualCameraResourcePtr &camera, const QnCameraBookmark &bookmark, OperationCallbackType callback) {
    //TODO: #GDM #Bookmarks #IMPLEMENT_ME
}

void QnCameraBookmarksManagerPrivate::deleteCameraBookmark(const QnVirtualCameraResourcePtr &camera, const QnCameraBookmark &bookmark, OperationCallbackType callback) {
    //TODO: #GDM #Bookmarks #IMPLEMENT_ME
}


void QnCameraBookmarksManagerPrivate::handleDataLoaded(int status, const QnCameraBookmarkList &bookmarks, int handle) {
    if (!m_requests.contains(handle))
        return;

    auto callback = m_requests.take(handle);
    callback(status == 0, bookmarks);
}

void QnCameraBookmarksManagerPrivate::clearCache() {
    m_requests.clear();

    for (auto query: m_autoUpdatingQueries.keys())
        updateQueryCache(query);
}

void QnCameraBookmarksManagerPrivate::registerAutoUpdateQuery(const QnCameraBookmarksQueryPtr &query) {
    m_autoUpdatingQueries[query] = QnCameraBookmarkList();
}

void QnCameraBookmarksManagerPrivate::unregisterAutoUpdateQuery(const QnCameraBookmarksQueryPtr &query) {
    m_autoUpdatingQueries.remove(query);
}

QnCameraBookmarkList QnCameraBookmarksManagerPrivate::executeQuery(const QnCameraBookmarksQueryPtr &query) const {
    /* Check if we have already cached query result. */
    if (m_autoUpdatingQueries.contains(query))
        return m_autoUpdatingQueries[query];

    /* Calculate result from local data. */
    return executeQueryInternal(query);
}

QnCameraBookmarkList QnCameraBookmarksManagerPrivate::executeQueryInternal(const QnCameraBookmarksQueryPtr &query) const {

    if (!query->isValid())
        return QnCameraBookmarkList();

    MultiServerCameraBookmarkList result;

    for (const QnVirtualCameraResourcePtr &camera: query->cameras()) {
        if (!m_bookmarksByCamera.contains(camera))
            continue;
            
        QnCameraBookmarkList cameraBookmarks = m_bookmarksByCamera[camera];

        auto startIter = std::lower_bound(cameraBookmarks.cbegin(), cameraBookmarks.cend(), query->filter().startTimeMs);
        auto endIter = std::upper_bound(cameraBookmarks.cbegin(), cameraBookmarks.cend(), query->filter().endTimeMs, [](qint64 value, const QnCameraBookmark &other) {
            return value < other.endTimeMs();
        });

        QnCameraBookmarkList filtered;
        Q_ASSERT_X(startIter <= endIter, Q_FUNC_INFO, "Check if bookmarks are sorted and query is valid");
        for (auto iter = startIter; iter != endIter; ++iter)
            filtered << *iter;
        result.push_back(std::move(filtered));        

    }

    return QnCameraBookmark::mergeCameraBookmarks(result, query->filter().limit, query->filter().strategy);
}

void QnCameraBookmarksManagerPrivate::updateQueryCache(const QnCameraBookmarksQueryPtr &query) {
    auto cachedValue = m_autoUpdatingQueries[query];
    auto updatedValue = executeQueryInternal(query);
    m_autoUpdatingQueries[query] = updatedValue;
    if (cachedValue != updatedValue)
        emit query->bookmarksChanged(updatedValue);
}

