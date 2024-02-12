// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSet>

#include <core/resource/camera_bookmark.h>
#include <utils/common/id.h>

/**
 * Extension of the simple bookmark list.
 * Maintains the bookmark list in sorted order (by start time).
 */
class NX_VMS_CLIENT_DESKTOP_API QnCameraBookmarkAggregation
{
public:
    QnCameraBookmarkAggregation(const QnCameraBookmarkList &bookmarkList = QnCameraBookmarkList());

    // @return Returns true if bookmarks list has changed
    bool addBookmark(const QnCameraBookmark &bookmark);

    // @return Returns true if bookmarks list has changed
    bool mergeBookmarkList(const QnCameraBookmarkList &bookmarkList);

    bool removeBookmark(const nx::Uuid &bookmarkId);

    void setBookmarkList(const QnCameraBookmarkList &bookmarkList);
    const QnCameraBookmarkList &bookmarkList() const;

    void clear();

private:
    QnCameraBookmarkList m_bookmarkList;
    QSet<nx::Uuid> m_bookmarkIds;
};
