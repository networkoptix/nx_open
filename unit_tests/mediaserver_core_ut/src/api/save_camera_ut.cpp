#include <QJsonDocument>
#include <QFile>
#include <QRegExp>
#include <vector>

#include <gtest/gtest.h>
#include "mediaserver_launcher.h"
#include <api/app_server_connection.h>
#include <nx/network/http/httpclient.h>
#include <nx/fusion/model_functions.h>
#include <rest/server/json_rest_result.h>

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

    auto ec2Connection = QnAppServerConnectionFactory::getConnection2();
    auto userManager = ec2Connection->getCameraManager(Qn::kSystemAccess);

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
