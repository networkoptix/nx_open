#include "bookmarks_request_helper.h"

#include <api/helpers/bookmark_request_data.h>

#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>

#include <database/server_db.h>

QnCameraBookmarkList QnBookmarksRequestHelper::loadBookmarks(const QnGetBookmarksRequestData& request)
{
    QnMultiServerCameraBookmarkList multiBookmarks;
    for (const auto &camera: request.cameras) {
        QnCameraBookmarkList bookmarks;
        if (!qnServerDb->getBookmarks(camera->getUniqueId(), request.filter, bookmarks))
            continue;
        multiBookmarks.push_back(std::move(bookmarks));
    }

    return QnCameraBookmark::mergeCameraBookmarks(multiBookmarks, request.filter.limit, request.filter.strategy);
}

QnCameraBookmarkTagList QnBookmarksRequestHelper::loadTags(const QnGetBookmarkTagsRequestData &request) {
    return qnServerDb->getBookmarkTags(request.limit);
}
