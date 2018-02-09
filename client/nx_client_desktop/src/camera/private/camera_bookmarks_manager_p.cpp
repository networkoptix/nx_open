#include "camera_bookmarks_manager_p.h"

#include <api/helpers/bookmark_request_data.h>

#include <camera/camera_bookmarks_manager.h>
#include <camera/camera_bookmarks_query.h>

#include <common/common_module.h>

#include <core/resource/camera_resource.h>
#include <core/resource/camera_history.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <utils/common/delayed.h>
#include <utils/common/synctime.h>
#include <utils/common/scoped_timer.h>

namespace {
    /** Each query can be updated no more often than once in that period. */
    const int minimimumRequestTimeoutMs = 10000;

    /** Timer precision to update all queries. */
    const int queriesCheckTimoutMs = 1000;

    /** Reserved value for invalid requests. */
    const int invalidRequestId = 0;

    /** Cache of bookmarks we have just added or removed. */
    const int pendingDiscardTimeout = 30000;
}

namespace {
QnCameraBookmarkList::iterator findBookmark(QnCameraBookmarkList &list, const QnUuid &bookmarkId)
{
    return std::find_if(list.begin(), list.end(), [&bookmarkId](const QnCameraBookmark &bookmark)
    {
        return bookmark.guid == bookmarkId;
    });
}

bool checkBookmarkForQuery(const QnCameraBookmarksQueryPtr &query, const QnCameraBookmark &bookmark)
{
    if (!query->cameras().isEmpty())
    {
        bool match = false;
        for (const QnVirtualCameraResourcePtr &camera : query->cameras())
        {
            if (bookmark.cameraId == camera->getId())
            {
                match = true;
                break;
            }
        }
        if (!match)
            return false;
    }

    return query->filter().checkBookmark(bookmark);
}
}


/************************************************************************/
/* OperationInfo                                                        */
/************************************************************************/
QnCameraBookmarksManagerPrivate::OperationInfo::OperationInfo()
    : operation(OperationType::Add)
    , callback()
{
}

QnCameraBookmarksManagerPrivate::OperationInfo::OperationInfo(
    OperationType operation,
    const QnUuid &bookmarkId,
    OperationCallbackType callback)
    : operation(operation)
    , callback(callback)
    , bookmarkId(bookmarkId)
{
}


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
    NX_ASSERT(false, Q_FUNC_INFO, "This functions is not to be used directly");
}

QnCameraBookmarksManagerPrivate::QueryInfo::QueryInfo(const QnCameraBookmarksQueryPtr &query)
    : queryRef(query.toWeakRef())
    , bookmarksCache()
    , state(QueryState::Invalid)
    , requestTimer()
    , requestId(invalidRequestId)
{
}


/************************************************************************/
/* PendingInfo                                                          */
/************************************************************************/
QnCameraBookmarksManagerPrivate::PendingInfo::PendingInfo(const QnCameraBookmark &bookmark, Type type)
    : bookmark(bookmark)
    , type(type)
{
    discardTimer.invalidate();
}

QnCameraBookmarksManagerPrivate::PendingInfo::PendingInfo(const QnUuid &bookmarkId, Type type)
    : type(type)
{
    bookmark.guid = bookmarkId;
    discardTimer.invalidate();
}


/************************************************************************/
/* QnCameraBookmarksManagerPrivate                                      */
/************************************************************************/
QnCameraBookmarksManagerPrivate::QnCameraBookmarksManagerPrivate(QnCameraBookmarksManager *parent)
    : base_type(parent)
    , q_ptr(parent)
    , m_operationsTimer(new QTimer(this))
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

    m_operationsTimer->setInterval(queriesCheckTimoutMs);
    m_operationsTimer->setSingleShot(false);
    connect(m_operationsTimer, &QTimer::timeout, this, &QnCameraBookmarksManagerPrivate::checkPendingBookmarks);
    connect(m_operationsTimer, &QTimer::timeout, this, &QnCameraBookmarksManagerPrivate::checkQueriesUpdate);

    connect(resourcePool(), &QnResourcePool::resourceRemoved, this, &QnCameraBookmarksManagerPrivate::removeCameraFromQueries);
}

QnCameraBookmarksManagerPrivate::~QnCameraBookmarksManagerPrivate()
{}

