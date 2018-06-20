#include "timeline_bookmark_item.h"

#include <chrono>

using std::chrono::milliseconds;
using namespace std::literals::chrono_literals;

milliseconds QnBookmarkCluster::endTime() const
{
    return startTime + duration;
}


QnTimelineBookmarkItem::QnTimelineBookmarkItem(const QnCameraBookmark &bookmark):
    m_bookmark(bookmark)
{
}

QnTimelineBookmarkItem::QnTimelineBookmarkItem(const QnBookmarkCluster &cluster):
    m_cluster(cluster)
{
}

bool QnTimelineBookmarkItem::isCluster() const
{
    return m_cluster.is_initialized();
}

QnCameraBookmark QnTimelineBookmarkItem::bookmark() const
{
    return m_bookmark.get();
}

QnBookmarkCluster QnTimelineBookmarkItem::cluster() const
{
    return m_cluster.get();
}

milliseconds QnTimelineBookmarkItem::startTime() const
{
    if (m_bookmark)
        return m_bookmark.get().startTimeMs;
    if (m_cluster)
        return m_cluster.get().startTime;
    return 0ms;
}

milliseconds QnTimelineBookmarkItem::endTime() const
{
    if (m_bookmark)
        return milliseconds(m_bookmark.get().endTime());
    if (m_cluster)
        return m_cluster.get().endTime();
    return 0ms;
}
