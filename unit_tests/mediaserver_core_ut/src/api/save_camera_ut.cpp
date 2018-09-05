#include <gtest/gtest.h>

#include <nx/utils/log/log.h>

#include "test_api_requests.h"

namespace nx {
namespace test {

TEST(SaveCamera, invalidData)
{
    MediaServerLauncher launcher;
    ASSERT_TRUE(launcher.start());

    nx::vms::api::CameraData cameraData;
    cameraData.parentId = QnUuid::createUuid();
    auto resTypePtr = qnResTypePool->getResourceTypeByName("Camera");
    ASSERT_FALSE(resTypePtr.isNull());
    cameraData.typeId = resTypePtr->getId();
    cameraData.vendor = "test vendor";
    cameraData.physicalId = "matching physicalId";
    cameraData.id = nx::vms::api::CameraData::physicalIdToId(cameraData.physicalId);

    NX_INFO(this, "[TEST] Both id and physicalId fields correctly defined.");
    NX_TEST_API_POST(&launcher, "/ec2/saveCamera", cameraData);

    NX_INFO(this, "[TEST] Error case: id doesn't match physicalId.");
    cameraData.physicalId = "non-matching physicalId";
    NX_TEST_API_POST(&launcher, "/ec2/saveCamera", cameraData,
        keepOnlyJsonFields({"id", "physicalId", "parentId", "typeId", "vendor"}),
        nx::network::http::StatusCode::forbidden);

    NX_INFO(this, "[TEST] Create another camera with auto-generated id.");
    NX_TEST_API_POST(&launcher, "/ec2/saveCamera", cameraData,
        keepOnlyJsonFields({/*"id",*/ "physicalId", "parentId", "typeId", "vendor"}));

    NX_INFO(this, "[TEST] Merge by id with the existing object.");
    NX_TEST_API_POST(&launcher, "/ec2/saveCamera", cameraData,
        keepOnlyJsonFields({"id", /*"physicalId",*/ "parentId", "typeId", "vendor"}));

    NX_INFO(this, "[TEST] Error case: both id and physicalId are missing.");
    NX_TEST_API_POST(&launcher, "/ec2/saveCamera", cameraData,
        keepOnlyJsonFields({/*"id",*/ /*"physicalId",*/ "parentId", "typeId", "vendor"}),
        nx::network::http::StatusCode::forbidden);
}

} // namespace test
} // namespace nx