bool QnCameraBookmarksManagerPrivate::isEnabled() const
{
    return m_operationsTimer->isActive();
}

void QnCameraBookmarksManagerPrivate::setEnabled(bool value)
{
    if (isEnabled() == value)
        return;

    if (value)
    {
        m_operationsTimer->start();
        checkQueriesUpdate();
    }
    else
    {
        m_operationsTimer->stop();
    }
}

int QnCameraBookmarksManagerPrivate::getBookmarksAsync(const QnVirtualCameraResourceSet &cameras
    , const QnCameraBookmarkSearchFilter &filter
    , BookmarksInternalCallbackType callback)
{
    auto server = commonModule()->currentServer();
    if (!server)
    {
        executeCallbackDelayed(callback);
        return invalidRequestId;
    }

    auto connection = server->apiConnection();
    if (!connection)
    {
        executeCallbackDelayed(callback);
        return invalidRequestId;
    }

    QnGetBookmarksRequestData requestData;
    requestData.cameras = cameras.toList();
    requestData.filter = filter;
    requestData.format = Qn::SerializationFormat::UbjsonFormat;

    int requestId = connection->getBookmarksAsync(requestData, this, SLOT(handleDataLoaded(int, const QnCameraBookmarkList &, int)));
    if (callback)
        m_requests[requestId] = callback;
    return requestId;
}

void QnCameraBookmarksManagerPrivate::addCameraBookmark(
    const QnCameraBookmark &bookmark,
    OperationCallbackType callback)
{
    addCameraBookmarkInternal(bookmark, QnUuid(), callback);
}

void QnCameraBookmarksManagerPrivate::acknowledgeEvent(
    const QnCameraBookmark& bookmark,
    const QnUuid& eventRuleId,
    OperationCallbackType callback)
{
    addCameraBookmarkInternal(bookmark, eventRuleId, callback);
}

void QnCameraBookmarksManagerPrivate::addCameraBookmarkInternal(
    const QnCameraBookmark& bookmark,
    const QnUuid& eventRuleId,
    OperationCallbackType callback)
{
    NX_ASSERT(bookmark.isValid(), Q_FUNC_INFO, "Invalid bookmark must not be added");
    if (!bookmark.isValid())
        return;

    QnVirtualCameraResourcePtr camera = resourcePool()->getResourceById<QnVirtualCameraResource>(bookmark.cameraId);
    QnMediaServerResourcePtr server = cameraHistoryPool()->getMediaServerOnTime(camera, bookmark.startTimeMs);
    if (!server || server->getStatus() != Qn::Online)
        server = commonModule()->currentServer();

    if (!server)
    {
        executeDelayedParented([this, callback] { callback(false); }, this);
        return;
    }

    setEnabled(true); // Forcefully enable on modifying operation

    const auto operationType = !eventRuleId.isNull()
        ? OperationInfo::OperationType::Acknowledge
        : OperationInfo::OperationType::Add;

    const int handle = operationType == OperationInfo::OperationType::Acknowledge
        ? server->apiConnection()->acknowledgeEventAsync(
            bookmark, eventRuleId, this, SLOT(handleBookmarkOperation(int, int)))
        : server->apiConnection()->addBookmarkAsync(
            bookmark, this, SLOT(handleBookmarkOperation(int, int)));

    m_operations[handle] = OperationInfo(operationType, bookmark.guid, callback);

    addUpdatePendingBookmark(bookmark);
}

void QnCameraBookmarksManagerPrivate::updateCameraBookmark(const QnCameraBookmark &bookmark, OperationCallbackType callback)
{
    NX_ASSERT(bookmark.isValid(), Q_FUNC_INFO, "Invalid bookmark must not be added");
    if (!bookmark.isValid())
        return;

    setEnabled(true); // Forcefully enable on modifying operation
    int handle = commonModule()->currentServer()->apiConnection()->updateBookmarkAsync(bookmark, this, SLOT(handleBookmarkOperation(int, int)));
    m_operations[handle] = OperationInfo(OperationInfo::OperationType::Update, bookmark.guid, callback);

    addUpdatePendingBookmark(bookmark);
}

