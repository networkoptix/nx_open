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

void AbstractAsyncSearchListModel::Private::clear()
{
    m_fetchedAll = false;
    m_earliestTimeMs = qnSyncTime->currentMSecsSinceEpoch();
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

    m_currentFetchId = requestPrefetch(m_earliestTimeMs - 1);
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

qint64 AbstractAsyncSearchListModel::Private::earliestTimeMs() const
{
    return m_earliestTimeMs; // TODO: #vkutin Think how to get rid of this function.
}

} // namespace
} // namespace client
} // namespace nx
