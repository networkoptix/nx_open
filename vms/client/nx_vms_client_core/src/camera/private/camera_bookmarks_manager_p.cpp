// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_bookmarks_manager_p.h"

#include <api/helpers/bookmark_request_data.h>
#include <api/server_rest_connection.h>
#include <camera/camera_bookmarks_manager.h>
#include <camera/camera_bookmarks_query.h>
#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/fusion/serialization/ubjson.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/rest/params.h>
#include <nx/utils/algorithm/merge_sorted_lists.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/utils/datetime.h>
#include <nx/vms/api/data/bookmark_models.h>
#include <nx/vms/client/core/event_search/event_search_globals.h>
#include <nx/vms/client/core/event_search/utils/event_search_item_helper.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/utils/log_strings_format.h>
#include <nx/vms/common/api/helpers/bookmark_api_converter.h>
#include <nx/vms/common/bookmark/bookmark_facade.h>
#include <utils/common/delayed.h>
#include <utils/common/scoped_timer.h>
#include <utils/common/synctime.h>

using namespace nx::vms::client::core;

namespace {

/** Each query can be updated no more often than once in that period. */
static constexpr int kMinimimumRequestTimeoutMs = 10000;

/** Timer precision to update all queries. */
static constexpr int kQueriesCheckTimeoutMs = 1000;

/** Reserved value for invalid requests. */
static constexpr auto kInvalidRequestId = rest::Handle();

QnCameraBookmarkList::iterator findBookmark(QnCameraBookmarkList& list, const nx::Uuid& bookmarkId)
{
    return std::find_if(list.begin(), list.end(), [&bookmarkId](const QnCameraBookmark& bookmark)
    {
        return bookmark.guid == bookmarkId;
    });
}

bool checkBookmarkForQuery(const QnCameraBookmarksQueryPtr& query, const QnCameraBookmark& bookmark)
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
QnCameraBookmarksManagerPrivate::RawResponseType makeUbjsonResponseWrapper(
    nx::utils::AsyncHandlerExecutor executor,
    std::function<void (bool, rest::Handle, ResponseType)> callback)
{
    return
        [internalCallback = executor.bind(std::move(callback))](
            bool success,
            rest::Handle handle,
            QByteArray body,
            nx::network::http::HttpHeaders /*headers*/) mutable
        {
            ResponseType response;

            if (success)
                response = QnUbjson::deserialized(body, ResponseType(), &success);

            internalCallback(success, handle, std::move(response));
        };
}

QString debugBookmarksRange(const QnCameraBookmarkList& bookmarks)
{
    if (bookmarks.empty())
        return "empty";

    return nx::format("%1 - %2",
        nx::utils::timestampToDebugString(bookmarks.front().startTimeMs),
        nx::utils::timestampToDebugString(bookmarks.back().endTime()));
}

enum class CheckResult
{
    dataFound,
    tryMore
};

/**
 *  Result structure for fetched data check. Contains successful mark of operation and new central
 *  point to fetch in case of check fail.
 */
struct CheckResultData
{
    CheckResult result = CheckResult::tryMore;
    std::chrono::milliseconds nextCentralPointMs;
};

/**
 * Checks if fetched data contains enough tail. Used in bookmarks requests to old servers (with
 * version less than 6.0) where we don't have central point parameter. Thus we have to request data
 * sequentially and check if results have enough tail and body.
 */
CheckResultData checkFetchedData(
    const FetchedData<QnCameraBookmarkList>& fetched,
    int length,
    EventSearch::FetchDirection direction)
{
    const int maxBodyLength = length / 2;
    const int maxTailLength = length - maxBodyLength;

    if (fetched.data.empty())
        return CheckResultData{CheckResult::dataFound, {}};

    const bool fetchedAllNeeded =
        fetched.ranges.tail.length + fetched.ranges.body.length < maxBodyLength + maxTailLength;

    // If data size is less than maximum we expect that we found all existing records.
    // Also, as our request uses central point (start time or end time) of currently existing data
    // (at least at the moment of last request) we suppose that maxTailLength/2 data size is enough
    // to be represented to user.
    if (fetchedAllNeeded || fetched.ranges.tail.length > maxTailLength / 2)
        return CheckResultData{CheckResult::dataFound, {}};

    if (fetched.data.begin()->startTimeMs >= fetched.data.rbegin()->startTimeMs)
        return CheckResultData{CheckResult::tryMore, {}};

    // Looking for the new period to be fetched to get an appropriate tail data.
    const int shift = std::min(maxTailLength / 2, fetched.ranges.body.length / 2);

    if (direction == EventSearch::FetchDirection::older)
    {
        auto newBodyStartTime = fetched.data.at(shift).startTimeMs;
        const auto bodyStartTime = fetched.data.begin()->startTimeMs;
        if (newBodyStartTime >= bodyStartTime && bodyStartTime.count())
            newBodyStartTime = bodyStartTime - 1ms;

        return CheckResultData{CheckResult::tryMore, newBodyStartTime};
    }

    auto newBodyStartTime = fetched.data.at(int(fetched.data.size()) - shift).startTimeMs;
    const auto bodyStartTime = fetched.data.rbegin()->startTimeMs;
    if (newBodyStartTime <= bodyStartTime && bodyStartTime.count() != QnTimePeriod::kMaxTimeValue)
        newBodyStartTime = bodyStartTime + 1ms;

    return CheckResultData{CheckResult::tryMore, newBodyStartTime};
}

nx::network::http::HttpHeaders makeProxyToServerHeaders(const std::optional<nx::Uuid>& serverId)
{
    if (!serverId)
        return {};

    nx::network::http::HttpHeaders result;
    nx::network::http::insertHeader(
        &result, {Qn::SERVER_GUID_HEADER_NAME, serverId.value().toSimpleStdString()});

    return result;
}

} // namespace

