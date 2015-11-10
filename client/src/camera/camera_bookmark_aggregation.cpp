#include "camera_bookmark_aggregation.h"

#include <core/resource/camera_bookmark.h>

QnCameraBookmarkAggregation::QnCameraBookmarkAggregation(const QnCameraBookmarkList &bookmarkList) {
    setBookmarkList(bookmarkList);
}

void QnCameraBookmarkAggregation::addBookmark(const QnCameraBookmark &bookmark) {
    auto it = std::lower_bound(m_bookmarkList.begin(), m_bookmarkList.end(), bookmark);

    if (it->guid == bookmark.guid) {
        *it = bookmark;
    } else {
        if (m_bookmarkIds.contains(bookmark.guid)) {
            auto predicate = [&bookmark](const QnCameraBookmark &other) {
                return bookmark.guid == other.guid;
            };
            auto oldIt = std::find_if(m_bookmarkList.begin(), m_bookmarkList.end(), predicate);

            Q_ASSERT(oldIt != m_bookmarkList.end());
            if (oldIt != m_bookmarkList.end()) {
                if (oldIt < it) {
                    /* For optimization purposes we suppose the iterator behaves like a pointer.
                     * So if we removed something before 'it' then 'it' should be decreased. */
                    --it;
                }
                m_bookmarkList.erase(oldIt);
            }
        } else {
            m_bookmarkIds.insert(bookmark.guid);
        }
        m_bookmarkList.insert(it, bookmark);
    }
}

void QnCameraBookmarkAggregation::mergeBookmarkList(const QnCameraBookmarkList &bookmarkList) {
    if (m_bookmarkList.isEmpty())
        setBookmarkList(bookmarkList);

    for (const QnCameraBookmark &bookmark: bookmarkList)
        addBookmark(bookmark);
}

bool QnCameraBookmarkAggregation::removeBookmark(const QnUuid &bookmarkId) {
    if (!m_bookmarkIds.remove(bookmarkId))
        return false;

    auto it = std::find_if(m_bookmarkList.begin(), m_bookmarkList.end(), [&bookmarkId](const QnCameraBookmark &bookmark){
        return bookmark.guid == bookmarkId;
    });

    Q_ASSERT_X(it != m_bookmarkList.end(), Q_FUNC_INFO, "Bookmark should be present in both id set and list");
    if (it == m_bookmarkList.end())
        return false;

    m_bookmarkList.erase(it);

    return true;
}

void QnCameraBookmarkAggregation::setBookmarkList(const QnCameraBookmarkList &bookmarkList) {
    m_bookmarkList = bookmarkList;
    m_bookmarkIds.clear();

    for (const QnCameraBookmark &bookmark: bookmarkList)
        m_bookmarkIds.insert(bookmark.guid);
}

const QnCameraBookmarkList &QnCameraBookmarkAggregation::bookmarkList() const {
    return m_bookmarkList;
}

void QnCameraBookmarkAggregation::clear() {
    m_bookmarkList.clear();
    m_bookmarkIds.clear();
}

