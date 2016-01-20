
#include "timeline_bookmarks_watcher.h"

#include <core/resource/camera_resource.h>
#include <camera/bookmarks_cache.h>
#include <ui/utils/bookmark_merge_helper.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/graphics/items/resource/resource_widget.h>
namespace
{
    enum { kMaxBookmarksOnTimeline = 10000 };
}

// TODO: add switch on/off bookmarks mode helper

QnTimelineBookmarksWatcher::QnTimelineBookmarksWatcher(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)

    , m_bookmarksCache(new QnCurrentLayoutBookmarksCache(kMaxBookmarksOnTimeline, Qn::EarliestFirst, parent))
    , m_mergeHelper(new QnBookmarkMergeHelper())
    , m_currentCamera()
{
    connect(navigator(), &QnWorkbenchNavigator::currentWidgetChanged
        , this, &QnTimelineBookmarksWatcher::onWorkbenchCurrentWidgetChanged);
    connect(navigator(), &QnWorkbenchNavigator::bookmarksModeEnabledChanged
        , this, &QnTimelineBookmarksWatcher::onBookmarksModeEnabledChanged);
    connect(m_bookmarksCache, &QnCurrentLayoutBookmarksCache::bookmarksChanged
        , this, &QnTimelineBookmarksWatcher::onBookmarksChanged);
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

void QnTimelineBookmarksWatcher::onBookmarksChanged(const QnVirtualCameraResourcePtr &camera
    , const QnCameraBookmarkList &bookmarks)
{
    if (m_currentCamera != camera)
        return;

    m_mergeHelper->setBookmarks(bookmarks);
}

void QnTimelineBookmarksWatcher::setCurrentCamera(const QnVirtualCameraResourcePtr &camera)
{
    if (m_currentCamera == camera)
        return;

    m_currentCamera = camera;
    m_mergeHelper->setBookmarks(m_bookmarksCache->bookmarks(m_currentCamera));
}