
#include "bookmark_helpers.h"

#include <utils/math/math.h>
#include <core/resource/camera_bookmark.h>

QnCameraBookmarkList helpers::bookmarksAtPosition(const QnCameraBookmarkList &bookmarks
                                         , qint64 posMs)
{
    const auto predicate = [posMs](const QnCameraBookmark &bookmark)
    {
        return qBetween(bookmark.startTimeMs, posMs, bookmark.endTimeMs());
    };

    QnCameraBookmarkList result;
    std::copy_if(bookmarks.cbegin(), bookmarks.cend(), std::back_inserter(result), predicate);
    return result;
};
