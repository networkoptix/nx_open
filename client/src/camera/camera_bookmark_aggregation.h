#pragma once

#include <QtCore/QSet>

#include <core/resource/camera_bookmark.h>

#include <utils/common/id.h>

class QnCameraBookmarkAggregation {
public:
    QnCameraBookmarkAggregation(const QnCameraBookmarkList &bookmarkList = QnCameraBookmarkList());

    // @return Returns true if bookmarks list has changed
    bool addBookmark(const QnCameraBookmark &bookmark);

    // @return Returns true if bookmarks list has changed
    bool mergeBookmarkList(const QnCameraBookmarkList &bookmarkList);

    bool removeBookmark(const QnUuid &bookmarkId);

    void setBookmarkList(const QnCameraBookmarkList &bookmarkList);
    const QnCameraBookmarkList &bookmarkList() const;

    void clear();

private:
    QnCameraBookmarkList m_bookmarkList;
    QSet<QnUuid> m_bookmarkIds;
};
