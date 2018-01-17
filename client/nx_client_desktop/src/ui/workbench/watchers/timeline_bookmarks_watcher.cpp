#include "timeline_bookmarks_watcher.h"

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
#include <nx/utils/pending_operation.h>

namespace {

static constexpr int kMaxStaticBookmarksCount = 5000;
static constexpr int kMaxTimelineBookmarksCount = 500;
static constexpr int kUpdateQueryPeriodMs = 200;
static constexpr int kTimelineMinWindowChangeMs = 20 * 1000;

void updateQueryWindowExtended(const QnCameraBookmarksQueryPtr& query, qint64 startTimeMs,
    qint64 endTimeMs)
{
    auto filter = query->filter();
    const bool shouldChange = helpers::isTimeWindowChanged(startTimeMs, endTimeMs,
        filter.startTimeMs, filter.endTimeMs, kTimelineMinWindowChangeMs);

    if (!shouldChange)
        return;

    filter.startTimeMs = startTimeMs;
    filter.endTimeMs = endTimeMs;
    query->setFilter(filter);
}

void updateQuerySparing(const QnCameraBookmarksQueryPtr&query,
    const QnBookmarkSparsingOptions& options)
{
    auto filter = query->filter();
    filter.sparsing = options;
    query->setFilter(filter);
}

} // namespace

QnTimelineBookmarksWatcher::QnTimelineBookmarksWatcher(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)

    , m_aggregation(new QnCameraBookmarkAggregation())
    , m_mergeHelper(new QnBookmarkMergeHelper())
    , m_queriesCache(new QnBookmarkQueriesCache())
    , m_timelineQuery()
    , m_currentCamera()
    , m_updateStaticQueriesTimer(new QTimer())
{
    const auto delayedUpdateWindow =
        [this]()
        {
            if (!m_timelineQuery)
                return;

            updateQueryWindowExtended(m_timelineQuery, m_timlineFilter.startTimeMs,
                m_timlineFilter.endTimeMs);
        };
    m_updateQueryOperation = new nx::utils::PendingOperation(delayedUpdateWindow,
        kUpdateQueryPeriodMs, this);

    m_timlineFilter.sparsing.used = true;
    m_timlineFilter.limit = kMaxTimelineBookmarksCount;

    connect(resourcePool(), &QnResourcePool::resourceRemoved
        , this, &QnTimelineBookmarksWatcher::onResourceRemoved);

    connect(navigator(), &QnWorkbenchNavigator::currentWidgetChanged
        , this, &QnTimelineBookmarksWatcher::updateCurrentCamera);
    connect(navigator(), &QnWorkbenchNavigator::bookmarksModeEnabledChanged
        , this, &QnTimelineBookmarksWatcher::updateCurrentCamera);

    connect(display(), &QnWorkbenchDisplay::widgetAdded, this,
        [this](QnResourceWidget* widget)
        {
            const auto& resource = widget->resource();
            if (const auto& camera = resource.dynamicCast<QnVirtualCameraResource>())
                ensureQueryForCamera(camera);
        });

    connect(qnCameraBookmarksManager, &QnCameraBookmarksManager::bookmarkRemoved
        , this, &QnTimelineBookmarksWatcher::onBookmarkRemoved);

    // TODO: #ynikitenkov Remove dependency from time slider? and metrics below to navigator?
    connect(navigator()->timeSlider(), &QnTimeSlider::windowChanged
        , this, &QnTimelineBookmarksWatcher::onTimelineWindowChanged);
    connect(navigator()->timeSlider(), &QnTimeSlider::msecsPerPixelChanged, this, [this]()
    {
        m_timlineFilter.sparsing = QnBookmarkSparsingOptions(true
            , navigator()->timeSlider()->msecsPerPixel());
        if (m_timelineQuery)
            updateQuerySparing(m_timelineQuery, m_timlineFilter.sparsing);
    });
    navigator()->timeSlider()->setBookmarksHelper(m_mergeHelper);

    enum { kUpdateStaticQueriesIntervalMs = 5 * 60 * 1000};
    m_updateStaticQueriesTimer->setInterval(kUpdateStaticQueriesIntervalMs);
    connect(m_updateStaticQueriesTimer, &QTimer::timeout
        , m_queriesCache, &QnBookmarkQueriesCache::refreshQueries);
    m_updateStaticQueriesTimer->start();
}

