
#include "timeline_bookmarks_watcher.h"

#include <utils/camera/bookmark_helpers.h>
#include <core/resource/camera_resource.h>
#include <camera/camera_bookmarks_manager.h>
#include <camera/camera_bookmark_aggregation.h>
#include <camera/current_layout_bookmarks_cache.h>
#include <ui/utils/bookmark_merge_helper.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/controls/time_slider.h>

namespace
{
    enum { kMaxBookmarksOnTimeline = 10000 };

    enum { kTimelineMinWindowChangeMs = 30000 };
}

// TODO: add switch on/off bookmarks mode helper

QnTimelineBookmarksWatcher::QnTimelineBookmarksWatcher(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)

    , m_bookmarksCache(new QnCurrentLayoutBookmarksCache(kMaxBookmarksOnTimeline
        , QnBookmarkSortProps::default, kTimelineMinWindowChangeMs, parent))
    , m_mergeHelper(new QnBookmarkMergeHelper())
    , m_aggregation(new QnCameraBookmarkAggregation())
    , m_currentCamera()
{
    connect(navigator(), &QnWorkbenchNavigator::currentWidgetChanged
        , this, &QnTimelineBookmarksWatcher::onWorkbenchCurrentWidgetChanged);
    connect(navigator(), &QnWorkbenchNavigator::bookmarksModeEnabledChanged
        , this, &QnTimelineBookmarksWatcher::onBookmarksModeEnabledChanged);
    connect(m_bookmarksCache, &QnCurrentLayoutBookmarksCache::bookmarksChanged
        , this, &QnTimelineBookmarksWatcher::onBookmarksChanged);
    connect(qnCameraBookmarksManager, &QnCameraBookmarksManager::bookmarkRemoved
        , this, &QnTimelineBookmarksWatcher::onBookmarkRemoved);

    // TODO: #ynikitenkov Remove dependency from time slider
    connect(navigator()->timeSlider(), &QnTimeSlider::windowChanged
        , this, &QnTimelineBookmarksWatcher::onTimelineWindowChanged);
}

QnTimelineBookmarksWatcher::~QnTimelineBookmarksWatcher()
{
}

QnTimelineBookmarkItemList QnTimelineBookmarksWatcher::mergedBookmarks(qint64 msecsPerDp)
{
    return m_mergeHelper->bookmarks(msecsPerDp);
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
    return helpers::bookmarksAtPosition(m_bookmarksCache->bookmarks(camera), positionMs);
}

void QnTimelineBookmarksWatcher::onWorkbenchCurrentWidgetChanged()
{
    const auto resourceWidget = navigator()->currentWidget();
    const auto camera = (resourceWidget
        ? resourceWidget->resource().dynamicCast<QnVirtualCameraResource>()
        : QnVirtualCameraResourcePtr());

    setCurrentCamera(camera);
}

void QnTimelineBookmarksWatcher::onBookmarksModeEnabledChanged()
{
    const bool bookmarksMode = navigator()->bookmarksModeEnabled();
    if (bookmarksMode)
        onWorkbenchCurrentWidgetChanged();  // Sets current camera
    else
        setCurrentCamera(QnVirtualCameraResourcePtr());
}

void QnTimelineBookmarksWatcher::onBookmarkRemoved(const QnUuid &id)
{
    m_aggregation->removeBookmark(id);
    m_mergeHelper->setBookmarks(m_aggregation->bookmarkList());
}

void QnTimelineBookmarksWatcher::onBookmarksChanged(const QnVirtualCameraResourcePtr &camera
    , const QnCameraBookmarkList &bookmarks)
{
    if (m_currentCamera != camera)
        return;

    m_aggregation->mergeBookmarkList(bookmarks);
    m_mergeHelper->setBookmarks(m_aggregation->bookmarkList());
}

void QnTimelineBookmarksWatcher::onTimelineWindowChanged(qint64 startTimeMs
    , qint64 endTimeMs)
{
    m_bookmarksCache->setWindow(QnTimePeriod::createFromInterval(startTimeMs, endTimeMs));
}

void QnTimelineBookmarksWatcher::setCurrentCamera(const QnVirtualCameraResourcePtr &camera)
{
    if (m_currentCamera == camera)
        return;

    m_currentCamera = camera;
    m_aggregation->setBookmarkList(m_bookmarksCache->bookmarks(m_currentCamera));
    m_mergeHelper->setBookmarks(m_aggregation->bookmarkList());
}