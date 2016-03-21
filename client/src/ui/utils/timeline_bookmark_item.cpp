#include "timeline_bookmark_item.h"

QnBookmarkCluster::QnBookmarkCluster()
    : startTimeMs(0)
    , durationMs(0)
{
}

qint64 QnBookmarkCluster::endTimeMs() const
{
    return startTimeMs + durationMs;
}


QnTimelineBookmarkItem::QnTimelineBookmarkItem(const QnCameraBookmark &bookmark)
    : m_bookmark(bookmark)
{
}

QnTimelineBookmarkItem::QnTimelineBookmarkItem(const QnBookmarkCluster &cluster)
    : m_cluster(cluster)
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

qint64 QnTimelineBookmarkItem::startTimeMs() const
{
    if (m_bookmark)
        return m_bookmark.get().startTimeMs;
    if (m_cluster)
        return m_cluster.get().startTimeMs;
    return 0;
}

qint64 QnTimelineBookmarkItem::endTimeMs() const
{
    if (m_bookmark)
        return m_bookmark.get().endTimeMs();
    if (m_cluster)
        return m_cluster.get().endTimeMs();
    return 0;
}
