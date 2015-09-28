#include "camera_bookmarks_manager_p.h"

#include <api/helpers/bookmark_request_data.h>

#include <camera/camera_bookmarks_manager.h>
#include <camera/camera_bookmarks_query.h>

#include <common/common_module.h>

#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_history.h>
#include <core/resource/media_server_resource.h>

#include <utils/common/delayed.h>

namespace {
    const int updateLiveBookmarksTimeoutMs = 10000;
}

QnCameraBookmarksManagerPrivate::QueryInfo::QueryInfo() 
    : queryRef()
    , bookmarksCache()
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "This functions is not to be used directly");
}

QnCameraBookmarksManagerPrivate::QueryInfo::QueryInfo(const QnCameraBookmarksQueryPtr &query)
    : queryRef(query.toWeakRef())
    , bookmarksCache()
{}

QnCameraBookmarksManagerPrivate::QnCameraBookmarksManagerPrivate(QnCameraBookmarksManager *parent)
    : base_type(parent)
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

    QTimer* timer = new QTimer(this);
    timer->setInterval(updateLiveBookmarksTimeoutMs);
    timer->setSingleShot(false);
    connect(timer, &QTimer::timeout, this, [this]{
        for (auto queryId: m_queries.keys()) {
            updateQueryAsync(queryId);  //TODO: #GDM #bookmarks update only queries that require it and MERGE results
                                        // also make sure we are not updating bookmarks that are already in update process
        }
    });
    timer->start();

}

QnCameraBookmarksManagerPrivate::~QnCameraBookmarksManagerPrivate()
{}

void QnCameraBookmarksManagerPrivate::getBookmarksAsync(const QnVirtualCameraResourceSet &cameras
                                                        , const QnCameraBookmarkSearchFilter &filter
                                                        , BookmarksCallbackType callback) {
    auto server = qnCommon->currentServer();
    if (!server) {
        executeCallbackDelayed(callback);
        return;
    }

    auto connection = server->apiConnection();
    if (!connection) {
        executeCallbackDelayed(callback);
        return;
    }

    QnBookmarkRequestData requestData;
    requestData.cameras = cameras.toList();
    requestData.format = Qn::JsonFormat;
    requestData.filter = filter;

    int handle = connection->getBookmarksAsync(requestData, this, SLOT(handleDataLoaded(int, const QnCameraBookmarkList &, int)));
    if (callback)
        m_requests[handle] = callback;
}

QnCameraBookmarkList QnCameraBookmarksManagerPrivate::getLocalBookmarks(const QnVirtualCameraResourceSet &cameras, const QnCameraBookmarkSearchFilter &filter) const {
    if (cameras.isEmpty() || !filter.isValid())
        return QnCameraBookmarkList();

    MultiServerCameraBookmarkList result;

    for (const QnVirtualCameraResourcePtr &camera: cameras) {
        if (!m_bookmarksByCamera.contains(camera))
            continue;

        QnCameraBookmarkList cameraBookmarks = m_bookmarksByCamera[camera];

        auto startIter = std::lower_bound(cameraBookmarks.cbegin(), cameraBookmarks.cend(), filter.startTimeMs);
        auto endIter = std::upper_bound(cameraBookmarks.cbegin(), cameraBookmarks.cend(), filter.endTimeMs, [](qint64 value, const QnCameraBookmark &other) {
            return value < other.endTimeMs();
        });

        QnCameraBookmarkList filtered;
        Q_ASSERT_X(startIter <= endIter, Q_FUNC_INFO, "Check if bookmarks are sorted and query is valid");
        for (auto iter = startIter; iter != endIter; ++iter)
            filtered << *iter;
        result.push_back(std::move(filtered));        

    }

    return QnCameraBookmark::mergeCameraBookmarks(result, filter.limit, filter.strategy);
}


void QnCameraBookmarksManagerPrivate::addCameraBookmark(const QnVirtualCameraResourcePtr &camera, const QnCameraBookmark &bookmark, OperationCallbackType callback) {
    QnMediaServerResourcePtr server = qnCameraHistoryPool->getMediaServerOnTime(camera, bookmark.startTimeMs);
    if (!server)
        server = camera->getParentServer();

    //TODO: #GDM #Bookmarks notify queries?
    //TODO: #GDM #Bookmarks check if 
    //TODO: #GDM #Bookmarks implement distributed call
//     int handle = server->apiConnection()->addBookmarkAsync(camera, bookmark, this, SLOT(at_bookmarkAdded(int, const QnCameraBookmark &, int)));
//     m_processingBookmarks[handle] = camera;
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

    for (auto queryId: m_queries.keys()) {
        updateQueryCache(queryId, QnCameraBookmarkList());
        updateQueryAsync(queryId);
    }
}

