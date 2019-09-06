#include "timeline_bookmarks_watcher.h"

#include <chrono>

#include <QtCore/QTimer>

#include <utils/common/delayed.h>
#include <utils/camera/bookmark_helpers.h>
#include <core/resource/camera_resource.h>
#include <camera/camera_bookmarks_manager.h>
#include <camera/camera_bookmark_aggregation.h>
#include <ui/utils/bookmark_merge_helper.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/controls/time_slider.h>

#include <ui/workbench/workbench_display.h>

#include <camera/bookmark_queries_cache.h>
#include <camera/camera_bookmarks_query.h>
#include <core/resource_management/resource_pool.h>

#include <utils/common/scoped_timer.h>

#include <nx/utils/datetime.h>
#include <nx/utils/pending_operation.h>

using namespace std::chrono;

namespace {

// This number of bookmarks is requested once per camera on its first opening.
static constexpr int kMaxStaticBookmarksCount = 5000;

// This number of bookmarks is requested for each timeline window change.
static constexpr int kMaxTimelineWindowBookmarksCount = 500;

// Timeline window is considered changed if begin or end is shifted by the given time.
static constexpr auto kTimelineMinWindowChange = 20s;

// Timeline window query is updated not more often than once in 200ms.
static constexpr int kUpdateTimelineWindowQueryPeriodMs = 200;

// Once in 5 minutes refresh all loaded bookmarks.
static constexpr auto kUpdateStaticQueriesInterval = 5min;

} // namespace

