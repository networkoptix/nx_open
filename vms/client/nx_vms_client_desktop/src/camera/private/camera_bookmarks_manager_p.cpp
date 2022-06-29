// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_bookmarks_manager_p.h"

#include <api/helpers/bookmark_request_data.h>
#include <api/server_rest_connection.h>
#include <camera/camera_bookmarks_manager.h>
#include <camera/camera_bookmarks_query.h>
#include <common/common_module.h>
#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/rest/params.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/desktop/system_context.h>
#include <utils/common/delayed.h>
#include <utils/common/scoped_timer.h>
#include <utils/common/synctime.h>

using namespace nx::vms::client::desktop;

namespace {
    /** Each query can be updated no more often than once in that period. */
    const int minimimumRequestTimeoutMs = 10000;

    /** Timer precision to update all queries. */
    const int queriesCheckTimoutMs = 1000;

    /** Reserved value for invalid requests. */
    const int kInvalidRequestId = 0;

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
    if (!query->cameras().empty())
    {
        bool match = false;
        for (const auto& cameraId: query->cameras())
        {
            if (bookmark.cameraId == cameraId)
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

template<class ResponseType>
std::function<void (bool, rest::Handle, QByteArray, nx::network::http::HttpHeaders)> makeUbjsonResponseWrapper(
    std::function<void (bool, int, const ResponseType&)> internalCallback)
{
    return [internalCallback](bool success, rest::Handle handle, QByteArray response,
        nx::network::http::HttpHeaders /*headers*/)
        {
            if (success)
            {
                bool parsed = false;
                ResponseType reply = QnUbjson::deserialized(response, ResponseType(), &parsed);
                if (parsed)
                    internalCallback(true, handle, reply);
            }
            if (!success)
                internalCallback(success, handle, {});
        };
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
    , requestId(kInvalidRequestId)
{
    NX_ASSERT(false, "This functions is not to be used directly");
}

QnCameraBookmarksManagerPrivate::QueryInfo::QueryInfo(const QnCameraBookmarksQueryPtr &query)
    : queryRef(query.toWeakRef())
    , bookmarksCache()
    , state(QueryState::Invalid)
    , requestTimer()
    , requestId(kInvalidRequestId)
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
QnCameraBookmarksManagerPrivate::QnCameraBookmarksManagerPrivate(
    SystemContext* systemContext,
    QObject* parent)
    :
    base_type(parent),
    SystemContextAware(systemContext),
    m_operationsTimer(new QTimer(this))
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

std::optional<QnUuid> QnCameraBookmarksManagerPrivate::getServerForBookmark(
    const QnCameraBookmark& bookmark)
{
    QnVirtualCameraResourcePtr camera =
        resourcePool()->getResourceById<QnVirtualCameraResource>(bookmark.cameraId);
    if (!camera)
        return {};

    QnMediaServerResourcePtr server =
        cameraHistoryPool()->getMediaServerOnTime(camera, bookmark.startTimeMs.count());
    if (!server || server->getStatus() != nx::vms::api::ResourceStatus::online)
        return {};

    return server->getId();
}

int QnCameraBookmarksManagerPrivate::sendPostRequest(
    const QString& path,
    QnMultiserverRequestData &request,
    std::optional<QnUuid> serverId)
{
    if (!connection())
        return kInvalidRequestId;

    request.format = Qn::SerializationFormat::UbjsonFormat;

    auto callback = nx::utils::guarded(this,
        [this](
            bool success,
            int handle,
            const rest::ServerConnection::EmptyResponseType& /*response*/)
        {
            handleBookmarkOperation(success, handle);
        });

    return connectedServerApi()->postEmptyResult(
        path,
        request.toParams(),
        /*body*/ {},
        callback,
        thread(),
        serverId);
}

int QnCameraBookmarksManagerPrivate::sendGetRequest(
    const QString& path,
    QnMultiserverRequestData &request,
    RawResponseType callback)
{
    if (!connection())
    {
        executeDelayedParented([callback]() { callback(false, kInvalidRequestId, {}, {}); }, this);
        return kInvalidRequestId;
    }

    request.format = Qn::SerializationFormat::UbjsonFormat;

    auto guardedCallback = nx::utils::guarded(this, callback);
    return connectedServerApi()->getRawResult(path, request.toParams(), guardedCallback, thread());
}

int QnCameraBookmarksManagerPrivate::getBookmarksAsync(
    const QnCameraBookmarkSearchFilter &filter,
    BookmarksCallbackType callback)
{
    QnGetBookmarksRequestData requestData;
    requestData.filter = filter;
    requestData.format = Qn::SerializationFormat::UbjsonFormat;
    return sendGetRequest("/ec2/bookmarks", requestData, makeUbjsonResponseWrapper(callback));
}

int QnCameraBookmarksManagerPrivate::getBookmarkTagsAsync(
    int maxTags, BookmarkTagsCallbackType callback)
{
    QnGetBookmarkTagsRequestData requestData;
    requestData.limit = maxTags;
    requestData.format = Qn::SerializationFormat::UbjsonFormat;
    return sendGetRequest("/ec2/bookmarks/tags", requestData, makeUbjsonResponseWrapper(callback));
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
    NX_ASSERT(bookmark.isValid(), "Invalid bookmark must not be added");
    if (!bookmark.isValid())
        return;

    const auto serverId = getServerForBookmark(bookmark);
    setEnabled(true); // Forcefully enable on modifying operation

    const auto operationType = !eventRuleId.isNull()
        ? OperationInfo::OperationType::Acknowledge
        : OperationInfo::OperationType::Add;

    int handle = kInvalidRequestId;

    if (operationType == OperationInfo::OperationType::Acknowledge)
    {
        QnUpdateBookmarkRequestData request(bookmark, eventRuleId);
        handle = sendPostRequest("/ec2/bookmarks/acknowledge", request, serverId);
    }
    else
    {
        QnUpdateBookmarkRequestData request(bookmark);
        handle = sendPostRequest("/ec2/bookmarks/add", request, serverId);
    }

    if (handle != kInvalidRequestId)
        m_operations[handle] = OperationInfo(operationType, bookmark.guid, callback);
    else
    {
        callback(false);
    }

    addUpdatePendingBookmark(bookmark);
}

void QnCameraBookmarksManagerPrivate::updateCameraBookmark(
    const QnCameraBookmark &bookmark,
    OperationCallbackType callback)
{
    NX_ASSERT(bookmark.isValid(), "Invalid bookmark must not be added");
    if (!bookmark.isValid())
        return;

    setEnabled(true); // Forcefully enable on modifying operation

    QnUpdateBookmarkRequestData request(bookmark);
    int handle = sendPostRequest("/ec2/bookmarks/update", request);
    m_operations[handle] = OperationInfo(
        OperationInfo::OperationType::Update, bookmark.guid, callback);

    addUpdatePendingBookmark(bookmark);
}

void QnCameraBookmarksManagerPrivate::deleteCameraBookmark(
    const QnUuid &bookmarkId,
    OperationCallbackType callback)
{
    setEnabled(true); // Forcefully enable on modifying operation
    QnDeleteBookmarkRequestData request(bookmarkId);
    int handle = sendPostRequest("/ec2/bookmarks/delete", request);
    m_operations[handle] = OperationInfo(
        OperationInfo::OperationType::Delete, bookmarkId, callback);

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

bool QnCameraBookmarksManagerPrivate::isQueryUpdateRequired(const QUuid &queryId)
{
    NX_ASSERT(m_queries.contains(queryId), "Query should be registered");
    if (!m_queries.contains(queryId))
        return false;

    const QueryInfo info = m_queries[queryId];
    QnCameraBookmarksQueryPtr query = info.queryRef.toStrongRef();
    NX_ASSERT(query, "Interface does not allow query to be null");

    switch (info.state)
    {
        case QueryInfo::QueryState::Invalid:
            return true;
        case QueryInfo::QueryState::Queued:
            return !info.requestTimer.isValid()
                || info.requestTimer.hasExpired(minimimumRequestTimeoutMs) || !query->isValid();
        case QueryInfo::QueryState::Requested:
        case QueryInfo::QueryState::Actual:
            return false;
        default:
            NX_ASSERT(false, "Should never get here");
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
        NX_ASSERT(query, "Interface does not allow query to be null");
        if (!query)
            continue;

        executeQueryRemoteAsync(query, BookmarksCallbackType());
    }
}

void QnCameraBookmarksManagerPrivate::registerQuery(const QnCameraBookmarksQueryPtr &query)
{
    NX_ASSERT(query, "Interface does not allow query to be null");
    NX_ASSERT(!m_queries.contains(query->id()), "Query should be registered and unregistered only once");

    /* Do not store shared pointer in the functor so it can be safely unregistered. */
    QUuid queryId = query->id();
    m_queries.insert(queryId, QueryInfo(query));
    connect(query.get(), &QnCameraBookmarksQuery::queryChanged, this, [this, queryId]
    {
        updateQueryAsync(queryId);
    });

    updateQueryAsync(queryId);
}

void QnCameraBookmarksManagerPrivate::unregisterQuery(const QUuid &queryId)
{
    NX_ASSERT(m_queries.contains(queryId), "Query should be registered and unregistered only once");
    m_queries.remove(queryId);
}

QnCameraBookmarkList QnCameraBookmarksManagerPrivate::cachedBookmarks(const QnCameraBookmarksQueryPtr &query) const
{
    NX_ASSERT(query, "Interface does not allow query to be null");

    /* Check if we have already cached query result. */
    if (m_queries.contains(query->id()))
        return m_queries[query->id()].bookmarksCache;

    return QnCameraBookmarkList();
}

void QnCameraBookmarksManagerPrivate::executeQueryRemoteAsync(
    const QnCameraBookmarksQueryPtr &query, BookmarksCallbackType callback)
{
    NX_ASSERT(query, "Interface does not allow query to be null");
    if (!query)
    {
        executeCallbackDelayed(callback);
        return;
    }

    /* Do not store shared pointer here so it can be safely unregistered in the process. */
    QUuid queryId = query->id();

    NX_ASSERT(m_queries.contains(queryId), "Query must be registered");
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
            callback(false, kInvalidRequestId, QnCameraBookmarkList());

        return;
    }

    info.state = QueryInfo::QueryState::Requested;
    info.requestId = getBookmarksAsync(query->filter(),
        [this, queryId, callback](bool success, int requestId, QnCameraBookmarkList bookmarks)
    {
        if (success && m_queries.contains(queryId) && requestId != kInvalidRequestId)
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
            callback(success, requestId, bookmarks);
    });
}

void QnCameraBookmarksManagerPrivate::updateQueryAsync(const QUuid &queryId)
{
    if (!m_queries.contains(queryId))
        return;

    QueryInfo &info = m_queries[queryId];
    info.state = QueryInfo::QueryState::Queued;
}

void QnCameraBookmarksManagerPrivate::updateQueryCache(
    const QUuid &queryId, const QnCameraBookmarkList &bookmarks)
{
    NX_ASSERT(m_queries.contains(queryId), "Query must be registered");
    if (!m_queries.contains(queryId))
        return;

    QnCameraBookmarksQueryPtr query = m_queries[queryId].queryRef.toStrongRef();
    NX_ASSERT(query, "Interface does not allow query to be null");
    if (!query)
        return;

    auto cachedValue = m_queries[queryId].bookmarksCache;
    m_queries[queryId].bookmarksCache = bookmarks;
    if (cachedValue != bookmarks)
        emit query->bookmarksChanged(bookmarks);
}

QnCameraBookmarksQueryPtr QnCameraBookmarksManagerPrivate::createQuery(const QnCameraBookmarkSearchFilter& filter)
{
    QnCameraBookmarksQueryPtr query(new QnCameraBookmarksQuery(systemContext(), filter));
    QUuid queryId = query->id();
    registerQuery(query);

    connect(query.get(), &QObject::destroyed, this, [this, queryId]()
    {
        unregisterQuery(queryId);
    });

    return query;
}

void QnCameraBookmarksManagerPrivate::executeCallbackDelayed(BookmarksCallbackType callback)
{
    if (!callback)
        return;

    const auto timerCallback =
        [callback]
        {
            callback(false, kInvalidRequestId, QnCameraBookmarkList());
        };

    executeDelayedParented(timerCallback, this);
}

void QnCameraBookmarksManagerPrivate::addUpdatePendingBookmark(const QnCameraBookmark &bookmark)
{
    m_pendingBookmarks.insert(bookmark.guid, PendingInfo(bookmark, PendingInfo::AddBookmark));

    for (QueryInfo &info : m_queries)
    {
        QnCameraBookmarksQueryPtr query = info.queryRef.toStrongRef();
        NX_ASSERT(query, "Interface does not allow query to be null");
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
        NX_ASSERT(query, "Interface does not allow query to be null");
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
        NX_ASSERT(query, "Interface does not allow query to be null");
        if (!query)
            continue;

        if (!query->removeCamera(camera->getId()))
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