/************************************************************************/
/* QueryInfo                                                            */
/************************************************************************/
QnCameraBookmarksManagerPrivate::QueryInfo::QueryInfo()
    : queryRef()
    , requestTimer()
    , requestId(kInvalidRequestId)
{
}

QnCameraBookmarksManagerPrivate::QueryInfo::QueryInfo(const QnCameraBookmarksQueryPtr& query)
    : queryRef(query.toWeakRef())
    , requestTimer()
    , requestId(kInvalidRequestId)
{
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

    m_operationsTimer->setInterval(kQueriesCheckTimeoutMs);
    m_operationsTimer->setSingleShot(false);
    connect(m_operationsTimer, &QTimer::timeout, this, &QnCameraBookmarksManagerPrivate::checkQueriesUpdate);

    connect(resourcePool(), &QnResourcePool::resourcesRemoved, this,
        [this](const QnResourceList& resources)
        {
            for (const auto& resource: resources)
                removeCameraFromQueries(resource);
        });
}

QnCameraBookmarksManagerPrivate::~QnCameraBookmarksManagerPrivate()
{
}

void QnCameraBookmarksManagerPrivate::startOperationsTimer()
{
    if (!m_operationsTimer->isActive())
    {
        m_operationsTimer->start();
        checkQueriesUpdate();
    }
}

rest::Handle QnCameraBookmarksManagerPrivate::sendRestRequest(
    const QString& path,
    const nx::vms::api::BookmarkV3& bookmark,
    const nx::network::http::Method& method,
    RestRequestCallback&& callback)
{
    auto serverApi = connectedServerApi();

    if (!serverApi)
        return kInvalidRequestId;

    const auto body = method == nx::network::http::Method::delete_
        ? std::string()
        : nx::reflect::json::serialize(bookmark);

    const auto helper = systemContext()->mode() == nx::vms::client::core::SystemContext::Mode::client
        ? systemContext()->getSessionTokenHelper()
        : nx::vms::common::SessionTokenHelperPtr{};

    return serverApi->sendRequest<rest::ErrorOrData<QByteArray>>(
        helper,
        method,
        path,
        nx::network::rest::Params{},
        body,
        std::move(callback),
        this);
}

