#include "abstract_async_search_list_model_p.h"

#include <core/resource/camera_resource.h>
#include <utils/common/synctime.h>

#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>

namespace nx::client::desktop {

AbstractAsyncSearchListModel::Private::Private(AbstractAsyncSearchListModel* q):
    base_type(),
    q(q)
{
}

AbstractAsyncSearchListModel::Private::~Private()
{
}

bool AbstractAsyncSearchListModel::Private::requestFetch()
{
    return prefetch(nx::utils::guarded(this,
        [this](const QnTimePeriod& fetchedPeriod, FetchResult result)
        {
            q->addToFetchedTimeWindow(fetchedPeriod);

            if (result == FetchResult::complete || result == FetchResult::incomplete)
                commit(fetchedPeriod);

            q->finishFetch(result);
        }));
}

void AbstractAsyncSearchListModel::Private::cancelPrefetch()
{
    if (m_request.id && m_prefetchCompletionHandler)
        m_prefetchCompletionHandler({}, FetchResult::cancelled);

    m_request = {};
}

bool AbstractAsyncSearchListModel::Private::canFetch() const
{
    if (fetchInProgress() || !hasAccessRights())
        return false;

    if (q->fetchedTimeWindow().isEmpty())
        return true;

    if (q->fetchDirection() == FetchDirection::earlier)
        return q->fetchedTimeWindow().startTimeMs > q->relevantTimePeriod().startTimeMs;

    NX_ASSERT(q->fetchDirection() == FetchDirection::later);
    return q->fetchedTimeWindow().endTimeMs() < q->relevantTimePeriod().endTimeMs();
}

bool AbstractAsyncSearchListModel::Private::prefetch(PrefetchCompletionHandler completionHandler)
{
    if (!canFetch() || !completionHandler)
        return false;

    m_request.direction = q->fetchDirection();
    m_request.batchSize = q->fetchBatchSize();

    if (q->fetchedTimeWindow().isEmpty())
    {
        m_request.period = q->relevantTimePeriod();
    }
    else if (m_request.direction == FetchDirection::earlier)
    {
        m_request.period.startTimeMs = q->relevantTimePeriod().startTimeMs;
        m_request.period.setEndTimeMs(q->fetchedTimeWindow().startTimeMs - 1);
    }
    else
    {
        NX_ASSERT(m_request.direction == FetchDirection::later);
        m_request.period.startTimeMs = q->fetchedTimeWindow().endTimeMs();
        if (!q->effectiveLiveSupported()) //< In live mode there can be overlap, otherwise cannot.
            ++m_request.period.startTimeMs;

        m_request.period.setEndTimeMs(q->relevantTimePeriod().endTimeMs());
    }

    m_request.id = requestPrefetch(m_request.period);
    if (!m_request.id)
        return false;

    NX_VERBOSE(q) << "Prefetch id:" << m_request.id;

    m_prefetchCompletionHandler = completionHandler;
    return true;
}

void AbstractAsyncSearchListModel::Private::commit(const QnTimePeriod& periodToCommit)
{
    if (!fetchInProgress())
        return;

    NX_VERBOSE(q) << "Commit id:" << m_request.id;

    commitPrefetch(periodToCommit);

    if (count() > q->maximumCount())
    {
        NX_VERBOSE(q) << "Truncating to maximum count";
        q->truncateToMaximumCount();
    }

    m_request = {};
    m_prefetchCompletionHandler = {};
}

bool AbstractAsyncSearchListModel::Private::fetchInProgress() const
{
    return m_request.id != rest::Handle();
}

const AbstractAsyncSearchListModel::Private::FetchInformation&
    AbstractAsyncSearchListModel::Private::currentRequest() const
{
    return m_request;
}

void AbstractAsyncSearchListModel::Private::completePrefetch(
    const QnTimePeriod& actuallyFetched, bool success, int fetchedCount)
{
    NX_ASSERT(m_request.direction == q->fetchDirection());

    if (fetchedCount == 0)
    {
        NX_VERBOSE(q) << "Pre-fetched no items";
    }
    else
    {
        NX_VERBOSE(q) << "Pre-fetched" << fetchedCount << "items from"
            << utils::timestampToDebugString(actuallyFetched.startTimeMs) << "to"
            << utils::timestampToDebugString(actuallyFetched.endTimeMs());
    }

    const bool fetchedAll = success && fetchedCount < m_request.batchSize;
    const bool mayGoLive = fetchedAll && m_request.direction == FetchDirection::later;

    const auto fetchedPeriod =
        [this, &actuallyFetched, success, fetchedAll]() -> QnTimePeriod
        {
            if (!success)
                return {};

            auto result = m_request.period;
            if (result.isNull())
                return result;

            auto fetched = result.intersected(actuallyFetched);
            if (fetched.isNull())
                fetched = q->fetchedTimeWindow();

            if (m_request.direction == FetchDirection::earlier)
            {
                if (!fetchedAll)
                    result.truncateFront(fetched.startTimeMs + 1);
                if (q->effectiveLiveSupported())
                    result.truncate(fetched.endTimeMs());

                if (result.isNull() && fetchedAll)
                    result.durationMs = 1; //< To avoid getting null time window when all is fetched.
            }
            else
            {
                NX_ASSERT(m_request.direction == FetchDirection::later);
                if (!fetchedAll)
                    result.truncate(fetched.endTimeMs() - 1);
                else if (q->effectiveLiveSupported())
                    result.truncate(fetched.endTimeMs());
            }

            NX_VERBOSE(q) << "Effective period is from"
                << utils::timestampToDebugString(result.startTimeMs) << "to"
                << utils::timestampToDebugString(result.endTimeMs());

            return result;
        };

    const auto result = success
        ? (fetchedAll ? FetchResult::complete : FetchResult::incomplete)
        : FetchResult::failed;

    NX_VERBOSE(q) << "Fetch result:" << QVariant::fromValue(result).toString();

    NX_ASSERT(m_prefetchCompletionHandler);
    m_prefetchCompletionHandler(fetchedPeriod(), result);
    m_prefetchCompletionHandler = {};

    // If top is reached, go to live mode.
    if (mayGoLive)
        q->setLive(q->effectiveLiveSupported());
}

} // namespace nx::client::desktop