void QnCameraBookmarksManagerPrivate::deleteCameraBookmark(const QnUuid &bookmarkId, OperationCallbackType callback)
{
    setEnabled(true); // Forcefully enable on modifying operation
    int handle = commonModule()->currentServer()->apiConnection()->deleteBookmarkAsync(bookmarkId, this, SLOT(handleBookmarkOperation(int, int)));
    m_operations[handle] = OperationInfo(OperationInfo::OperationType::Delete, bookmarkId, callback);

    addRemovePendingBookmark(bookmarkId);
}

void QnCameraBookmarksManagerPrivate::handleBookmarkOperation(int status, int handle)
{
    if (!m_operations.contains(handle))
        return;

    auto operationInfo = m_operations.take(handle);
    if (operationInfo.callback)
        operationInfo.callback(status == 0);

    auto it = m_pendingBookmarks.find(operationInfo.bookmarkId);
    if (it != m_pendingBookmarks.end())
        it->discardTimer.start();
}


void QnCameraBookmarksManagerPrivate::handleDataLoaded(int status, const QnCameraBookmarkList &bookmarks, int requestId)
{
    if (!m_requests.contains(requestId))
        return;

    auto callback = m_requests.take(requestId);
    callback(status == 0, bookmarks, requestId);
}

void QnCameraBookmarksManagerPrivate::clearCache()
{
    m_requests.clear();
    m_pendingBookmarks.clear();

    for (auto queryId : m_queries.keys())
    {
        updateQueryCache(queryId, QnCameraBookmarkList());
        updateQueryAsync(queryId);
    }
}

bool QnCameraBookmarksManagerPrivate::isQueryUpdateRequired(const QUuid &queryId)
{
    NX_ASSERT(m_queries.contains(queryId), Q_FUNC_INFO, "Query should be registered");
    if (!m_queries.contains(queryId))
        return false;

    const QueryInfo info = m_queries[queryId];
    QnCameraBookmarksQueryPtr query = info.queryRef.toStrongRef();
    NX_ASSERT(query, Q_FUNC_INFO, "Interface does not allow query to be null");

    switch (info.state)
    {
        case QueryInfo::QueryState::Invalid:
            return true;
        case QueryInfo::QueryState::Queued:
            return !info.requestTimer.isValid() || info.requestTimer.hasExpired(minimimumRequestTimeoutMs) || !query->isValid();
        case QueryInfo::QueryState::Requested:
        case QueryInfo::QueryState::Actual:
            return false;
        default:
            NX_ASSERT(false, Q_FUNC_INFO, "Should never get here");
            break;
    }
    return false;
}


void QnCameraBookmarksManagerPrivate::checkQueriesUpdate()
{
    for (auto queryId : m_queries.keys())
    {
        bool updateRequired = isQueryUpdateRequired(queryId);
        if (!updateRequired)
            continue;

        const QueryInfo info = m_queries[queryId];
        QnCameraBookmarksQueryPtr query = info.queryRef.toStrongRef();
        NX_ASSERT(query, Q_FUNC_INFO, "Interface does not allow query to be null");
        if (!query)
            continue;

        executeQueryRemoteAsync(query, BookmarksCallbackType());
    }
}

void QnCameraBookmarksManagerPrivate::registerQuery(const QnCameraBookmarksQueryPtr &query)
{
    NX_ASSERT(query, Q_FUNC_INFO, "Interface does not allow query to be null");
    NX_ASSERT(!m_queries.contains(query->id()), Q_FUNC_INFO, "Query should be registered and unregistered only once");

    /* Do not store shared pointer in the functor so it can be safely unregistered. */
    QUuid queryId = query->id();
    m_queries.insert(queryId, QueryInfo(query));
    connect(query, &QnCameraBookmarksQuery::queryChanged, this, [this, queryId]
    {
        updateQueryAsync(queryId);
    });

    updateQueryAsync(queryId);
}

void QnCameraBookmarksManagerPrivate::unregisterQuery(const QUuid &queryId)
{
    NX_ASSERT(m_queries.contains(queryId), Q_FUNC_INFO, "Query should be registered and unregistered only once");
    m_queries.remove(queryId);
}

