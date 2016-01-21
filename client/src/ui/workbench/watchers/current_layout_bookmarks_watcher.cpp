
#include "current_layout_bookmarks_watcher.h"

#include <camera/current_layout_bookmarks_cache.h>
#include <ui/workbench/workbench_navigator.h>

namespace
{
    enum { kMaxBookmarksNearThePosition = 512 };

    enum
    {
        kWindowWidthMs = 30000      // 15 seconds before and after current position
        , kLeftOffset = kWindowWidthMs / 2
        , kRightOffset = kLeftOffset

        , kMinWindowChange = kRightOffset / 3
    };
}

QnCurrentLayoutBookmarksWatcher::QnCurrentLayoutBookmarksWatcher(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)

    , m_bookmarksCache(new QnCurrentLayoutBookmarksCache(kMaxBookmarksNearThePosition
        , Qn::EarliestFirst, kMinWindowChange, parent))
{
    connect(navigator(), &QnWorkbenchNavigator::positionChanged
        , this, &QnCurrentLayoutBookmarksWatcher::updatePosition);
}

QnCurrentLayoutBookmarksWatcher::~QnCurrentLayoutBookmarksWatcher()
{}

void QnCurrentLayoutBookmarksWatcher::updatePosition()
{
    enum { kMsInUsec = 1000 };
    const auto pos = navigator()->positionUsec() / kMsInUsec;
    const QnTimePeriod newWindow = QnTimePeriod::createFromInterval(
        pos - kLeftOffset, pos + kRightOffset);
    m_bookmarksCache->setWindow(newWindow);
}

