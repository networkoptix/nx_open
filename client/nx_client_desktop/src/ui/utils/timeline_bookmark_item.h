#pragma once

#include <chrono>

#ifndef Q_MOC_RUN
#include <boost/optional/optional.hpp>
#endif

#include <core/resource/camera_bookmark.h>

struct QnBookmarkCluster
{
    using milliseconds = std::chrono::milliseconds;
    milliseconds startTimeMs;
    milliseconds durationMs;

    milliseconds endTimeMs() const;

    QnBookmarkCluster();
};

class QnTimelineBookmarkItem
{
public:
    using milliseconds = std::chrono::milliseconds;

    QnTimelineBookmarkItem(const QnCameraBookmark &bookmark);
    QnTimelineBookmarkItem(const QnBookmarkCluster &cluster);

    bool isCluster() const;

    QnCameraBookmark bookmark() const;
    QnBookmarkCluster cluster() const;

    milliseconds startTimeMs() const;
    milliseconds endTimeMs() const;

private:
    boost::optional<QnCameraBookmark> m_bookmark;
    boost::optional<QnBookmarkCluster> m_cluster;
};

typedef QList<QnTimelineBookmarkItem> QnTimelineBookmarkItemList;