QnTimelineBookmarksWatcher::~QnTimelineBookmarksWatcher()
{
}

QnCameraBookmarkList QnTimelineBookmarksWatcher::bookmarksAtPosition(qint64 position
    , qint64 msecdsPerDp)
{
    return m_mergeHelper->bookmarksAtPosition(position, msecdsPerDp);
}

QnCameraBookmarkList QnTimelineBookmarksWatcher::rawBookmarksAtPosition(
    const QnVirtualCameraResourcePtr &camera
    , qint64 positionMs)
{
    if (!m_queriesCache->hasQuery(camera))
        return QnCameraBookmarkList();

    if (camera == m_currentCamera)
        return helpers::bookmarksAtPosition(m_aggregation->bookmarkList(), positionMs);

    const auto &query = m_queriesCache->getOrCreateQuery(camera);
    const auto &bookmarks = query->cachedBookmarks();

    return helpers::bookmarksAtPosition(bookmarks, positionMs);
}

void QnTimelineBookmarksWatcher::onResourceRemoved(const QnResourcePtr &resource)
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

void QnTimelineBookmarksWatcher::updateCurrentCamera()
{
    const bool bookmarksMode = navigator()->bookmarksModeEnabled();
    const auto resourceWidget = navigator()->currentWidget();
    const auto camera = (resourceWidget && bookmarksMode
        ? resourceWidget->resource().dynamicCast<QnVirtualCameraResource>()
        : QnVirtualCameraResourcePtr());

    setCurrentCamera(camera);
}

void QnTimelineBookmarksWatcher::onBookmarkRemoved(const QnUuid &id)
{
    if (m_aggregation->removeBookmark(id))
        m_mergeHelper->setBookmarks(m_aggregation->bookmarkList());
}

void QnTimelineBookmarksWatcher::tryUpdateTimelineBookmarks(const QnVirtualCameraResourcePtr &camera)
{
    QN_LOG_TIME(Q_FUNC_INFO);

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
    m_aggregation->mergeBookmarkList(m_timelineQuery->cachedBookmarks());

    m_mergeHelper->setBookmarks(m_aggregation->bookmarkList());
}

void QnTimelineBookmarksWatcher::onTimelineWindowChanged(qint64 startTimeMs, qint64 endTimeMs)
{
    if (!m_timelineQuery)
        return;

    if (!startTimeMs || !endTimeMs || (startTimeMs > endTimeMs))
        return;

    m_timlineFilter.startTimeMs = startTimeMs;
    m_timlineFilter.endTimeMs = endTimeMs;
    m_updateQueryOperation->requestOperation();
}

void QnTimelineBookmarksWatcher::setCurrentCamera(const QnVirtualCameraResourcePtr &camera)
{
    QN_LOG_TIME(Q_FUNC_INFO);

    if (m_currentCamera == camera)
        return;

    m_currentCamera = camera;

    if (m_timelineQuery)
        disconnect(m_timelineQuery, nullptr, this, nullptr);

    if (m_currentCamera)
    {
        // Ensure query exists
        ensureQueryForCamera(m_currentCamera);

        m_timelineQuery = qnCameraBookmarksManager->createQuery(
            QnVirtualCameraResourceSet() << m_currentCamera, m_timlineFilter);

        connect(m_timelineQuery, &QnCameraBookmarksQuery::bookmarksChanged, this
            , [this](const QnCameraBookmarkList & /*bookmarks*/)
        {
            if (m_currentCamera)
                tryUpdateTimelineBookmarks(m_currentCamera);
        });
    }

    tryUpdateTimelineBookmarks(m_currentCamera);
}

void QnTimelineBookmarksWatcher::ensureQueryForCamera(const QnVirtualCameraResourcePtr &camera)
{
    if (m_queriesCache->hasQuery(camera))
        return;

    const auto query = m_queriesCache->getOrCreateQuery(camera);
    connect(query, &QnCameraBookmarksQuery::bookmarksChanged, this,
        [this, camera](const QnCameraBookmarkList & /*bookmarks */)
        {
            tryUpdateTimelineBookmarks(camera);
        });

    auto filter = query->filter();
    filter.limit = kMaxStaticBookmarksCount;
    filter.sparsing.used = true;
    query->setFilter(filter);
}
