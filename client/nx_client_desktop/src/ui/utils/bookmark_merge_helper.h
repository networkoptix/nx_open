#pragma once

#include <utils/common/id.h>

#include "timeline_bookmark_item.h"

class QnBookmarkMergeHelperPrivate;

class QnBookmarkMergeHelper
{
public:
    enum BookmarkSearchOption
    {
        OnlyTopmost = 0x01,

        // If there is no bookmarks at the given position, search the nearest in the 20px window.
        ExpandArea = 0x02,
    };
    Q_DECLARE_FLAGS(BookmarkSearchOptions, BookmarkSearchOption)

    QnBookmarkMergeHelper();
    ~QnBookmarkMergeHelper();

    void setBookmarks(const QnCameraBookmarkList &bookmarks);

    QnTimelineBookmarkItemList bookmarks(qint64 msecsPerDp = 0) const;

    /** Find the topmost (or nearest) item at the given position from the provided list. */
    static int indexAtPosition(const QnTimelineBookmarkItemList& bookmarks,
        qint64 timeMs,
        int msecsPerDp = 0,
        BookmarkSearchOptions options = OnlyTopmost);

    QnCameraBookmarkList bookmarksAtPosition(qint64 timeMs, int msecsPerDp = 0,
        BookmarkSearchOptions options = OnlyTopmost) const;

    void addBookmark(const QnCameraBookmark &bookmark);
    void removeBookmark(const QnCameraBookmark &bookmark);

private:
    Q_DECLARE_PRIVATE(QnBookmarkMergeHelper)
    QScopedPointer<QnBookmarkMergeHelperPrivate> const d_ptr;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnBookmarkMergeHelper::BookmarkSearchOptions)
