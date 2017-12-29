#include <gtest/gtest.h>

#include <nx/utils/log/log.h>

#include "test_api_requests.h"

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

    NX_LOG("[TEST] Both id and physicalId fields correctly defined.", cl_logINFO);
    NX_TEST_API_POST(&launcher, "/ec2/saveCamera", cameraData);

    NX_LOG("[TEST] Error case: id doesn't match physicalId.", cl_logINFO);
    cameraData.physicalId = "non-matching physicalId";
    NX_TEST_API_POST(&launcher, "/ec2/saveCamera", cameraData,
        keepOnlyJsonFields({"id", "physicalId", "parentId", "typeId", "vendor"}),
        nx::network::http::StatusCode::forbidden);

    NX_LOG("[TEST] Create another camera with auto-generated id.", cl_logINFO);
    NX_TEST_API_POST(&launcher, "/ec2/saveCamera", cameraData,
        keepOnlyJsonFields({/*"id",*/ "physicalId", "parentId", "typeId", "vendor"}));

    NX_LOG("[TEST] Merge by id with the existing object.", cl_logINFO);
    NX_TEST_API_POST(&launcher, "/ec2/saveCamera", cameraData,
        keepOnlyJsonFields({"id", /*"physicalId",*/ "parentId", "typeId", "vendor"}));

    NX_LOG("[TEST] Error case: both id and physicalId are missing.", cl_logINFO);
    NX_TEST_API_POST(&launcher, "/ec2/saveCamera", cameraData,
        keepOnlyJsonFields({/*"id",*/ /*"physicalId",*/ "parentId", "typeId", "vendor"}),
        nx::network::http::StatusCode::forbidden);
}

} // namespace test
} // namespace nx
