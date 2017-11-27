#include "unified_search_list_model_p.h"

#include <nx/client/desktop/common/models/concatenation_list_model.h>
#include <nx/client/desktop/event_search/models/analytics_search_list_model.h>
#include <nx/client/desktop/event_search/models/bookmark_search_list_model.h>
#include <nx/client/desktop/event_search/models/event_search_list_model.h>
#include <nx/utils/log/assert.h>

namespace nx {
namespace client {
namespace desktop {

UnifiedSearchListModel::Private::Private(UnifiedSearchListModel* q):
    base_type(),
    q(q),
    m_eventsModel(new EventSearchListModel(q)),
    m_bookmarksModel(new BookmarkSearchListModel(q)),
    m_analyticsModel(new AnalyticsSearchListModel(q))
{
    q->setModels({m_eventsModel, m_bookmarksModel, m_analyticsModel});
}

UnifiedSearchListModel::Private::~Private()
{
}

bool UnifiedSearchListModel::Private::canFetchMore() const
{
    return m_camera && !m_fetchInProgress
        && (m_eventsModel->canFetchMore() || m_bookmarksModel->canFetchMore()
            || m_analyticsModel->canFetchMore());
}

void UnifiedSearchListModel::Private::fetchMore()
{
    if (!canFetchMore())
        return;

    NX_ASSERT(m_fetchingTypes == Types());

    // Will be called after all prefetches are complete.
    auto totalCompletionHandler =
        [this, guard = QPointer<Private>(this)]()
        {
            if (!guard)
                return;

            if (m_fetchingTypes.testFlag(Type::events))
                m_eventsModel->commitPrefetch(m_latestStartTimeMs);

            if (m_fetchingTypes.testFlag(Type::bookmarks))
                m_bookmarksModel->commitPrefetch(m_latestStartTimeMs);

            if (m_fetchingTypes.testFlag(Type::analytics))
                m_analyticsModel->commitPrefetch(m_latestStartTimeMs);

            m_fetchingTypes = Types();
        };

    auto completionGuard = QnRaiiGuard::createDestructible(totalCompletionHandler);

    // Will be called after a single prefetch is complete.
    const auto prefetchCompletionHandler =
        [this, completionGuard](qint64 earliestTimeMs)
        {
            m_latestStartTimeMs = qMax(m_latestStartTimeMs, earliestTimeMs);
        };

    m_latestStartTimeMs = 0;
    m_fetchInProgress = completionGuard;
    m_fetchingTypes = Types();

    if (m_eventsModel->prefetchAsync(prefetchCompletionHandler))
        m_fetchingTypes |= Type::events;

    if (m_bookmarksModel->prefetchAsync(prefetchCompletionHandler))
        m_fetchingTypes |= Type::bookmarks;

    if (m_analyticsModel->prefetchAsync(prefetchCompletionHandler))
        m_fetchingTypes |= Type::analytics;
}

QnVirtualCameraResourcePtr UnifiedSearchListModel::Private::camera() const
{
    return m_camera;
}

void UnifiedSearchListModel::Private::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (m_camera == camera)
        return;

    m_camera = camera;
    updateModels();
}

UnifiedSearchListModel::Types UnifiedSearchListModel::Private::filter() const
{
    return m_filter;
}

void UnifiedSearchListModel::Private::setFilter(Types filter)
{
    if (filter == m_filter)
        return;

    m_filter = filter;
    updateModels();
}

void UnifiedSearchListModel::Private::updateModels()
{
    m_eventsModel->setCamera(m_filter.testFlag(Type::events)
        ? m_camera
        : QnVirtualCameraResourcePtr());

    m_bookmarksModel->setCamera(m_filter.testFlag(Type::bookmarks)
        ? m_camera
        : QnVirtualCameraResourcePtr());

    m_analyticsModel->setCamera(m_filter.testFlag(Type::analytics)
        ? m_camera
        : QnVirtualCameraResourcePtr());

    fetchMore();
}

} // namespace
} // namespace client
} // namespace nx
