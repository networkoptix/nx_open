// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    return m_cluster.has_value();
}

QnCameraBookmark QnTimelineBookmarkItem::bookmark() const
{
    return *m_bookmark;
}

QnBookmarkCluster QnTimelineBookmarkItem::cluster() const
{
    return *m_cluster;
}

milliseconds QnTimelineBookmarkItem::startTime() const
{
    if (m_bookmark)
        return m_bookmark->startTimeMs;
    if (m_cluster)
        return m_cluster->startTime;
    return 0ms;
}

milliseconds QnTimelineBookmarkItem::endTime() const
{
    if (m_bookmark)
        return milliseconds(m_bookmark->endTime());
    if (m_cluster)
        return m_cluster->endTime();
    return 0ms;
}
