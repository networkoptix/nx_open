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

    NX_INFO(this, "Create a camera.");
    NX_TEST_API_POST(&launcher, "/ec2/saveCamera", cameraData);

    NX_INFO(this, "Check that there are no bookmarks for the camera.");
    QnCameraBookmarkList bookmarks;
    NX_TEST_API_GET(&launcher,
        lit("/ec2/bookmarks?cameraId=%1").arg(cameraData.id.toString()), &bookmarks);
    ASSERT_TRUE(bookmarks.isEmpty());
}

} // namespace test
} // namespace nx
