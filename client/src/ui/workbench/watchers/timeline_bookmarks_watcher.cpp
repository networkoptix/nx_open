
#include "timeline_bookmarks_watcher.h"

#include <utils/common/delayed.h>
#include <utils/camera/bookmark_helpers.h>
#include <core/resource/camera_resource.h>
#include <camera/camera_bookmarks_manager.h>
#include <camera/camera_bookmark_aggregation.h>
#include <ui/utils/bookmark_merge_helper.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/controls/time_slider.h>

#include <camera/bookmark_queries_cache.h>
#include <camera/camera_bookmarks_query.h>
#include <core/resource_management/resource_pool.h>

namespace
{
    enum
    {
        kMaxStaticBookmarksCount = 5000
        , kMaxTimelineBookmarksConut = 500
    };

    void updateQueryWindowExtended(const QnCameraBookmarksQueryPtr &query
        , qint64 startTimeMs
        , qint64 endTimeMs)
    {
        enum { kTimelineMinWindowChangeMs = 20 * 1000 };
        enum { kTimlineWindowShiftSize = kTimelineMinWindowChangeMs * 3};

        auto filter = query->filter();
        const auto newWindow = helpers::extendTimeWindow(startTimeMs, endTimeMs
            , kTimlineWindowShiftSize, kTimlineWindowShiftSize);
        const bool shouldChange = helpers::isTimeWindowChanged(startTimeMs, endTimeMs
            , filter.startTimeMs, filter.endTimeMs, kTimelineMinWindowChangeMs);

        if (!shouldChange)
            return;

        filter.startTimeMs = startTimeMs;
        filter.endTimeMs = endTimeMs;
        query->setFilter(filter);
    }

    void updateQuerySparing(const QnCameraBookmarksQueryPtr &query
        , const QnBookmarkSparsingOptions &options)
    {
        auto filter = query->filter();
        filter.sparsing = options;
        query->setFilter(filter);
    }
}

QnTimelineBookmarksWatcher::QnTimelineBookmarksWatcher(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)

    , m_aggregation(new QnCameraBookmarkAggregation())
    , m_mergeHelper(new QnBookmarkMergeHelper())
    , m_queriesCache(new QnBookmarkQueriesCache())
    , m_timelineQuery()
    , m_currentCamera()

    , m_updateStaticQueriesTimer(new QTimer())
    , m_delayedTimer()
    , m_delayedUpdateCounter()
    , m_timlineFilter()
{
    m_timlineFilter.sparsing.used = true;
    m_timlineFilter.limit = kMaxTimelineBookmarksConut;

    connect(qnResPool, &QnResourcePool::resourceAdded
        , this, &QnTimelineBookmarksWatcher::onResourceAdded);
    connect(qnResPool, &QnResourcePool::resourceRemoved
        , this, &QnTimelineBookmarksWatcher::onResourceRemoved);

    connect(navigator(), &QnWorkbenchNavigator::currentWidgetChanged
        , this, &QnTimelineBookmarksWatcher::updateCurrentCamera);
    connect(navigator(), &QnWorkbenchNavigator::bookmarksModeEnabledChanged
        , this, &QnTimelineBookmarksWatcher::updateCurrentCamera);

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

    const auto &query = m_queriesCache->getOrCreateQuery(camera);
    const auto &bookmarks = query->cachedBookmarks();
    return helpers::bookmarksAtPosition(bookmarks, positionMs);
}

void QnTimelineBookmarksWatcher::onResourceAdded(const QnResourcePtr &resource)
{
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    const auto query = m_queriesCache->getOrCreateQuery(camera);
    connect(query, &QnCameraBookmarksQuery::bookmarksChanged, this
        , [this, camera](const QnCameraBookmarkList & /*bookmarks */)
    {
        tryUpdateTimelineBookmarks(camera);
    });

    auto filter = query->filter();
    filter.limit = kMaxStaticBookmarksCount;
    filter.sparsing.used = true;
    query->setFilter(filter);
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

void QnTimelineBookmarksWatcher::onTimelineWindowChanged(qint64 startTimeMs
    , qint64 endTimeMs)
{
    if (!m_timelineQuery)
        return;

    if (!startTimeMs || !endTimeMs || (startTimeMs > endTimeMs))
        return;

    const auto delayedUpdateWindow = [this, startTimeMs, endTimeMs]()
    {
        m_delayedTimer.reset(); // Have to reset manually to prevent double deletion

        m_timlineFilter.startTimeMs = startTimeMs;
        m_timlineFilter.endTimeMs = endTimeMs;

        if (!m_timelineQuery)
            return;

        m_delayedUpdateCounter.restart();
        updateQueryWindowExtended(m_timelineQuery, startTimeMs, endTimeMs);
    };

    enum { kMaxUpdateTimeMs = 5000};
    if (!m_delayedUpdateCounter.isValid() || m_delayedUpdateCounter.hasExpired(kMaxUpdateTimeMs))
    {
        delayedUpdateWindow();
        return;
    }

    enum { kDelayUpdateMs = 200 };
    m_delayedTimer.reset(executeDelayedParented(delayedUpdateWindow, kDelayUpdateMs, this));
}

void QnTimelineBookmarksWatcher::setCurrentCamera(const QnVirtualCameraResourcePtr &camera)
{
    if (m_currentCamera == camera)
        return;

    m_currentCamera = camera;

    if (m_timelineQuery)
        disconnect(m_timelineQuery, nullptr, this, nullptr);

    if (m_currentCamera)
    {
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