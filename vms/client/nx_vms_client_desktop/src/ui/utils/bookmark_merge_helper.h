// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QScopedPointer>

#include <utils/common/id.h>

#include "timeline_bookmark_item.h"

class QnBookmarkMergeHelperPrivate;

class QnBookmarkMergeHelper
{
public:
    using milliseconds = std::chrono::milliseconds;

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

    QnTimelineBookmarkItemList bookmarks(milliseconds msecsPerDp = milliseconds(0)) const;

    //** Find the topmost (or nearest) item at the given position from the provided list. */
    static int indexAtPosition(const QnTimelineBookmarkItemList& bookmarks,
        milliseconds timeMs,
        milliseconds msecsPerDp = milliseconds(0),
        BookmarkSearchOptions options = OnlyTopmost);

    QnCameraBookmarkList bookmarksAtPosition(milliseconds timeMs,
        milliseconds msecsPerDp = milliseconds(0), BookmarkSearchOptions options = OnlyTopmost) const;

    void addBookmark(const QnCameraBookmark &bookmark);
    void removeBookmark(const QnCameraBookmark &bookmark);

private:
    Q_DECLARE_PRIVATE(QnBookmarkMergeHelper)
    QScopedPointer<QnBookmarkMergeHelperPrivate> const d_ptr;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnBookmarkMergeHelper::BookmarkSearchOptions)
