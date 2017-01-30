#include <gtest/gtest.h>

#include "test_api_request.h"

namespace nx {
namespace test {

TEST(SaveCamera, invalidData)
{
    MediaServerLauncher launcher;
    ASSERT_TRUE(launcher.start());

    ec2::ApiCameraData cameraData;
    cameraData.parentId = QnUuid::createUuid();
    auto resTypePtr = qnResTypePool->getResourceTypeByName("Camera");
    ASSERT_FALSE(resTypePtr.isNull());
    cameraData.typeId = resTypePtr->getId();
    cameraData.vendor = "test vendor";
    cameraData.physicalId = "matching physicalId";
    cameraData.id = ec2::ApiCameraData::physicalIdToId(cameraData.physicalId);

    // Both id and physicalId fields correctly defined.
    testApiPost(launcher, "/ec2/saveCamera", cameraData);

    // Error: id doesn't match physicalId.
    cameraData.physicalId = "non-matching physicalId";
    testApiPost(launcher, "/ec2/saveCamera", cameraData,
        keepOnlyJsonFields({"id", "physicalId", "parentId", "typeId", "vendor"}),
        nx_http::StatusCode::forbidden);

    // Create another camera with auto-generated id.
    testApiPost(launcher, "/ec2/saveCamera", cameraData,
        keepOnlyJsonFields({/*"id",*/ "physicalId", "parentId", "typeId", "vendor"}));

    // Merged by id with the existing object.
    testApiPost(launcher, "/ec2/saveCamera", cameraData,
        keepOnlyJsonFields({"id", /*"physicalId",*/ "parentId", "typeId", "vendor"}));

    // Error: both id and physicalId are missing.
    testApiPost(launcher, "/ec2/saveCamera", cameraData,
        keepOnlyJsonFields({/*"id",*/ /*"physicalId",*/ "parentId", "typeId", "vendor"}),
        nx_http::StatusCode::forbidden);
}

} // namespace test
} // namespace nx
