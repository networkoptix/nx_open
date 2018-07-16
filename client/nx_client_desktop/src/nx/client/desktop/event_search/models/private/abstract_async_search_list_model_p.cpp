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

    clear();
    m_camera = camera;
}

void AbstractAsyncSearchListModel::Private::relevantTimePeriodChanged(
    const QnTimePeriod& previousValue)
{
    const auto currentValue = q->relevantTimePeriod();
    NX_ASSERT(currentValue != previousValue);

    if (!currentValue.isValid())
    {
        clear();
        m_fetchedAll = true;
        return;
    }

    if (currentValue.endTimeMs() > previousValue.endTimeMs()
        || !currentValue.intersects(previousValue))
    {
        clear();
    }
    else
    {
        m_fetchedAll = m_fetchedAll && m_fetchedTimeWindow.contains(currentValue);
        m_fetchedTimeWindow = m_fetchedTimeWindow.isEmpty()
            ? m_fetchedTimeWindow.intersected(currentValue)
            : currentValue;

        clipToSelectedTimePeriod();
        cancelPrefetch();
    }
}

void AbstractAsyncSearchListModel::Private::fetchDirectionChanged()
{
    cancelPrefetch();
    m_fetchedAll = false;
    // TODO: FIXME: #vkutin Implement me!
}

void AbstractAsyncSearchListModel::Private::clear()
{
    NX_VERBOSE(this) << "Clear model";

    m_fetchedAll = false;
    m_fetchedTimeWindow = {};

    cancelPrefetch();
    q->setFetchDirection(FetchDirection::earlier);
}

bool AbstractAsyncSearchListModel::Private::requestFetch()
{
    return prefetch(
        [this, guard = QPointer<AbstractAsyncSearchListModel>(q)](
            const QnTimePeriod& fetchedPeriod)
        {
            if (!guard)
                return;

            const bool cancelled = fetchedPeriod.startTimeMs < 0;

            QnRaiiGuard finishFetch(
                [this]() { q->beginFinishFetch(); },
                [this, cancelled]() { q->endFinishFetch(cancelled); });

            if (!cancelled)
                commit(fetchedPeriod);
        });
}

void AbstractAsyncSearchListModel::Private::cancelPrefetch()
{
    if (m_currentFetchId && m_prefetchCompletionHandler)
        m_prefetchCompletionHandler(QnTimePeriod(-1, 0));

    m_currentFetchId = {};
    m_prefetchCompletionHandler = {};
}

bool AbstractAsyncSearchListModel::Private::canFetchMore() const
{
    if (m_fetchedAll || !m_camera || fetchInProgress() || !hasAccessRights())
        return false;

    if (m_fetchedTimeWindow.isEmpty())
        return true;

    switch (q->fetchDirection())
    {
        case FetchDirection::earlier:
            return m_fetchedTimeWindow.startTimeMs > q->relevantTimePeriod().startTimeMs;

        case FetchDirection::later:
            return m_fetchedTimeWindow.endTimeMs() < q->relevantTimePeriod().endTimeMs();

        default:
            NX_ASSERT(false);
            return false;
    }
}

bool AbstractAsyncSearchListModel::Private::prefetch(PrefetchCompletionHandler completionHandler)
{
    if (!canFetchMore() || !completionHandler)
        return false;

    m_prefetchDirection = q->fetchDirection();
    m_lastBatchSize = q->fetchBatchSize();

    if (fetchedTimeWindow().isEmpty())
    {
        m_requestedFetchPeriod = QnTimePeriod(0, QnTimePeriod::infiniteDuration());
    }
    else if (m_prefetchDirection == FetchDirection::earlier)
    {
        m_requestedFetchPeriod.startTimeMs = q->relevantTimePeriod().startTimeMs;
        m_requestedFetchPeriod.setEndTimeMs(fetchedTimeWindow().startTimeMs - 1);
    }
    else
    {
        m_requestedFetchPeriod.startTimeMs = fetchedTimeWindow().endTimeMs() + 1;
        m_requestedFetchPeriod.setEndTimeMs(q->relevantTimePeriod().endTimeMs());
    }

    m_currentFetchId = requestPrefetch(m_requestedFetchPeriod);
    if (!m_currentFetchId)
        return false;

    NX_VERBOSE(this) << "Prefetch id:" << m_currentFetchId;

    m_prefetchCompletionHandler = completionHandler;
    return true;
}

void AbstractAsyncSearchListModel::Private::commit(const QnTimePeriod& periodToCommit)
{
    if (!m_currentFetchId)
        return;

    NX_VERBOSE(this) << "Commit id:" << m_currentFetchId;
    m_currentFetchId = rest::Handle();

    if (!commitPrefetch(periodToCommit, m_fetchedAll))
        return;

    if (m_fetchedTimeWindow.isEmpty())
        m_fetchedTimeWindow = periodToCommit;
    else
        m_fetchedTimeWindow.addPeriod(periodToCommit);
}

bool AbstractAsyncSearchListModel::Private::fetchInProgress() const
{
    return m_currentFetchId != rest::Handle();
}

bool AbstractAsyncSearchListModel::Private::shouldSkipResponse(rest::Handle requestId) const
{
    return !m_currentFetchId || !m_prefetchCompletionHandler || m_currentFetchId != requestId;
}

void AbstractAsyncSearchListModel::Private::completePrefetch(
    const QnTimePeriod& actuallyFetched, bool limitReached)
{
    auto fetchedPeriod =
        [&]()
        {
            if (!limitReached)
                return m_requestedFetchPeriod;

            auto result = m_requestedFetchPeriod;
            if (m_prefetchDirection == FetchDirection::earlier)
                result.startTimeMs = actuallyFetched.startTimeMs + 1;
            else
                result.setEndTimeMs(actuallyFetched.endTimeMs() - 1);

            return result;
        };

    NX_ASSERT(m_prefetchCompletionHandler);
    m_prefetchCompletionHandler(fetchedPeriod());
    m_prefetchCompletionHandler = PrefetchCompletionHandler();
}

QnTimePeriod AbstractAsyncSearchListModel::Private::fetchedTimeWindow() const
{
    return m_fetchedTimeWindow;
}

int AbstractAsyncSearchListModel::Private::lastBatchSize() const
{
    return m_lastBatchSize;
}

} // namespace desktop
} // namespace client
} // namespace nx
