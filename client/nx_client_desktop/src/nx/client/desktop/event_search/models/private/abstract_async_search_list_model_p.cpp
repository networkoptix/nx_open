#include "abstract_async_search_list_model_p.h"

#include <core/resource/camera_resource.h>
#include <utils/common/synctime.h>

#include <nx/utils/log/log.h>

namespace nx {
namespace client {
namespace desktop {

AbstractAsyncSearchListModel::Private::Private(AbstractAsyncSearchListModel* q):
    base_type(),
    q(q)
{
}

AbstractAsyncSearchListModel::Private::~Private()
{
}

QnVirtualCameraResourcePtr AbstractAsyncSearchListModel::Private::camera() const
{
    return m_camera;
}

void AbstractAsyncSearchListModel::Private::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (m_camera == camera)
        return;

    q->clear();
    m_camera = camera;
}

bool AbstractAsyncSearchListModel::Private::requestFetch()
{
    return prefetch(
        [this, guard = QPointer<AbstractAsyncSearchListModel>(q)](
            const QnTimePeriod& fetchedPeriod, bool cancelled)
        {
            if (!guard)
                return;

            if (!cancelled)
                commit(fetchedPeriod);

            q->finishFetch(cancelled);
        });
}

void AbstractAsyncSearchListModel::Private::cancelPrefetch()
{
    if (m_request.id && m_prefetchCompletionHandler)
        m_prefetchCompletionHandler({}, true);

    m_request = {};
}

bool AbstractAsyncSearchListModel::Private::canFetch() const
{
    if (!m_camera || fetchInProgress() || !hasAccessRights())
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
        if (!q->live()) //< In live mode there can be overlap, otherwise cannot.
            ++m_request.period.startTimeMs;

        m_request.period.setEndTimeMs(q->relevantTimePeriod().endTimeMs());
    }

    m_request.id = requestPrefetch(m_request.period);
    if (!m_request.id)
        return false;

    NX_VERBOSE(this) << "Prefetch id:" << m_request.id;

    m_prefetchCompletionHandler = completionHandler;
    return true;
}

void AbstractAsyncSearchListModel::Private::commit(const QnTimePeriod& periodToCommit)
{
    if (!fetchInProgress())
        return;

    NX_VERBOSE(this) << "Commit id:" << m_request.id;

    if (commitPrefetch(periodToCommit))
    {
        auto timeWindow = q->fetchedTimeWindow();
        timeWindow.addPeriod(periodToCommit);
        q->setFetchedTimeWindow(timeWindow);

        if (count() > q->maximumCount())
        {
            NX_VERBOSE(this) << "Truncating to maximum count";
            q->truncateToMaximumCount();
        }
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
    const QnTimePeriod& actuallyFetched, bool fetchedAll)
{
    NX_ASSERT(currentRequest().direction == q->fetchDirection());

    NX_VERBOSE(q) << "Actually fetched"
        << utils::timestampToDebugString(actuallyFetched.startTimeMs) << "to"
        << utils::timestampToDebugString(actuallyFetched.endTimeMs());

    NX_VERBOSE(q) << "Fetched all:" << fetchedAll;

    const auto fetchedPeriod =
        [&]()
        {
            auto result = m_request.period;
            if (result.isNull())
                return result;

            auto fetched = actuallyFetched;
            if (fetched.isNull())
                fetched = q->fetchedTimeWindow();

            if (m_request.direction == FetchDirection::earlier)
            {
                if (!fetchedAll)
                    result.truncateFront(fetched.startTimeMs + 1);
                if (q->live())
                    result.truncate(fetched.endTimeMs());
            }
            else
            {
                NX_ASSERT(m_request.direction == FetchDirection::later);
                if (!fetchedAll)
                    result.truncate(fetched.endTimeMs() - 1);
                else if (q->live())
                    result.truncate(fetched.endTimeMs());
            }

            NX_VERBOSE(q) << "Effective period is from"
                << utils::timestampToDebugString(result.startTimeMs) << "to"
                << utils::timestampToDebugString(result.endTimeMs());

            return result;
        };

    NX_ASSERT(m_prefetchCompletionHandler);
    m_prefetchCompletionHandler(fetchedPeriod(), false);
    m_prefetchCompletionHandler = {};
}

} // namespace desktop
} // namespace client
} // namespace nx