QnCameraBookmarkList QnCameraBookmarksManagerPrivate::cachedBookmarks(const QnCameraBookmarksQueryPtr &query) const
{
    NX_ASSERT(query, Q_FUNC_INFO, "Interface does not allow query to be null");
    QN_LOG_TIME(Q_FUNC_INFO);

    /* Check if we have already cached query result. */
    if (m_queries.contains(query->id()))
        return m_queries[query->id()].bookmarksCache;

    return QnCameraBookmarkList();
}

void QnCameraBookmarksManagerPrivate::executeQueryRemoteAsync(const QnCameraBookmarksQueryPtr &query, BookmarksCallbackType callback)
{
    NX_ASSERT(query, Q_FUNC_INFO, "Interface does not allow query to be null");
    if (!query)
    {
        executeCallbackDelayed(callback);
        return;
    }

    /* Do not store shared pointer here so it can be safely unregistered in the process. */
    QUuid queryId = query->id();

    NX_ASSERT(m_queries.contains(queryId), Q_FUNC_INFO, "Query must be registered");
    if (!m_queries.contains(queryId))
    {
        executeCallbackDelayed(callback);
        return;
    }

    QueryInfo &info = m_queries[queryId];
    info.requestTimer.restart();

    if (!query->isValid())
    {
        info.state = QueryInfo::QueryState::Actual;

        if (callback)
            callback(false, QnCameraBookmarkList());

        return;
    }

    info.state = QueryInfo::QueryState::Requested;
    info.requestId = getBookmarksAsync(query->cameras(), query->filter(),
        [this, queryId, callback](bool success, QnCameraBookmarkList bookmarks, int requestId)
    {
        if (success && m_queries.contains(queryId) && requestId != invalidRequestId)
        {
            QueryInfo &info = m_queries[queryId];
            if (info.requestId == requestId)
            {
                const QnCameraBookmarksQueryPtr &query = info.queryRef.toStrongRef();
                if (!query)
                    return;

                mergeWithPendingBookmarks(query, bookmarks);
                updateQueryCache(queryId, bookmarks);
                if (info.state == QueryInfo::QueryState::Requested)
                    info.state = QueryInfo::QueryState::Actual;
            }
        }
        if (callback)
            callback(success, bookmarks);
    });
}

void QnCameraBookmarksManagerPrivate::updateQueryAsync(const QUuid &queryId)
{
    if (!m_queries.contains(queryId))
        return;

    QN_LOG_TIME(Q_FUNC_INFO);
    QueryInfo &info = m_queries[queryId];
    info.state = QueryInfo::QueryState::Queued;
}

void QnCameraBookmarksManagerPrivate::updateQueryCache(const QUuid &queryId, const QnCameraBookmarkList &bookmarks)
{
    QN_LOG_TIME(Q_FUNC_INFO);

    NX_ASSERT(m_queries.contains(queryId), Q_FUNC_INFO, "Query must be registered");
    if (!m_queries.contains(queryId))
        return;

    QnCameraBookmarksQueryPtr query = m_queries[queryId].queryRef.toStrongRef();
    NX_ASSERT(query, Q_FUNC_INFO, "Interface does not allow query to be null");
    if (!query)
        return;

    auto cachedValue = m_queries[queryId].bookmarksCache;
    m_queries[queryId].bookmarksCache = bookmarks;
    if (cachedValue != bookmarks)
        emit query->bookmarksChanged(bookmarks);
}

QnCameraBookmarksQueryPtr QnCameraBookmarksManagerPrivate::createQuery(const QnVirtualCameraResourceSet &cameras, const QnCameraBookmarkSearchFilter &filter)
{
    QnCameraBookmarksQueryPtr query(new QnCameraBookmarksQuery(cameras, filter));
    QUuid queryId = query->id();
    registerQuery(query);

    connect(query, &QObject::destroyed, this, [this, queryId]()
    {
        unregisterQuery(queryId);
    });

    return query;
}

void QnCameraBookmarksManagerPrivate::executeCallbackDelayed(BookmarksInternalCallbackType callback)
{
    if (!callback)
        return;

    const auto timerCallback =
        [this, callback]
        {
            callback(false, QnCameraBookmarkList(), invalidRequestId);
        };

    executeDelayedParented(timerCallback, this);
}

