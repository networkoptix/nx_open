#include "abstract_async_search_list_model_p.h"

#include <core/resource/camera_resource.h>
#include <utils/common/synctime.h>

namespace nx {
namespace client {
namespace desktop {

AbstractAsyncSearchListModel::Private::Private(AbstractAsyncSearchListModel* q):
    base_type(),
    q(q),
    m_selectedTimePeriod(QnTimePeriod::kMinTimeValue, QnTimePeriod::infiniteDuration())
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

QnTimePeriod AbstractAsyncSearchListModel::Private::selectedTimePeriod() const
{
    return m_selectedTimePeriod;
}

void AbstractAsyncSearchListModel::Private::setSelectedTimePeriod(const QnTimePeriod& newTimePeriod)
{
    if (m_selectedTimePeriod == newTimePeriod)
        return;

    const auto oldTimePeriod = m_selectedTimePeriod;
    m_selectedTimePeriod = newTimePeriod;

    if (newTimePeriod.endTimeMs() > oldTimePeriod.endTimeMs())
    {
        clear();
    }
    else
    {
        m_fetchedAll = m_fetchedAll && (newTimePeriod.startTimeMs >= oldTimePeriod.startTimeMs);
        m_earliestTimeMs = qBound(newTimePeriod.startTimeMs, m_earliestTimeMs, newTimePeriod.endTimeMs());
        clipToSelectedTimePeriod();
        cancelPrefetch();
    }
}

void AbstractAsyncSearchListModel::Private::clear()
{
    m_fetchedAll = false;
    m_earliestTimeMs = qBound(m_selectedTimePeriod.startTimeMs,
        qnSyncTime->currentMSecsSinceEpoch(), m_selectedTimePeriod.endTimeMs());
    cancelPrefetch();
}

void AbstractAsyncSearchListModel::Private::cancelPrefetch()
{
    m_currentFetchId = rest::Handle();
    m_prefetchCompletionHandler = PrefetchCompletionHandler();
}

bool AbstractAsyncSearchListModel::Private::canFetchMore() const
{
    return !m_fetchedAll && m_camera && !fetchInProgress() && hasAccessRights();
}

bool AbstractAsyncSearchListModel::Private::prefetch(PrefetchCompletionHandler completionHandler)
{
    if (!canFetchMore() || !completionHandler)
        return false;

    m_currentFetchId = requestPrefetch(m_selectedTimePeriod.startTimeMs, m_earliestTimeMs - 1);
    if (!m_currentFetchId)
        return false;

    m_prefetchCompletionHandler = completionHandler;
    return true;
}

void AbstractAsyncSearchListModel::Private::commit(qint64 earliestTimeToCommitMs)
{
    if (!m_currentFetchId)
        return;

    m_currentFetchId = rest::Handle();

    if (!commitPrefetch(earliestTimeToCommitMs, m_fetchedAll))
        return;

    m_earliestTimeMs = qBound(m_selectedTimePeriod.startTimeMs, earliestTimeToCommitMs,
        m_selectedTimePeriod.endTimeMs());
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
    return m_selectedTimePeriod.isInfinite()
        ? QnTimePeriod(m_earliestTimeMs, QnTimePeriod::infiniteDuration())
        : QnTimePeriod::fromInterval(m_earliestTimeMs, m_selectedTimePeriod.endTimeMs());
}

} // namespace desktop
} // namespace client
} // namespace nx
