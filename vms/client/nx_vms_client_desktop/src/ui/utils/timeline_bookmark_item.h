#pragma once

#include <chrono>

#ifndef Q_MOC_RUN
#include <boost/optional/optional.hpp>
#endif

#include <core/resource/camera_bookmark.h>

struct QnBookmarkCluster
{
    using milliseconds = std::chrono::milliseconds;

    milliseconds startTime = milliseconds(0);
    milliseconds duration = milliseconds(0);

    milliseconds endTime() const;
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

    milliseconds startTime() const;
    milliseconds endTime() const;

private:
    boost::optional<QnCameraBookmark> m_bookmark;
    boost::optional<QnBookmarkCluster> m_cluster;
};

typedef QList<QnTimelineBookmarkItem> QnTimelineBookmarkItemList;
