// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>

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
    std::optional<QnCameraBookmark> m_bookmark;
    std::optional<QnBookmarkCluster> m_cluster;
};

typedef QList<QnTimelineBookmarkItem> QnTimelineBookmarkItemList;
