#include "camera_bookmark_aggregation.h"

#include <core/resource/camera_bookmark.h>

#include <utils/common/scoped_timer.h>

QnCameraBookmarkAggregation::QnCameraBookmarkAggregation(const QnCameraBookmarkList &bookmarkList) {
    setBookmarkList(bookmarkList);
}

bool QnCameraBookmarkAggregation::addBookmark(const QnCameraBookmark &bookmark)
{
#ifdef _DEBUG
    /* C++ asserts are required for unit tests to work correctly. Do not replace by NX_ASSERT, it won't matter under define. */
    assert(!bookmark.isNull());
#endif
    QN_LOG_TIME(Q_FUNC_INFO);

    /* Null bookmarks must not be stored in the aggregation. */
    if (bookmark.isNull())
        return false;

    // Searches the place to insert new bookmark (by startTimeMs)
    auto it = std::lower_bound(m_bookmarkList.begin(), m_bookmarkList.end(), bookmark);

    if (it != m_bookmarkList.cend() && it->guid == bookmark.guid)
    {
        if (*it == bookmark)
            return false;

        *it = bookmark;
        return true;
    }

    if (m_bookmarkIds.contains(bookmark.guid))
    {
        const auto predicate = [&bookmark](const QnCameraBookmark &other)
            { return bookmark.guid == other.guid; };

        auto oldIt = std::find_if(m_bookmarkList.begin(), m_bookmarkList.end(), predicate);

        NX_ASSERT(oldIt != m_bookmarkList.end());
        if (oldIt != m_bookmarkList.end())
        {
            if (oldIt < it)
            {
                /* For optimization purposes we suppose the iterator behaves like a pointer.
                    * So if we removed something before 'it' then 'it' should be decreased. */
                --it;
            }
            m_bookmarkList.erase(oldIt);
        }
    }
    else
    {
        m_bookmarkIds.insert(bookmark.guid);
    }

    m_bookmarkList.insert(it, bookmark);
    return true;
}

bool QnCameraBookmarkAggregation::mergeBookmarkList(const QnCameraBookmarkList &bookmarkList)
{
    QN_LOG_TIME(Q_FUNC_INFO);

    if (m_bookmarkList.isEmpty())
    {
        setBookmarkList(bookmarkList);
        return !bookmarkList.empty();
    }

    bool result = false;
    for (const QnCameraBookmark &bookmark: bookmarkList)
        result |= addBookmark(bookmark);

    return result;
}

bool QnCameraBookmarkAggregation::removeBookmark(const QnUuid &bookmarkId)
{
    QN_LOG_TIME(Q_FUNC_INFO);

    if (!m_bookmarkIds.remove(bookmarkId))
        return false;

    auto it = std::find_if(m_bookmarkList.begin(), m_bookmarkList.end(), [&bookmarkId](const QnCameraBookmark &bookmark){
        return bookmark.guid == bookmarkId;
    });

    NX_ASSERT(it != m_bookmarkList.end(), Q_FUNC_INFO, "Bookmark should be present in both id set and list");
    if (it == m_bookmarkList.end())
        return false;

    m_bookmarkList.erase(it);

    return true;
}

void QnCameraBookmarkAggregation::setBookmarkList(const QnCameraBookmarkList &bookmarkList)
{
    QN_LOG_TIME(Q_FUNC_INFO);

    m_bookmarkList = bookmarkList;
    m_bookmarkIds.clear();

    for (const QnCameraBookmark &bookmark: bookmarkList)
        m_bookmarkIds.insert(bookmark.guid);
}

const QnCameraBookmarkList &QnCameraBookmarkAggregation::bookmarkList() const
{
    return m_bookmarkList;
}

void QnCameraBookmarkAggregation::clear()
{
    m_bookmarkList.clear();
    m_bookmarkIds.clear();
}

