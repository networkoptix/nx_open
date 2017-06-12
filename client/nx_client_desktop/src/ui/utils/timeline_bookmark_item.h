#pragma once

#ifndef Q_MOC_RUN
#include <boost/optional/optional.hpp>
#endif

#include <core/resource/camera_bookmark.h>

struct QnBookmarkCluster {
    qint64 startTimeMs;
    qint64 durationMs;

    qint64 endTimeMs() const;

    QnBookmarkCluster();
};

class QnTimelineBookmarkItem
{
public:
    QnTimelineBookmarkItem(const QnCameraBookmark &bookmark);
    QnTimelineBookmarkItem(const QnBookmarkCluster &cluster);

    bool isCluster() const;

    QnCameraBookmark bookmark() const;
    QnBookmarkCluster cluster() const;

    qint64 startTimeMs() const;
    qint64 endTimeMs() const;

private:
    boost::optional<QnCameraBookmark> m_bookmark;
    boost::optional<QnBookmarkCluster> m_cluster;
};

typedef QList<QnTimelineBookmarkItem> QnTimelineBookmarkItemList;
