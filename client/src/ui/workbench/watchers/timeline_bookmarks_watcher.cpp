
#include "timeline_bookmarks_watcher.h"

#include <utils/common/delayed.h>
#include <utils/camera/bookmark_helpers.h>
#include <core/resource/camera_resource.h>
#include <camera/camera_bookmarks_manager.h>
#include <camera/camera_bookmark_aggregation.h>
#include <ui/utils/bookmark_merge_helper.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_bookmarks_cache.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/controls/time_slider.h>

namespace
{
    enum { kMaxBookmarksOnTimeline = 10000 };

    enum { kTimelineMinWindowChangeMs = 60000 };
}

// TODO: add switch on/off bookmarks mode helper

QnTimelineBookmarksWatcher::QnTimelineBookmarksWatcher(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)

    , m_bookmarksCache(new QnWorkbenchBookmarksCache(kMaxBookmarksOnTimeline
        , QnBookmarkSortOrder::default, QnBookmarkSparsingOptions(true, 0)
        , kTimelineMinWindowChangeMs, parent))
    , m_mergeHelper(new QnBookmarkMergeHelper())
    , m_aggregation(new QnCameraBookmarkAggregation())
    , m_currentCamera()
    , m_delayedTimer()
    , m_delayedUpdateCounter()
{
    connect(navigator(), &QnWorkbenchNavigator::currentWidgetChanged
        , this, &QnTimelineBookmarksWatcher::updateCurrentCamera);
    connect(navigator(), &QnWorkbenchNavigator::bookmarksModeEnabledChanged
        , this, &QnTimelineBookmarksWatcher::updateCurrentCamera);
    connect(m_bookmarksCache, &QnWorkbenchBookmarksCache::bookmarksChanged
        , this, &QnTimelineBookmarksWatcher::onBookmarksChanged);
    connect(qnCameraBookmarksManager, &QnCameraBookmarksManager::bookmarkRemoved
        , this, &QnTimelineBookmarksWatcher::onBookmarkRemoved);

    // TODO: #ynikitenkov Remove dependency from time slider?
    connect(navigator()->timeSlider(), &QnTimeSlider::windowChanged
        , this, &QnTimelineBookmarksWatcher::onTimelineWindowChanged);
    connect(navigator()->timeSlider(), &QnTimeSlider::msecsPerPixel, this, [this]()
    {
        const auto sparsing = QnBookmarkSparsingOptions(true
            , navigator()->timeSlider()->msecsPerPixel());
        m_bookmarksCache->setsparsing(sparsing);
    });

    navigator()->timeSlider()->setBookmarksHelper(m_mergeHelper);
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
    return helpers::bookmarksAtPosition(m_bookmarksCache->bookmarks(camera), positionMs);
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

void QnTimelineBookmarksWatcher::onBookmarksChanged(const QnVirtualCameraResourcePtr &camera
    , const QnCameraBookmarkList &bookmarks)
{
    if (m_currentCamera != camera)
        return;

    m_aggregation->setBookmarkList(bookmarks);
    m_mergeHelper->setBookmarks(m_aggregation->bookmarkList());
}

void QnTimelineBookmarksWatcher::onTimelineWindowChanged(qint64 startTimeMs
    , qint64 endTimeMs)
{
    const auto delayedUpdateWindow = [this, startTimeMs, endTimeMs]()
    {
        m_delayedTimer.reset(); // Have to reset manually to prevent double deletion
        m_bookmarksCache->setWindow(QnTimePeriod::fromInterval(startTimeMs, endTimeMs));
        m_delayedUpdateCounter.invalidate();
        m_delayedUpdateCounter.start();
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
    m_aggregation->setBookmarkList(m_bookmarksCache->bookmarks(m_currentCamera));
    m_mergeHelper->setBookmarks(m_aggregation->bookmarkList());
}