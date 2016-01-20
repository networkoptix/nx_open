
#include "current_layout_bookmarks_watcher.h"

#include <camera/current_layout_bookmarks_cache.h>

namespace
{
    enum { kMaxBookmarksNearThePosition = 512 };
}

QnCurrentLayoutBookmarksWatcher::QnCurrentLayoutBookmarksWatcher(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)

    , m_bookmarksCache(new QnCurrentLayoutBookmarksCache(kMaxBookmarksNearThePosition, Qn::EarliestFirst, parent))
{
}

QnCurrentLayoutBookmarksWatcher::~QnCurrentLayoutBookmarksWatcher()
{}

void QnCurrentLayoutBookmarksWatcher::setPosition(qint64 pos)
{
}

