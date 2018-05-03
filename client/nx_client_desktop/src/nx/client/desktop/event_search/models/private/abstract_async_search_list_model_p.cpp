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

    if (currentValue.endTimeMs() > previousValue.endTimeMs())
    {
        clear();
    }
    else
    {
        m_fetchedAll = m_fetchedAll && (currentValue.startTimeMs >= previousValue.startTimeMs);
        m_earliestTimeMs = currentValue.bound(m_earliestTimeMs);
        clipToSelectedTimePeriod();
        cancelPrefetch();
    }
}

void AbstractAsyncSearchListModel::Private::clear()
{
    m_fetchedAll = false;
    m_earliestTimeMs = q->relevantTimePeriod().bound(qnSyncTime->currentMSecsSinceEpoch());
    cancelPrefetch();
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
    return !m_fetchedAll && m_camera && !fetchInProgress() && hasAccessRights()
        && q->relevantTimePeriod().startTimeMs < m_earliestTimeMs;
}

bool AbstractAsyncSearchListModel::Private::prefetch(PrefetchCompletionHandler completionHandler)
{
    if (!canFetchMore() || !completionHandler)
        return false;

    m_currentFetchId = requestPrefetch(q->relevantTimePeriod().startTimeMs, m_earliestTimeMs - 1);
    if (!m_currentFetchId)
        return false;

    qDebug() << "Prefetch id:" << m_currentFetchId;

    m_prefetchCompletionHandler = completionHandler;
    return true;
}

void AbstractAsyncSearchListModel::Private::commit(qint64 earliestTimeToCommitMs)
{
    if (!m_currentFetchId)
        return;

    qDebug() << "Commit id:" << m_currentFetchId;
    m_currentFetchId = rest::Handle();

    earliestTimeToCommitMs = q->relevantTimePeriod().bound(earliestTimeToCommitMs);

    if (!commitPrefetch(earliestTimeToCommitMs, m_fetchedAll))
        return;

    m_earliestTimeMs = earliestTimeToCommitMs;
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

QnTimePeriod AbstractAsyncSearchListModel::Private::fetchedTimePeriod() const
{
    return q->relevantTimePeriod().isInfinite()
        ? QnTimePeriod(m_earliestTimeMs, QnTimePeriod::infiniteDuration())
        : QnTimePeriod::fromInterval(m_earliestTimeMs, q->relevantTimePeriod().endTimeMs());
}

} // namespace desktop
} // namespace client
} // namespace nx
