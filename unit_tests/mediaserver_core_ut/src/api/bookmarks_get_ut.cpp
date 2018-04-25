#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <core/resource/camera_bookmark.h>

#include "test_api_requests.h"

namespace nx {
namespace test {

TEST(BookmarksGet, noBookmarks)
{
    MediaServerLauncher launcher;
    ASSERT_TRUE(launcher.start());

    nx::vms::api::CameraData cameraData;
    cameraData.id = QnUuid::createUuid();
    cameraData.parentId = QnUuid::createUuid();
    auto resTypePtr = qnResTypePool->getResourceTypeByName("Camera");
    ASSERT_FALSE(resTypePtr.isNull());
    cameraData.typeId = resTypePtr->getId();
    cameraData.vendor = "test vendor";
    cameraData.physicalId = "test physicalId";
    cameraData.id = nx::vms::api::CameraData::physicalIdToId(cameraData.physicalId);

    NX_LOG("[TEST] Create a camera.", cl_logINFO);
    NX_TEST_API_POST(&launcher, "/ec2/saveCamera", cameraData);

    NX_LOG("[TEST] Check that there are no bookmarks for the camera.", cl_logINFO);
    QnCameraBookmarkList bookmarks;
    NX_TEST_API_GET(&launcher,
        lit("/ec2/bookmarks?cameraId=%1").arg(cameraData.id.toString()), &bookmarks);
    ASSERT_TRUE(bookmarks.isEmpty());
}

} // namespace test
} // namespace nx
