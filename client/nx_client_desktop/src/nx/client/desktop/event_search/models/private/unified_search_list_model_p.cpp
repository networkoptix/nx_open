#include "unified_search_list_model_p.h"

#include <utils/common/delayed.h>

#include <nx/client/desktop/common/models/concatenation_list_model.h>
#include <nx/client/desktop/event_search/models/analytics_search_list_model.h>
#include <nx/client/desktop/event_search/models/bookmark_search_list_model.h>
#include <nx/client/desktop/event_search/models/event_search_list_model.h>
#include <nx/client/desktop/event_search/models/private/busy_indicator_model_p.h>
#include <nx/utils/log/assert.h>

namespace nx {
namespace client {
namespace desktop {

UnifiedSearchListModel::Private::Private(UnifiedSearchListModel* q):
    base_type(),
    q(q),
    m_eventsModel(new EventSearchListModel(q)),
    m_bookmarksModel(new BookmarkSearchListModel(q)),
    m_analyticsModel(new AnalyticsSearchListModel(q)),
    m_busyIndicatorModel(new BusyIndicatorModel(q))
{
    q->setModels({m_eventsModel, m_bookmarksModel, m_analyticsModel, m_busyIndicatorModel});
}

UnifiedSearchListModel::Private::~Private()
{
}

bool UnifiedSearchListModel::Private::canFetchMore() const
{
    return m_camera && !m_currentFetchGuard
        && (m_eventsModel->canFetchMore() || m_bookmarksModel->canFetchMore()
            || m_analyticsModel->canFetchMore());
}

void UnifiedSearchListModel::Private::fetchMore()
{
    if (!canFetchMore())
        return;

    NX_ASSERT(!fetchInProgress());

    // Will be called after all prefetches are complete.
    auto totalCompletionHandler =
        [this, guard = QPointer<Private>(this)]()
        {
            if (!guard)
                return;

            m_busyIndicatorModel->setActive(false);

            if (m_queuedFetchMore)
            {
                executeDelayedParented([this]() { ensureFetchMore(); }, 0, this);
                m_queuedFetchMore = false;
            }

            if (!fetchInProgress()) //< If previous fetch was completely cancelled.
                return;

            const auto oldRowCount = q->rowCount();

            QnRaiiGuard fetchCommittedSignalGuard(
                [this, oldRowCount]()
                {
                    emit q->fetchCommitted(q->rowCount() - oldRowCount,
                        UnifiedSearchListModel::QPrivateSignal());
                });

            emit q->fetchAboutToBeCommitted(UnifiedSearchListModel::QPrivateSignal());

            if (m_eventsModel->fetchInProgress())
                m_eventsModel->commitPrefetch(m_latestStartTimeMs);

            if (m_bookmarksModel->fetchInProgress())
                m_bookmarksModel->commitPrefetch(m_latestStartTimeMs);

            if (m_analyticsModel->fetchInProgress())
                m_analyticsModel->commitPrefetch(m_latestStartTimeMs);
        };

    auto completionGuard = QnRaiiGuard::createDestructible(totalCompletionHandler);

    // Will be called after a single prefetch is complete.
    const auto prefetchCompletionHandler =
        [this, completionGuard](qint64 earliestTimeMs)
        {
            m_latestStartTimeMs = qMax(m_latestStartTimeMs, earliestTimeMs);
        };

    m_latestStartTimeMs = 0;
    m_currentFetchGuard = completionGuard;

    m_eventsModel->prefetchAsync(prefetchCompletionHandler);
    m_bookmarksModel->prefetchAsync(prefetchCompletionHandler);
    m_analyticsModel->prefetchAsync(prefetchCompletionHandler);

    if (fetchInProgress())
        m_busyIndicatorModel->setActive(true);
}

bool UnifiedSearchListModel::Private::fetchInProgress() const
{
    return m_eventsModel->fetchInProgress()
        || m_bookmarksModel->fetchInProgress()
        || m_analyticsModel->fetchInProgress();
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

    const bool somethingAdded = (filter & ~m_filter) != 0;
    if (somethingAdded) //< We should not add new types to existing search.
        clear();

    m_filter = filter;
    updateModels();
}

vms::event::EventType UnifiedSearchListModel::Private::selectedEventType() const
{
    return m_selectedEventType;
}

void UnifiedSearchListModel::Private::setSelectedEventType(vms::event::EventType value)
{
    if (selectedEventType() == value)
        return;

    m_selectedEventType = value;

    if (m_filter != Types(Type::events))
        return;

    updateModels();
}

void UnifiedSearchListModel::Private::setAnalyticsSearchRect(const QRectF& relativeRect)
{
    m_analyticsModel->setFilterRect(relativeRect);

    if (m_filter.testFlag(Type::analytics))
        updateModels();
}

void UnifiedSearchListModel::Private::clear()
{
    m_eventsModel->setCamera(QnVirtualCameraResourcePtr());
    m_bookmarksModel->setCamera(QnVirtualCameraResourcePtr());
    m_analyticsModel->setCamera(QnVirtualCameraResourcePtr());
}

void UnifiedSearchListModel::Private::updateModels()
{
    m_eventsModel->setCamera(m_filter.testFlag(Type::events)
        ? m_camera
        : QnVirtualCameraResourcePtr());

    m_eventsModel->setSelectedEventType(m_filter == Types(Type::events)
        ? m_selectedEventType
        : vms::event::undefinedEvent);

    m_bookmarksModel->setCamera(m_filter.testFlag(Type::bookmarks)
        ? m_camera
        : QnVirtualCameraResourcePtr());

    m_analyticsModel->setCamera(m_filter.testFlag(Type::analytics)
        ? m_camera
        : QnVirtualCameraResourcePtr());

    ensureFetchMore();
}

void UnifiedSearchListModel::Private::ensureFetchMore()
{
    if (m_currentFetchGuard)
    {
        m_queuedFetchMore = true;
    }
    else
    {
        NX_ASSERT(!m_queuedFetchMore);
        m_queuedFetchMore = false;
        fetchMore();
    }
}

} // namespace
} // namespace client
} // namespace nx
