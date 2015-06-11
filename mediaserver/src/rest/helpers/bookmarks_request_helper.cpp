#include "bookmarks_request_helper.h"

#include <api/helpers/bookmark_request_data.h>
#include "recorder/storage_manager.h"
#include "core/resource/camera_bookmark.h"
#include <core/resource/camera_resource.h>
#include <core/resource/camera_bookmark.h>
#include "motion/motion_helper.h"
#include <utils/serialization/json.h>
#include <utils/serialization/json_functions.h>
#include "core/resource_management/resource_pool.h"

QnCameraBookmarkList QnBookmarksRequestHelper::load(const QnBookmarkRequestData& request)
{
    MultiServerCameraBookmarkList multiBookmarks;
    for (const auto &camera: request.cameras) {
        QnCameraBookmarkList bookmarks;
        if (!qnStorageMan->getBookmarks(camera->getPhysicalId().toUtf8(), request.filter, bookmarks))
            continue;
        multiBookmarks.push_back(std::move(bookmarks));
    }
    
    //TODO: #GDM #Bookmarks muticameras request, merge,  limit

    return multiBookmarks.empty()
        ? QnCameraBookmarkList()
        : multiBookmarks.front();
}
