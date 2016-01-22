
#include "bookmark_helpers.h"

#include <core/resource/camera_bookmark.h>

QnCameraBookmarkList helpers::bookmarksAtPosition(const QnCameraBookmarkList &bookmarks
                                         , qint64 posMs)
{
    const auto perdicate = [posMs](const QnCameraBookmark &bookmark)
    {
        return (bookmark.startTimeMs <= posMs) && (posMs < bookmark.endTimeMs());
    };

    QnCameraBookmarkList result;
    std::copy_if(bookmarks.cbegin(), bookmarks.cend(), std::back_inserter(result), perdicate);
    return result;
};
