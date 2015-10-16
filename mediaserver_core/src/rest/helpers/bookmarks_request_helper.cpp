#include "bookmarks_request_helper.h"

#include <api/helpers/bookmark_requests.h>

#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>

#include <database/server_db.h>

QnCameraBookmarkList QnBookmarksRequestHelper::load(const QnGetBookmarksRequestData& request)
{
    MultiServerCameraBookmarkList multiBookmarks;
    for (const auto &camera: request.cameras) {
        QnCameraBookmarkList bookmarks;        
        if (!qnServerDb->getBookmarks(camera->getUniqueId(), request.filter, bookmarks))
            continue;
        multiBookmarks.push_back(std::move(bookmarks));
    }
    
    return QnCameraBookmark::mergeCameraBookmarks(multiBookmarks, request.filter.limit, request.filter.strategy);
}
