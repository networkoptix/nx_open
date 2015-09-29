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
#include <utils/common/synctime.h>

namespace {
    /** Live queries should be updated once in a time even if data is actual. */
    const int updateLiveBookmarksTimeoutMs = 10000;

    /** Each query can be updated no more often than once in that period. */
    const int minimimumRequestTimeoutMs = 3000;

    /** Timer precision to update all queries. */
    const int queriesCheckTimoutMs = 1000;

    /** Reserved value for invalid requests. */
    const int invalidRequestId = 0;
}


/************************************************************************/
/* OperationInfo                                                        */
/************************************************************************/
QnCameraBookmarksManagerPrivate::OperationInfo::OperationInfo()
    : operation(OperationType::Add)
    , callback()
    , camera()
{}

QnCameraBookmarksManagerPrivate::OperationInfo::OperationInfo(OperationType operation, OperationCallbackType callback, const QnVirtualCameraResourcePtr &camera)
    : operation(operation)
    , callback(callback)
    , camera(camera)
{}


/************************************************************************/
/* QueryInfo                                                            */
/************************************************************************/
QnCameraBookmarksManagerPrivate::QueryInfo::QueryInfo() 
    : queryRef()
    , bookmarksCache()
    , state(QueryState::Invalid)
    , requestTimer()
    , requestId(invalidRequestId)
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "This functions is not to be used directly");
}

QnCameraBookmarksManagerPrivate::QueryInfo::QueryInfo(const QnCameraBookmarksQueryPtr &query)
    : queryRef(query.toWeakRef())
    , bookmarksCache()
    , state(QueryState::Invalid)
    , requestTimer()
    , requestId(invalidRequestId)
{}


/************************************************************************/
/* QnCameraBookmarksManagerPrivate                                      */
/************************************************************************/
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
    timer->setInterval(queriesCheckTimoutMs);
    timer->setSingleShot(false);
    connect(timer, &QTimer::timeout, this, &QnCameraBookmarksManagerPrivate::checkQueriesUpdate);
    timer->start();
}

QnCameraBookmarksManagerPrivate::~QnCameraBookmarksManagerPrivate()
{}

int QnCameraBookmarksManagerPrivate::getBookmarksAsync(const QnVirtualCameraResourceSet &cameras
                                                        , const QnCameraBookmarkSearchFilter &filter
                                                        , BookmarksInternalCallbackType callback) {
    auto server = qnCommon->currentServer();
    if (!server) {
        executeCallbackDelayed(callback);
        return invalidRequestId; 
    }

    auto connection = server->apiConnection();
    if (!connection) {
        executeCallbackDelayed(callback);
        return invalidRequestId;
    }

    QnBookmarkRequestData requestData;
    requestData.cameras = cameras.toList();
    requestData.format = Qn::JsonFormat;
    requestData.filter = filter;

    int requestId = connection->getBookmarksAsync(requestData, this, SLOT(handleDataLoaded(int, const QnCameraBookmarkList &, int)));
    if (callback)
        m_requests[requestId] = callback;
    return requestId;
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
    //TODO: #GDM #Bookmarks implement distributed call
    int handle = server->apiConnection()->addBookmarkAsync(camera, bookmark, this, SLOT(handleBookmarkOperation(int, const QnCameraBookmark &, int)));
    m_operations[handle] = OperationInfo(OperationInfo::OperationType::Add, callback, camera);
}

void QnCameraBookmarksManagerPrivate::updateCameraBookmark(const QnVirtualCameraResourcePtr &camera, const QnCameraBookmark &bookmark, OperationCallbackType callback) {
    QnMediaServerResourcePtr server = qnCameraHistoryPool->getMediaServerOnTime(camera, bookmark.startTimeMs);
    if (!server)
        server = camera->getParentServer();

    //TODO: #GDM #Bookmarks notify queries?
    //TODO: #GDM #Bookmarks implement distributed call
    int handle = server->apiConnection()->updateBookmarkAsync(camera, bookmark, this, SLOT(handleBookmarkOperation(int, const QnCameraBookmark &, int)));
    m_operations[handle] = OperationInfo(OperationInfo::OperationType::Update, callback, camera);
}

void QnCameraBookmarksManagerPrivate::deleteCameraBookmark(const QnVirtualCameraResourcePtr &camera, const QnCameraBookmark &bookmark, OperationCallbackType callback) {
    QnMediaServerResourcePtr server = qnCameraHistoryPool->getMediaServerOnTime(camera, bookmark.startTimeMs);
    if (!server)
        server = camera->getParentServer();

    //TODO: #GDM #Bookmarks notify queries?
    //TODO: #GDM #Bookmarks implement distributed call
    int handle = server->apiConnection()->deleteBookmarkAsync(camera, bookmark, this, SLOT(handleBookmarkOperation(int, const QnCameraBookmark &, int)));
    m_operations[handle] = OperationInfo(OperationInfo::OperationType::Delete, callback, camera);
}

