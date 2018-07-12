#include "abstract_async_search_list_model_p.h"

#include <core/resource/camera_resource.h>
#include <utils/common/synctime.h>

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

    qDebug() << "Relevant time period changed";
    qDebug() << "--- Old was from"
        << utils::timestampToRfc2822(previousValue.startTimeMs) << "to"
        << utils::timestampToRfc2822(previousValue.endTimeMs());
    qDebug() << "--- New is from"
        << utils::timestampToRfc2822(currentValue.startTimeMs) << "to"
        << utils::timestampToRfc2822(currentValue.endTimeMs());

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
        m_fetchedTimeWindow = m_fetchedTimeWindow.intersected(currentValue);
        clipToSelectedTimePeriod();
        cancelPrefetch();
    }
}

void AbstractAsyncSearchListModel::Private::fetchDirectionChanged()
{
    qDebug() << "New fetch direction:" << q->fetchDirection();
    cancelPrefetch();
    m_fetchedAll = false;
    // TODO: FIXME: #vkutin Implement me!
}

void AbstractAsyncSearchListModel::Private::clear()
{
    m_fetchedAll = false;

    m_fetchedTimeWindow = q->relevantTimePeriod().isInfinite()
        ? infiniteFuture()
        : QnTimePeriod(q->relevantTimePeriod().endTimeMs(), 0);

    cancelPrefetch();
    q->setFetchDirection(FetchDirection::earlier);
}

void AbstractAsyncSearchListModel::Private::cancelPrefetch()
{
    if (m_currentFetchId && m_prefetchCompletionHandler)
        m_prefetchCompletionHandler(QnTimePeriod(-1, 0));

    m_currentFetchId = rest::Handle();
    m_prefetchCompletionHandler = PrefetchCompletionHandler();
}

bool AbstractAsyncSearchListModel::Private::canFetchMore() const
{
    if (m_fetchedAll || !m_camera || fetchInProgress() || !hasAccessRights())
        return false;

    switch (q->fetchDirection())
    {
        case FetchDirection::none:
            return false;

        case FetchDirection::earlier:

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

    if (m_prefetchDirection == FetchDirection::earlier)
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

    qDebug() << "Prefetch id:" << m_currentFetchId;

    m_prefetchCompletionHandler = completionHandler;
    return true;
}

void AbstractAsyncSearchListModel::Private::commit(const QnTimePeriod& periodToCommit)
{
    if (!m_currentFetchId)
        return;

    qDebug() << "Commit id:" << m_currentFetchId;
    m_currentFetchId = rest::Handle();

    if (!commitPrefetch(periodToCommit, m_fetchedAll))
        return;

    if (m_fetchedTimeWindow.isInfinite()) // TODO: FIXME: #vkutin Is this right?
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
    auto fetchedPeriod = [&]()
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

QnTimePeriod AbstractAsyncSearchListModel::Private::infiniteFuture()
{
    return QnTimePeriod(QnTimePeriod::maxTimeValue(), QnTimePeriod::infiniteDuration());
}

} // namespace desktop
} // namespace client
} // namespace nx