int QnCameraBookmarksManagerPrivate::sendGetRequest(
    const QString& path,
    QnMultiserverRequestData& request,
    RawResponseType callback)
{
    const auto serverApi = connectedServerApi();

    if (!serverApi)
    {
        executeDelayedParented([callback]() { callback(false, kInvalidRequestId, {}, {}); }, this);
        return kInvalidRequestId;
    }

    request.format = Qn::SerializationFormat::ubjson;

    return serverApi->getRawResult(path, request.toParams(), std::move(callback));
}

int QnCameraBookmarksManagerPrivate::getBookmarksAsync(
    const QnCameraBookmarkSearchFilter& filter,
    BookmarksCallbackType callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    nx::vms::common::GetBookmarksRequestData requestData;
    requestData.filter = filter;
    requestData.format = Qn::SerializationFormat::ubjson;
    if (!NX_ASSERT(requestData.filter.startTimeMs.count() >= 0, "Invalid start time passed"))
        requestData.filter.startTimeMs = {};
    if (!NX_ASSERT(requestData.filter.endTimeMs.count() >= 0, "Invalid end time passed"))
        requestData.filter.endTimeMs = {};

    return sendGetRequest(
        "/ec2/bookmarks",
        requestData,
        makeUbjsonResponseWrapper(std::move(executor), callback));
}

int QnCameraBookmarksManagerPrivate::getBookmarksAroundPoint_compatibilityMode(
    const QnCameraBookmarkSearchFilter& filter,
    BookmarksCallbackType callback)
{
    if (!NX_ASSERT(filter.centralTimePointMs.has_value()))
        return {};

    using namespace nx::vms::client::core;

    const bool validRange = filter.endTimeMs == milliseconds{}
        || filter.startTimeMs <= filter.endTimeMs;

    const auto startTimeMs = validRange ? filter.startTimeMs : filter.endTimeMs;
    const auto endTimeMs = validRange ? filter.endTimeMs : filter.startTimeMs;
    const auto effectiveEndTimeMs = endTimeMs != milliseconds{} ? endTimeMs : milliseconds::max();

    const auto centralTimePointMs =
        std::clamp(*filter.centralTimePointMs, startTimeMs - 1ms, effectiveEndTimeMs);

    const bool forwardLookup = centralTimePointMs < effectiveEndTimeMs;
    const bool backwardLookup = centralTimePointMs >= startTimeMs;

    struct Context
    {
        int completed = 0;
        int expected = 0;
        bool success = true;
        rest::Handle forwardHandle = {};
        rest::Handle backwardHandle = {};
        rest::Handle resultHandle = {};
        QnCameraBookmarkList forwardData;
        QnCameraBookmarkList backwardData;
        BookmarksCallbackType callback;
    };

    const auto context = std::make_shared<Context>();
    context->callback = std::move(callback);
    context->expected = (forwardLookup ? 1 : 0) + (backwardLookup ? 1 : 0);

    const auto commonCallback =
        [context, order = filter.orderBy.order](
            bool success, rest::Handle handle, QnCameraBookmarkList result)
        {
            if (handle == kInvalidRequestId)
            {
                context->success = false;
                ++context->completed;
            }
            else
            {
                if (!NX_ASSERT(handle == context->forwardHandle
                    || handle == context->backwardHandle))
                {
                    return;
                }

                // Store received data in the context.
                context->success &= success;
                ++context->completed;

                if (handle == context->forwardHandle)
                    context->forwardData = std::move(result);
                else
                    context->backwardData = std::move(result);
            }

            if (context->completed < context->expected)
                return;

            // Both requests are completed (or failed) at this point.

            if (!context->success)
            {
                context->callback(/*success*/ false, context->resultHandle, {});
                return;
            }

            // Merge received data.
            const bool ascending = order == Qt::AscendingOrder;
            if (ascending)
                std::reverse(context->backwardData.begin(), context->backwardData.end());
            else
                std::reverse(context->forwardData.begin(), context->forwardData.end());

            std::vector<QnCameraBookmarkList> lists;
            lists.reserve(2);
            lists.push_back(std::move(context->forwardData));
            lists.push_back(std::move(context->backwardData));

            auto mergedList = nx::utils::algorithm::merge_sorted_lists(std::move(lists),
                [](const QnCameraBookmark& b) { return std::make_tuple(b.startTimeMs, b.guid); },
                order);

            context->callback(/*success*/ true, context->resultHandle, std::move(mergedList));
        };

    // `commonCallback` is single-threaded, so execute it in the thread of `this`.

    if (forwardLookup)
    {
        QnCameraBookmarkSearchFilter forwardFilter(filter);
        forwardFilter.orderBy.order = Qt::AscendingOrder;
        forwardFilter.limit /= 2;
        forwardFilter.startTimeMs = centralTimePointMs + 1ms;
        forwardFilter.endTimeMs = endTimeMs;
        forwardFilter.centralTimePointMs = {};
        context->resultHandle = context->forwardHandle = getBookmarksAsync(
            forwardFilter, commonCallback, /*executor*/ this);
    }

    if (backwardLookup)
    {
        QnCameraBookmarkSearchFilter backwardFilter(filter);
        backwardFilter.orderBy.order = Qt::DescendingOrder;
        backwardFilter.limit /= 2;
        backwardFilter.startTimeMs = startTimeMs;
        backwardFilter.endTimeMs = centralTimePointMs;
        backwardFilter.centralTimePointMs = {};
        context->resultHandle = context->backwardHandle = getBookmarksAsync(
            backwardFilter, commonCallback, /*executor*/ this);
    }

    return context->resultHandle;
}

