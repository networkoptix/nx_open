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
        m_prefetchCompletionHandler(-1);

    m_currentFetchId = rest::Handle();
    m_prefetchCompletionHandler = PrefetchCompletionHandler();
}

bool AbstractAsyncSearchListModel::Private::canFetchMore() const
{
    // TODO: FIXME: #vkutin Check remaining time between relevantTimePeriod and fetchedTimeWindow
    return !m_fetchedAll && m_camera && !fetchInProgress() && hasAccessRights();
}

bool AbstractAsyncSearchListModel::Private::prefetch(PrefetchCompletionHandler completionHandler)
{
    if (!canFetchMore() || !completionHandler)
        return false;

    m_prefetchDirection = q->fetchDirection();
    m_lastBatchSize = q->fetchBatchSize();

    m_currentFetchId = m_prefetchDirection == FetchDirection::earlier
        ? requestPrefetch(q->relevantTimePeriod().startTimeMs, fetchedTimeWindow().startTimeMs - 1)
        : requestPrefetch(fetchedTimeWindow().endTimeMs() + 1, q->relevantTimePeriod().endTimeMs());

    if (!m_currentFetchId)
        return false;

    qDebug() << "Prefetch id:" << m_currentFetchId;

    m_prefetchCompletionHandler = completionHandler;
    return true;
}

void AbstractAsyncSearchListModel::Private::commit(qint64 syncTimeToCommitMs)
{
    if (!m_currentFetchId)
        return;

    qDebug() << "Commit id:" << m_currentFetchId;
    m_currentFetchId = rest::Handle();

    if (!commitPrefetch(syncTimeToCommitMs, m_fetchedAll))
        return;

    if (m_prefetchDirection == FetchDirection::earlier)
    {
        m_fetchedTimeWindow.startTimeMs = q->relevantTimePeriod().bound(syncTimeToCommitMs);
    }
    else
    {
        // TODO: FIXME: #vkutin Implement me!
        NX_ASSERT(false);
    }
}

bool AbstractAsyncSearchListModel::Private::fetchInProgress() const
{
    return m_currentFetchId != rest::Handle();
}

bool AbstractAsyncSearchListModel::Private::shouldSkipResponse(rest::Handle requestId) const
{
    return !m_currentFetchId || !m_prefetchCompletionHandler || m_currentFetchId != requestId;
}

void AbstractAsyncSearchListModel::Private::complete(qint64 earliestTimeMs)
{
    NX_ASSERT(m_prefetchCompletionHandler);
    m_prefetchCompletionHandler(earliestTimeMs);
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
    return QnTimePeriod(QnTimePeriod::kMaxTimeValue, QnTimePeriod::infiniteDuration());
}

} // namespace desktop
} // namespace client
} // namespace nx
