#include <gtest/gtest.h>

#include <core/resource/camera_bookmark.h>

#include "test_api_requests.h"

namespace nx {
namespace test {

TEST(BookmarksGet, noBookmarks)
{
    MediaServerLauncher launcher;
    ASSERT_TRUE(launcher.start());

    ec2::ApiCameraData cameraData;
    cameraData.id = QnUuid::createUuid();
    cameraData.parentId = QnUuid::createUuid();
    auto resTypePtr = qnResTypePool->getResourceTypeByName("Camera");
    ASSERT_FALSE(resTypePtr.isNull());
    cameraData.typeId = resTypePtr->getId();
    cameraData.vendor = "test vendor";
    cameraData.physicalId = "test physicalId";
    cameraData.id = ec2::ApiCameraData::physicalIdToId(cameraData.physicalId);

    NX_LOG("[TEST] Create a camera.", cl_logINFO);
    ASSERT_NO_FATAL_FAILURE(testApiPost(launcher, "/ec2/saveCamera", cameraData));

    NX_LOG("[TEST] Check that there are no bookmarks for the camera.", cl_logINFO);
    QnCameraBookmarkList bookmarks;
    ASSERT_NO_FATAL_FAILURE(testApiGet(launcher,
        lit("/ec2/bookmarks?cameraId=%1").arg(cameraData.id.toString()), &bookmarks));
    ASSERT_TRUE(bookmarks.isEmpty());
}

} // namespace test
} // namespace nx