int QnCameraBookmarksManagerPrivate::getBookmarkTagsAsync(
    int maxTags, BookmarkTagsCallbackType callback)
{
    nx::vms::common::GetBookmarkTagsRequestData requestData;
    requestData.limit = maxTags;
    requestData.format = Qn::SerializationFormat::ubjson;

    return sendGetRequest(
        "/ec2/bookmarks/tags",
        requestData,
        makeUbjsonResponseWrapper(this, callback));
}

void QnCameraBookmarksManagerPrivate::addOrUpdateBookmarkInQueries(
    const QnCameraBookmark& bookmark)
{
    for (QueryInfo& info: m_queries)
    {
        QnCameraBookmarksQueryPtr query = info.queryRef.toStrongRef();
        if (!NX_ASSERT(query, "Interface does not allow query to be null"))
            continue;

        if (!checkBookmarkForQuery(query, bookmark))
            continue;

        auto bookmarks = query->cachedBookmarks();
        auto it = findBookmark(bookmarks, bookmark.guid);
        if (it != bookmarks.end())
            bookmarks.erase(it);

        it = std::lower_bound(bookmarks.begin(), bookmarks.end(), bookmark);
        bookmarks.insert(it, bookmark);
        query->setCachedBookmarks(std::move(bookmarks));
    }
}

void QnCameraBookmarksManagerPrivate::removeCameraBookmarkFromQueries(const nx::Uuid& bookmarkId)
{
    for (QueryInfo& info: m_queries)
    {
        QnCameraBookmarksQueryPtr query = info.queryRef.toStrongRef();
        if (!NX_ASSERT(query, "Interface does not allow query to be null"))
            continue;

        auto bookmarks = query->cachedBookmarks();
        auto it = findBookmark(bookmarks, bookmarkId);
        if (it == bookmarks.end())
            continue;

        bookmarks.erase(it);
        query->setCachedBookmarks(std::move(bookmarks));
    }
}

bool QnCameraBookmarksManagerPrivate::isQueryUpdateRequired(const QUuid& queryId)
{
    NX_ASSERT(m_queries.contains(queryId), "Query should be registered");
    if (!m_queries.contains(queryId))
        return false;

    const QueryInfo info = m_queries[queryId];
    QnCameraBookmarksQueryPtr query = info.queryRef.toStrongRef();

    if (!NX_ASSERT(query, "Interface does not allow query to be null") || !query->active())
        return false;

    switch (query->state())
    {
        case QnCameraBookmarksQuery::State::invalid:
            return true;

        case QnCameraBookmarksQuery::State::queued:
            return !info.requestTimer.isValid()
                || info.requestTimer.hasExpired(kMinimimumRequestTimeoutMs) || !query->isValid();

        case QnCameraBookmarksQuery::State::requested:
        case QnCameraBookmarksQuery::State::partial:
        case QnCameraBookmarksQuery::State::actual:
            return false;

        default:
            NX_ASSERT(false, "Should never get here");
            break;
    }
    return false;
}

