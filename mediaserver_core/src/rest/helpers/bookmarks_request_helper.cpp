#include "bookmarks_request_helper.h"

#include <api/helpers/bookmark_request_data.h>
#include "recorder/storage_manager.h"
#include "core/resource/camera_bookmark.h"
#include <core/resource/camera_resource.h>

QnCameraBookmarkList QnBookmarksRequestHelper::load(const QnBookmarkRequestData& request)
{
    MultiServerCameraBookmarkList multiBookmarks;
    for (const auto &camera: request.cameras) {
        QnCameraBookmarkList bookmarks;
        if (!qnStorageMan->getBookmarks(camera->getPhysicalId().toUtf8(), request.filter, bookmarks))
            continue;
        multiBookmarks.push_back(std::move(bookmarks));
    }
    
    return QnCameraBookmark::mergeCameraBookmarks(multiBookmarks, request.filter.limit, request.filter.strategy);
}
