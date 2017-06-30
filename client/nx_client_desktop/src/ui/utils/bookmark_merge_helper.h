#pragma once

#include <utils/common/id.h>

#include "timeline_bookmark_item.h"

class QnBookmarkMergeHelperPrivate;

class QnBookmarkMergeHelper
{
public:
    QnBookmarkMergeHelper();
    ~QnBookmarkMergeHelper();

    void setBookmarks(const QnCameraBookmarkList &bookmarks);

    QnTimelineBookmarkItemList bookmarks(qint64 msecsPerDp = 0) const;
    QnCameraBookmarkList bookmarksAtPosition(qint64 timeMs, int msecsPerDp = 0, bool onlyTopmost = true) const;

    void addBookmark(const QnCameraBookmark &bookmark);
    void removeBookmark(const QnCameraBookmark &bookmark);

private:
    Q_DECLARE_PRIVATE(QnBookmarkMergeHelper)
    QScopedPointer<QnBookmarkMergeHelperPrivate> const d_ptr;
};