void QnCameraBookmarksManagerPrivate::checkQueriesUpdate()
{
    for (const auto& queryId: m_queries.keys())
    {
        bool updateRequired = isQueryUpdateRequired(queryId);
        if (!updateRequired)
            continue;

        const QueryInfo info = m_queries[queryId];
        QnCameraBookmarksQueryPtr query = info.queryRef.toStrongRef();
        if (!NX_ASSERT(query, "Interface does not allow query to be null"))
            continue;

        sendQueryRequest(query, BookmarksCallbackType());
    }
}

void QnCameraBookmarksManagerPrivate::registerQuery(const QnCameraBookmarksQueryPtr& query)
{
    NX_ASSERT(query, "Interface does not allow query to be null");
    NX_ASSERT(!m_queries.contains(query->id()), "Query should be registered and unregistered only once");

    // Do not store shared pointer in the functor so it can be safely unregistered.
    NX_VERBOSE(this, "Register query %1, filter is %2",
        query->id(),
        query->filter());
    QUuid queryId = query->id();
    m_queries.insert(queryId, QueryInfo(query));
    connect(query.get(), &QnCameraBookmarksQuery::queryChanged, this,
        [this, queryId]
        {
            updateQueryAsync(queryId);
        });

    updateQueryAsync(queryId);
    startOperationsTimer();
}

void QnCameraBookmarksManagerPrivate::unregisterQuery(const QUuid& queryId)
{
    NX_VERBOSE(this, "Unregister query %1", queryId);
    NX_ASSERT(m_queries.contains(queryId), "Query should be registered and unregistered only once");
    m_queries.remove(queryId);
}

rest::Handle QnCameraBookmarksManagerPrivate::sendQueryRequest(
    const QnCameraBookmarksQueryPtr& query,
    BookmarksCallbackType callback)
{
    // Do not store shared pointer here so it can be safely unregistered in the process.
    QUuid queryId = query->id();
    NX_ASSERT(m_queries.contains(queryId), "Query must be registered");

    QueryInfo& info = m_queries[queryId];

    if (!query->isValid())
    {
        query->setState(QnCameraBookmarksQuery::State::actual);

        if (callback)
            callback(false, kInvalidRequestId, QnCameraBookmarkList());

        return kInvalidRequestId;
    }

    query->setState(QnCameraBookmarksQuery::State::requested);

    QnCameraBookmarkSearchFilter filter = query->filter();
    if (query->requestChunkSize() > 0)
        filter.limit = query->requestChunkSize();

    auto internalCallback =
        [this, queryId, callback]
        (bool success, int requestId, QnCameraBookmarkList bookmarks)
        {
            handleQueryReply(queryId, success, requestId, std::move(bookmarks), callback);
        };

    info.requestId = getBookmarksAsync(filter, internalCallback, this);
    info.requestTimer.restart();
    NX_VERBOSE(this, "Sending initial request [%1] with filter %2",
        info.requestId,
        filter);
    return info.requestId;
}

void QnCameraBookmarksManagerPrivate::sendQueryTailRequest(
    const QnCameraBookmarksQueryPtr& query,
    std::chrono::milliseconds timePoint,
    BookmarksCallbackType callback)
{
    query->setState(QnCameraBookmarksQuery::State::partial);
    QnCameraBookmarkSearchFilter filter = query->filter();
    filter.limit = query->requestChunkSize();
    filter.startTimeMs = timePoint;
    filter.endTimeMs = std::chrono::milliseconds(DATETIME_NOW);

    auto internalCallback =
        [this, queryId = query->id(), callback]
        (bool success, int requestId, QnCameraBookmarkList bookmarks)
        {
            handleQueryReply(queryId, success, requestId, std::move(bookmarks), callback);
        };

    QueryInfo& info = m_queries[query->id()];
    info.requestId = getBookmarksAsync(filter, internalCallback, this);
    info.requestTimer.restart();
    NX_VERBOSE(this,
        "Sending partial request [%1] with filter %2",
        info.requestId,
        filter);
}