void QnCameraBookmarksManagerPrivate::executeCallbackDelayed(BookmarksCallbackType callback)
{
    if (!callback)
        return;

    const auto timerCallback =
        [this, callback]
        {
            callback(false, QnCameraBookmarkList());
        };

    executeDelayedParented(timerCallback, this);
}

void QnCameraBookmarksManagerPrivate::addUpdatePendingBookmark(const QnCameraBookmark &bookmark)
{
    m_pendingBookmarks.insert(bookmark.guid, PendingInfo(bookmark, PendingInfo::AddBookmark));

    for (QueryInfo &info : m_queries)
    {
        QnCameraBookmarksQueryPtr query = info.queryRef.toStrongRef();
        NX_ASSERT(query, Q_FUNC_INFO, "Interface does not allow query to be null");
        if (!query)
            continue;

        if (!checkBookmarkForQuery(query, bookmark))
            continue;

        auto it = findBookmark(info.bookmarksCache, bookmark.guid);
        if (it != info.bookmarksCache.end())
            info.bookmarksCache.erase(it);

        it = std::lower_bound(info.bookmarksCache.begin(), info.bookmarksCache.end(), bookmark);
        info.bookmarksCache.insert(it, bookmark);

        emit query->bookmarksChanged(info.bookmarksCache);
    }
}

void QnCameraBookmarksManagerPrivate::addRemovePendingBookmark(const QnUuid &bookmarkId)
{
    m_pendingBookmarks.insert(bookmarkId, PendingInfo(bookmarkId, PendingInfo::RemoveBookmark));

    for (QueryInfo &info : m_queries)
    {
        QnCameraBookmarksQueryPtr query = info.queryRef.toStrongRef();
        NX_ASSERT(query, Q_FUNC_INFO, "Interface does not allow query to be null");
        if (!query)
            continue;

        auto it = findBookmark(info.bookmarksCache, bookmarkId);
        if (it == info.bookmarksCache.end())
            continue;

        info.bookmarksCache.erase(it);

        emit query->bookmarksChanged(info.bookmarksCache);
    }
}

void QnCameraBookmarksManagerPrivate::mergeWithPendingBookmarks(const QnCameraBookmarksQueryPtr query, QnCameraBookmarkList &bookmarks)
{
    QN_LOG_TIME(Q_FUNC_INFO);

    for (const PendingInfo &info : m_pendingBookmarks)
    {
        if (info.type == PendingInfo::RemoveBookmark)
        {
            auto it = findBookmark(bookmarks, info.bookmark.guid);
            if (it == bookmarks.end())
                continue;

            bookmarks.erase(it);
        }
        else
        {
            if (!checkBookmarkForQuery(query, info.bookmark))
                continue;

            auto it = findBookmark(bookmarks, info.bookmark.guid);
            if (it != bookmarks.end())
            {
                *it = info.bookmark;
            }
            else
            {
                it = std::lower_bound(bookmarks.begin(), bookmarks.end(), info.bookmark);
                bookmarks.insert(it, info.bookmark);
            }

        }
    }
}

void QnCameraBookmarksManagerPrivate::checkPendingBookmarks()
{
    QN_LOG_TIME(Q_FUNC_INFO);

    for (auto it = m_pendingBookmarks.begin(); it != m_pendingBookmarks.end(); /* no inc */)
    {
        if (it->discardTimer.isValid() && it->discardTimer.hasExpired(pendingDiscardTimeout))
            it = m_pendingBookmarks.erase(it);
        else
            ++it;
    }
}

void QnCameraBookmarksManagerPrivate::removeCameraFromQueries(const QnResourcePtr& resource)
{
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    QnUuid cameraId = camera->getId();

    for (QueryInfo &info : m_queries)
    {
        QnCameraBookmarksQueryPtr query = info.queryRef.toStrongRef();
        NX_ASSERT(query, Q_FUNC_INFO, "Interface does not allow query to be null");
        if (!query)
            continue;

        if (!query->removeCamera(camera))
            continue;

        bool changed = false;
        for (auto it = info.bookmarksCache.begin(); it != info.bookmarksCache.end(); )
        {
            if (it->cameraId == cameraId)
            {
                it = info.bookmarksCache.erase(it);
                changed = true;
            }
            else
            {
                ++it;
            }
        }

        if (changed)
            emit query->bookmarksChanged(info.bookmarksCache);
    }

}