QnTimelineBookmarksWatcher::QnTimelineBookmarksWatcher(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_aggregation(new QnCameraBookmarkAggregation()),
    m_mergeHelper(new QnBookmarkMergeHelper()),
    m_queriesCache(new QnBookmarkQueriesCache()),
    m_updateStaticQueriesTimer(new QTimer())
{
    // Create query, which will request bookmarks for the current timeline window. So we make sure
    // user will see bookmarks even if there are more than 5000 of them.
    setupTimelineWindowQuery();

    connect(resourcePool(), &QnResourcePool::resourceRemoved,
        this, &QnTimelineBookmarksWatcher::onResourceRemoved);

    connect(navigator(), &QnWorkbenchNavigator::currentWidgetChanged, this,
        &QnTimelineBookmarksWatcher::updateCurrentCamera);
    connect(navigator(), &QnWorkbenchNavigator::bookmarksModeEnabledChanged, this,
        &QnTimelineBookmarksWatcher::updateCurrentCamera);

    connect(display(), &QnWorkbenchDisplay::widgetAdded, this,
        [this](QnResourceWidget* widget)
        {
            const auto& resource = widget->resource();
            if (const auto& camera = resource.dynamicCast<QnVirtualCameraResource>())
                ensureStaticQueryForCamera(camera);
        });

    connect(qnCameraBookmarksManager, &QnCameraBookmarksManager::bookmarkRemoved, this,
        &QnTimelineBookmarksWatcher::onBookmarkRemoved);

    navigator()->timeSlider()->setBookmarksHelper(m_mergeHelper);

    m_updateStaticQueriesTimer->setInterval(kUpdateStaticQueriesInterval);
    connect(m_updateStaticQueriesTimer, &QTimer::timeout, m_queriesCache,
        &QnBookmarkQueriesCache::refreshQueries);
    m_updateStaticQueriesTimer->start();
}

QnTimelineBookmarksWatcher::~QnTimelineBookmarksWatcher()
{
}

QnCameraBookmarkList QnTimelineBookmarksWatcher::bookmarksAtPosition(milliseconds position,
    milliseconds  msecdsPerDp)
{
    return m_mergeHelper->bookmarksAtPosition(position, msecdsPerDp);
}

QnCameraBookmarkList QnTimelineBookmarksWatcher::rawBookmarksAtPosition(
    const QnVirtualCameraResourcePtr& camera,
    qint64 positionMs)
{
    if (!m_queriesCache->hasQuery(camera))
        return QnCameraBookmarkList();

    if (camera == m_currentCamera)
        return helpers::bookmarksAtPosition(m_aggregation->bookmarkList(), positionMs);

    const auto& query = m_queriesCache->getOrCreateQuery(camera);
    const auto& bookmarks = query->cachedBookmarks();

    return helpers::bookmarksAtPosition(bookmarks, positionMs);
}

void QnTimelineBookmarksWatcher::onResourceRemoved(const QnResourcePtr& resource)
{
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    if (!m_queriesCache->hasQuery(camera))
        return;

    const auto query = m_queriesCache->getOrCreateQuery(camera);
    disconnect(query, &QnCameraBookmarksQuery::bookmarksChanged, this, nullptr);
    m_queriesCache->removeQueryByCamera(camera);
}

void QnTimelineBookmarksWatcher::setupTimelineWindowQuery()
{
    auto updateTimelineWindowQueryIfNeeded =
        [this]
        {
            if (!m_timelineWindowQuery)
                return;

            auto filter = m_timelineWindowQuery->filter();
            const bool shouldChange = helpers::isTimeWindowChanged(
                m_timelineWindowQueryFilter.startTimeMs,
                m_timelineWindowQueryFilter.endTimeMs,
                filter.startTimeMs,
                filter.endTimeMs,
                kTimelineMinWindowChange);

            if (!shouldChange)
                return;

            NX_VERBOSE(this, "Timeline window changed to %1 - %2",
                nx::utils::timestampToDebugString(m_timelineWindowQueryFilter.startTimeMs),
                nx::utils::timestampToDebugString(m_timelineWindowQueryFilter.endTimeMs));

            filter.startTimeMs = m_timelineWindowQueryFilter.startTimeMs;
            filter.endTimeMs = m_timelineWindowQueryFilter.endTimeMs;
            m_timelineWindowQuery->setFilter(filter);
        };

    auto updateTimelineWindowQueryOperation = new nx::utils::PendingOperation(
        updateTimelineWindowQueryIfNeeded,
        kUpdateTimelineWindowQueryPeriodMs,
        this);

    m_timelineWindowQueryFilter.sparsing = QnBookmarkSparsingOptions{true, navigator()->timeSlider()->msecsPerPixel()};
    m_timelineWindowQueryFilter.limit = kMaxTimelineWindowBookmarksCount;

    connect(navigator()->timeSlider(), &QnTimeSlider::windowChanged, this,
        [this, updateTimelineWindowQueryOperation](milliseconds startTimeMs, milliseconds endTimeMs)
        {
            if (!m_timelineWindowQuery)
                return;

            if (startTimeMs == 0ms || endTimeMs == 0ms || (startTimeMs > endTimeMs))
                return;

            m_timelineWindowQueryFilter.startTimeMs = startTimeMs;
            m_timelineWindowQueryFilter.endTimeMs = endTimeMs;
            updateTimelineWindowQueryOperation->requestOperation();
        });

    connect(navigator()->timeSlider(), &QnTimeSlider::msecsPerPixelChanged, this,
        [this]()
        {
            m_timelineWindowQueryFilter.sparsing = QnBookmarkSparsingOptions {
                true, navigator()->timeSlider()->msecsPerPixel()};
            if (m_timelineWindowQuery)
            {
                auto filter = m_timelineWindowQuery->filter();
                filter.sparsing = m_timelineWindowQueryFilter.sparsing;
                m_timelineWindowQuery->setFilter(filter);
            }
        });
}

void QnTimelineBookmarksWatcher::updateCurrentCamera()
{
    const bool bookmarksMode = navigator()->bookmarksModeEnabled();
    const auto resourceWidget = navigator()->currentWidget();
    const auto camera = (resourceWidget && bookmarksMode
        ? resourceWidget->resource().dynamicCast<QnVirtualCameraResource>()
        : QnVirtualCameraResourcePtr());

    setCurrentCamera(camera);
}

void QnTimelineBookmarksWatcher::onBookmarkRemoved(const QnUuid& id)
{
    if (m_aggregation->removeBookmark(id))
        setTimelineBookmarks(m_aggregation->bookmarkList());
}

void QnTimelineBookmarksWatcher::tryUpdateTimelineBookmarks(
    const QnVirtualCameraResourcePtr& camera)
{
    if (m_currentCamera != camera)
        return;

    if (!m_currentCamera)
    {
        m_aggregation->clear();
        m_mergeHelper->setBookmarks(QnCameraBookmarkList());
        return;
    }

    const auto staticQuery = m_queriesCache->getOrCreateQuery(m_currentCamera);
    m_aggregation->setBookmarkList(staticQuery->cachedBookmarks());
    m_aggregation->mergeBookmarkList(m_timelineWindowQuery->cachedBookmarks());

    NX_VERBOSE(this, "Merging %1 static bookmarks and %2 actual bookmarks. Total count %3.",
        staticQuery->cachedBookmarks().size(),
        m_timelineWindowQuery->cachedBookmarks().size(),
        m_aggregation->bookmarkList().size());
    setTimelineBookmarks(m_aggregation->bookmarkList());
}

void QnTimelineBookmarksWatcher::setTimelineBookmarks(const QnCameraBookmarkList& bookmarks)
{
    if (m_textFilter.isEmpty())
    {
        NX_VERBOSE(this, "Updating timeline without text filter. %1 bookmarks.",
            bookmarks.size());
        m_mergeHelper->setBookmarks(bookmarks);
    }
    else
    {
        QnCameraBookmarkSearchFilter filter;
        filter.text = m_textFilter;

        QnCameraBookmarkList filtered(bookmarks.size());
        filtered.resize(std::distance(filtered.begin(),
            std::copy_if(bookmarks.cbegin(), bookmarks.cend(), filtered.begin(),
                [&filter](const QnCameraBookmark& bookmark)
                {
                    return filter.checkBookmark(bookmark);
                })));

        NX_VERBOSE(this, "Updating timeline with text filter. Pass %1 bookmarks of %2",
            filtered.size(), bookmarks.size());
        m_mergeHelper->setBookmarks(filtered);
    }
}

void QnTimelineBookmarksWatcher::setCurrentCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (m_currentCamera == camera)
        return;

    m_currentCamera = camera;

    if (m_timelineWindowQuery)
        m_timelineWindowQuery->disconnect(this);

    if (m_currentCamera)
    {
        // Ensure query exists
        ensureStaticQueryForCamera(m_currentCamera);

        m_timelineWindowQuery = qnCameraBookmarksManager->createQuery(
            QnVirtualCameraResourceSet() << m_currentCamera, m_timelineWindowQueryFilter);

        connect(m_timelineWindowQuery, &QnCameraBookmarksQuery::bookmarksChanged, this,
            [this](const QnCameraBookmarkList& bookmarks)
            {
                NX_VERBOSE(this, "Timeline query result: %1 bookmarks", bookmarks.size());
                if (m_currentCamera)
                    tryUpdateTimelineBookmarks(m_currentCamera);
            });
    }

    tryUpdateTimelineBookmarks(m_currentCamera);
}