void QnCameraBookmarksManagerPrivate::handleQueryReply(
    const QUuid& queryId,
    bool success,
    int requestId,
    QnCameraBookmarkList bookmarks,
    BookmarksCallbackType callback)
{
    const bool isReplyActual = success
        && requestId != kInvalidRequestId
        && m_queries.contains(queryId)
        && m_queries[queryId].requestId == requestId;

    if (!isReplyActual)
    {
        NX_VERBOSE(this, "Reply %1 for query %2 is not actual", requestId, queryId);
        if (callback)
            callback(success, requestId, bookmarks);

        return;
    }

    QueryInfo& info = m_queries[queryId];
    const QnCameraBookmarksQueryPtr query = info.queryRef.toStrongRef();
    NX_VERBOSE(this,
        "Processing reply [%1]: %2 bookmarks received (%3)%4",
        requestId,
        bookmarks.size(),
        debugBookmarksRange(bookmarks),
        (query ? QString{", query state is %1"}.arg(query->state()) : QString{}));
    if (!query)
        return;

    const bool isPartialData =
        query->requestChunkSize() > 0 && (int) bookmarks.size() == query->requestChunkSize();

    NX_ASSERT(query->state() == QnCameraBookmarksQuery::State::requested
        || query->state() == QnCameraBookmarksQuery::State::partial);

    // On initial request reply, reset cached data with received, otherwise merge loaded
    // data with already cached. Only default sort order is supported yet.
    if (query->state() == QnCameraBookmarksQuery::State::partial)
    {
        NX_ASSERT(query->filter().orderBy == QnBookmarkSortOrder::defaultOrder);
        bookmarks = QnCameraBookmark::mergeCameraBookmarks(
            query->systemContext(), {query->cachedBookmarks(), bookmarks});
    }
    query->setCachedBookmarks(bookmarks);
    const bool limitExceeded =
        query->filter().limit > 0 && (int) bookmarks.size() >= query->filter().limit;

    if (isPartialData && !limitExceeded)
    {
        sendQueryTailRequest(query, bookmarks.back().startTimeMs, std::move(callback));
    }
    else
    {
        query->setState(QnCameraBookmarksQuery::State::actual);
        info.requestId = kInvalidRequestId;
        NX_VERBOSE(this, "Query %1 is actual, bookmarks count %2", queryId, bookmarks.size());
        if (callback)
            callback(success, requestId, bookmarks);
    }
}

void QnCameraBookmarksManagerPrivate::updateQueryAsync(const QUuid& queryId)
{
    if (!m_queries.contains(queryId))
        return;

    QueryInfo& info = m_queries[queryId];
    info.requestId = kInvalidRequestId;
    const QnCameraBookmarksQueryPtr query = info.queryRef.toStrongRef();
    if (NX_ASSERT(query))
    {
        query->setState(QnCameraBookmarksQuery::State::queued);
        query->setCachedBookmarks({});
    }
}

QnCameraBookmarksQueryPtr QnCameraBookmarksManagerPrivate::createQuery(
    const QnCameraBookmarkSearchFilter& filter)
{
    QnCameraBookmarksQueryPtr query(new QnCameraBookmarksQuery(systemContext(), filter));
    QUuid queryId = query->id();
    registerQuery(query);

    connect(query.get(), &QObject::destroyed, this,
        [this, queryId]()
        {
            unregisterQuery(queryId);
        });

    return query;
}

void QnCameraBookmarksManagerPrivate::removeCameraFromQueries(const QnResourcePtr& resource)
{
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    nx::Uuid cameraId = camera->getId();

    for (QueryInfo& info: m_queries)
    {
        QnCameraBookmarksQueryPtr query = info.queryRef.toStrongRef();
        if (!NX_ASSERT(query, "Interface does not allow query to be null"))
            continue;

        if (!query->removeCamera(camera->getId()))
            continue;

        auto bookmarks = query->cachedBookmarks();
        for (auto it = bookmarks.begin(); it != bookmarks.end();)
        {
            if (it->cameraId == cameraId)
                it = bookmarks.erase(it);
            else
                ++it;
        }
        query->setCachedBookmarks(std::move(bookmarks));
    }
}