void QnCameraBookmarksManagerPrivate::handleBookmarkOperation(int status, const QnCameraBookmark &bookmark, int handle) {
    Q_UNUSED(bookmark);
    if (!m_operations.contains(handle))
        return;
    
    auto operationInfo = m_operations.take(handle);
    if (operationInfo.callback)
        operationInfo.callback(status == 0);
}


void QnCameraBookmarksManagerPrivate::handleDataLoaded(int status, const QnCameraBookmarkList &bookmarks, int requestId) {
    if (!m_requests.contains(requestId))
        return;

    auto callback = m_requests.take(requestId);
    callback(status == 0, bookmarks, requestId);
}

void QnCameraBookmarksManagerPrivate::clearCache() {
    m_requests.clear();

    for (auto queryId: m_queries.keys()) {
        updateQueryCache(queryId, QnCameraBookmarkList());
        updateQueryAsync(queryId);
    }
}

bool QnCameraBookmarksManagerPrivate::isQueryUpdateRequired(const QUuid &queryId) {
    Q_ASSERT_X(m_queries.contains(queryId), Q_FUNC_INFO, "Query should be registered");
    if (!m_queries.contains(queryId))
        return false;

    const QueryInfo info = m_queries[queryId];
    QnCameraBookmarksQueryPtr query = info.queryRef.toStrongRef();
    Q_ASSERT_X(query, Q_FUNC_INFO, "Interface does not allow query to be null");

    switch (info.state) {
    case QueryInfo::QueryState::Invalid:
        return true;
    case QueryInfo::QueryState::Queued:
        return !info.requestTimer.isValid() || info.requestTimer.hasExpired(minimimumRequestTimeoutMs);
    case QueryInfo::QueryState::Requested:
        return false;
    case QueryInfo::QueryState::Actual:
        return (query->filter().endTimeMs > qnSyncTime->currentMSecsSinceEpoch()) && info.requestTimer.hasExpired(updateLiveBookmarksTimeoutMs);
    default:
        Q_ASSERT_X(false, Q_FUNC_INFO, "Should never get here");
        break;
    }
    return false;
}


void QnCameraBookmarksManagerPrivate::checkQueriesUpdate() {
    for (auto queryId: m_queries.keys()) {
        bool updateRequired = isQueryUpdateRequired(queryId);
        if (!updateRequired)
            continue;

        const QueryInfo info = m_queries[queryId];
        QnCameraBookmarksQueryPtr query = info.queryRef.toStrongRef();
        Q_ASSERT_X(query, Q_FUNC_INFO, "Interface does not allow query to be null");
        if (!query)
            continue;

        executeQueryRemoteAsync(query, BookmarksCallbackType());
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
    if (!query) {
        executeCallbackDelayed(callback);
        return;
    }

    /* Do not store shared pointer here so it can be safely unregistered in the process. */
    QUuid queryId = query->id();

    Q_ASSERT_X(m_queries.contains(queryId), Q_FUNC_INFO, "Query must be registered");
    if (!m_queries.contains(queryId)) {
        executeCallbackDelayed(callback);
        return;
    }

    QueryInfo &info = m_queries[queryId];
    info.state = QueryInfo::QueryState::Requested;
    info.requestTimer.restart(); 
    info.requestId = getBookmarksAsync(query->cameras(), query->filter(), 
        [this, queryId, callback](bool success, const QnCameraBookmarkList &bookmarks, int requestId) {      
        if (success && m_queries.contains(queryId) && requestId != invalidRequestId) {
            QueryInfo &info = m_queries[queryId];
            if (info.requestId == requestId) {
                updateQueryCache(queryId, bookmarks); //TODO: #GDM #bookmarks merge results
                Q_ASSERT_X(info.state == QueryInfo::QueryState::Requested, Q_FUNC_INFO, "On state change we should clear request id.");               
                if (info.state == QueryInfo::QueryState::Requested)
                    info.state = QueryInfo::QueryState::Actual;
            }
        }
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

    QueryInfo &info = m_queries[queryId];
    info.requestId = invalidRequestId; /*< Clean request id to make sure we will not process non-actual results. */
    info.state = QueryInfo::QueryState::Queued;
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

void QnCameraBookmarksManagerPrivate::executeCallbackDelayed(BookmarksInternalCallbackType callback) {
    if (!callback)
        return;

    executeDelayed([this, callback]{
        callback(false, QnCameraBookmarkList(), invalidRequestId);
    });
}

void QnCameraBookmarksManagerPrivate::executeCallbackDelayed(BookmarksCallbackType callback) {
    if (!callback)
        return;

    executeDelayed([this, callback]{
        callback(false, QnCameraBookmarkList());
    });
}
