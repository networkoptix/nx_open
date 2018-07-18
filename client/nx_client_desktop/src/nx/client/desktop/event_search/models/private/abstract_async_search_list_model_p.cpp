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
            const QnTimePeriod& fetchedPeriod)
        {
            if (!guard)
                return;

            const bool cancelled = fetchedPeriod.isNull();

            if (!cancelled)
                commit(fetchedPeriod);

            q->finishFetch(cancelled);
        });
}

void AbstractAsyncSearchListModel::Private::cancelPrefetch()
{
    if (m_request.id && m_prefetchCompletionHandler)
        m_prefetchCompletionHandler({});

    m_request = {};
}

bool AbstractAsyncSearchListModel::Private::canFetch() const
{
    if (!m_camera || fetchInProgress() || !hasAccessRights())
        return false;

    if (q->fetchedTimeWindow().isEmpty())
        return true;

    switch (q->fetchDirection())
    {
        case FetchDirection::earlier:
            return q->fetchedTimeWindow().startTimeMs > q->relevantTimePeriod().startTimeMs;

        case FetchDirection::later:
            return q->fetchedTimeWindow().endTimeMs() < q->relevantTimePeriod().endTimeMs();

        default:
            NX_ASSERT(false);
            return false;
    }
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
        m_request.period.startTimeMs = q->fetchedTimeWindow().endTimeMs() + 1;
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
    const QnTimePeriod& actuallyFetched, int fetchedBatchSize)
{
    const bool limitReached = fetchedBatchSize >= m_request.batchSize;

    const auto fetchedPeriod =
        [&]()
        {
            if (!limitReached)
                return m_request.period;

            auto result = m_request.period;
            if (m_request.direction == FetchDirection::earlier)
                result.truncateFront(actuallyFetched.startTimeMs + 1);
            else
                result.truncate(actuallyFetched.endTimeMs() - 1);

            return result;
        };

    NX_ASSERT(m_prefetchCompletionHandler);
    m_prefetchCompletionHandler(fetchedPeriod());
    m_prefetchCompletionHandler = {};
}

} // namespace desktop
} // namespace client
} // namespace nx