void QnTimelineBookmarksWatcher::ensureStaticQueryForCamera(
    const QnVirtualCameraResourcePtr& camera)
{
    if (m_queriesCache->hasQuery(camera))
        return;

    const auto query = m_queriesCache->getOrCreateQuery(camera);
    connect(query, &QnCameraBookmarksQuery::bookmarksChanged, this,
        [this, camera](const QnCameraBookmarkList& bookmarks)
        {
            NX_VERBOSE(this, "Static query result: %1 bookmarks", bookmarks.size());
            if (!bookmarks.empty())
            {
                NX_VERBOSE(this, "First bookmark at %1",
                    nx::utils::timestampToDebugString(bookmarks[0].startTimeMs));
                NX_VERBOSE(this, "Last bookmark at %1",
                    nx::utils::timestampToDebugString(bookmarks[bookmarks.size() - 1].startTimeMs));
            }

            tryUpdateTimelineBookmarks(camera);
        });

    auto filter = query->filter();
    filter.limit = kMaxStaticBookmarksCount;
    filter.sparsing.used = true;
    query->setFilter(filter);
}

QString QnTimelineBookmarksWatcher::textFilter() const
{
    return m_textFilter;
}

void QnTimelineBookmarksWatcher::setTextFilter(const QString& value)
{
    if (m_textFilter == value)
        return;

    m_textFilter = value;

    if (m_currentCamera)
        tryUpdateTimelineBookmarks(m_currentCamera);
}