void QnCameraBookmarksManagerPrivate::registerQuery(const QnCameraBookmarksQueryPtr &query) {
    Q_ASSERT_X(query, Q_FUNC_INFO, "Interface does not allow query to be null");
    Q_ASSERT_X(!m_queries.contains(query->id()), Q_FUNC_INFO, "Query should be registered and unregistered only once");

    /* Do not store shared pointer in the functor so it can be safely unregistered. */
    QUuid queryId = query->id();
    m_queries.insert(queryId, QueryInfo(query));
    connect(query, &QnCameraBookmarksQuery::queryChanged, this, [this, queryId] {
        updateQueryLocal(queryId);
        updateQueryAsync(queryId);
    });

    updateQueryLocal(queryId);
    updateQueryAsync(queryId);
}

void QnCameraBookmarksManagerPrivate::unregisterQuery(const QUuid &queryId) {
    Q_ASSERT_X(m_queries.contains(queryId), Q_FUNC_INFO, "Query should be registered and unregistered only once");
    m_queries.remove(queryId);
}

QnCameraBookmarkList QnCameraBookmarksManagerPrivate::executeQueryLocal(const QnCameraBookmarksQueryPtr &query) const {
    Q_ASSERT_X(query, Q_FUNC_INFO, "Interface does not allow query to be null");

    /* Check if we have already cached query result. */
    if (m_queries.contains(query->id()))
        return m_queries[query->id()].bookmarksCache;

    Q_ASSERT_X(false, Q_FUNC_INFO, "Should never get here. Cached value should always exist");
    /* Calculate result from local data. */
    return executeQueryInternal(query);
}

void QnCameraBookmarksManagerPrivate::executeQueryRemoteAsync(const QnCameraBookmarksQueryPtr &query, BookmarksCallbackType callback) {
    Q_ASSERT_X(query, Q_FUNC_INFO, "Interface does not allow query to be null");

    /* Do not store shared pointer here so it can be safely unregistered in the process. */
    QUuid queryId = query->id();
    getBookmarksAsync(query->cameras(), query->filter(), [this, queryId, callback](bool success, const QnCameraBookmarkList &bookmarks) {

        //TODO: #GDM #Bookmarks ensure query is the same as it was when we sent the request
        if (success && m_queries.contains(queryId))
            updateQueryCache(queryId, bookmarks);
        if (callback)
            callback(success, bookmarks);
    });
}


QnCameraBookmarkList QnCameraBookmarksManagerPrivate::executeQueryInternal(const QnCameraBookmarksQueryPtr &query) const {
    Q_ASSERT_X(query, Q_FUNC_INFO, "Interface does not allow query to be null");

    if (!query->isValid())
        return QnCameraBookmarkList();
    return getLocalBookmarks(query->cameras(), query->filter());   
}

void QnCameraBookmarksManagerPrivate::updateQueryLocal(const QUuid &queryId) {
    if (!m_queries.contains(queryId))
        return;

    QnCameraBookmarksQueryPtr query = m_queries[queryId].queryRef.toStrongRef();
    Q_ASSERT_X(query, Q_FUNC_INFO, "Interface does not allow query to be null");
    if (!query)
        return;

    updateQueryCache(queryId, executeQueryInternal(query));
}

void QnCameraBookmarksManagerPrivate::updateQueryAsync(const QUuid &queryId) {
    if (!m_queries.contains(queryId))
        return;

    QnCameraBookmarksQueryPtr query = m_queries[queryId].queryRef.toStrongRef();
    Q_ASSERT_X(query, Q_FUNC_INFO, "Interface does not allow query to be null");
    if (!query)
        return;

    executeQueryRemoteAsync(query, BookmarksCallbackType());
}

void QnCameraBookmarksManagerPrivate::updateQueryCache(const QUuid &queryId, const QnCameraBookmarkList &bookmarks) {
    Q_ASSERT_X(m_queries.contains(queryId), Q_FUNC_INFO, "Query must be registered");
    if (!m_queries.contains(queryId))
        return;

    QnCameraBookmarksQueryPtr query = m_queries[queryId].queryRef.toStrongRef();
    Q_ASSERT_X(query, Q_FUNC_INFO, "Interface does not allow query to be null");
    if (!query)
        return;

    auto cachedValue = m_queries[queryId].bookmarksCache;
    m_queries[queryId].bookmarksCache = bookmarks;
    if (cachedValue != bookmarks)
        emit query->bookmarksChanged(bookmarks);
}

QnCameraBookmarksQueryPtr QnCameraBookmarksManagerPrivate::createQuery(const QnVirtualCameraResourceSet &cameras, const QnCameraBookmarkSearchFilter &filter) {
    QnCameraBookmarksQueryPtr query(new QnCameraBookmarksQuery(cameras, filter));
    QUuid queryId = query->id();
    registerQuery(query);

    connect(query, &QObject::destroyed, this, [this, queryId]() {
        unregisterQuery(queryId);
    });

    return query;
}

void QnCameraBookmarksManagerPrivate::executeCallbackDelayed(BookmarksCallbackType callback) {
    if (!callback)
        return;

    executeDelayed([this, callback]{
        callback(false, QnCameraBookmarkList());
    });
}
