#include <gtest/gtest.h>

#include <test_support/mediaserver_launcher.h>
#include <api/test_api_requests.h>
#include <core/resource/camera_bookmark.h>
#include <nx/streaming/rtsp_client.h>
#include <nx/network/url/url_builder.h>

using namespace nx::test;
using namespace std::chrono;

namespace nx::vms::server::api::test {

TEST(CameraExList, defaultValues)
{
    auto mediaServerLauncher = std::make_unique<MediaServerLauncher>();
    ASSERT_TRUE(mediaServerLauncher->start());

    // Add camera.
    using namespace nx::vms::api;
    CameraDataList cameras;
    CameraData cameraData;
    cameraData.parentId = QnUuid::createUuid();
    cameraData.typeId = qnResTypePool->getResourceTypeByName("Camera")->getId();
    cameraData.vendor = "test vendor";
    cameraData.physicalId = "Some physicalId";
    cameraData.id = nx::vms::api::CameraData::physicalIdToId(cameraData.physicalId);
    cameras.push_back(cameraData);

    auto connection = mediaServerLauncher->commonModule()->ec2Connection();
    auto cameraManager = connection->getCameraManager(Qn::kSystemAccess);
    auto dbError = cameraManager->addCamerasSync(cameras);
    ASSERT_EQ(ec2::ErrorCode::ok, dbError);

    CameraDataExList resultCameras;
    NX_TEST_API_GET(mediaServerLauncher.get(), "/ec2/getCamerasEx", &resultCameras);

    ASSERT_EQ(1, resultCameras.size());
    ASSERT_EQ(CameraAttributesData(), (CameraAttributesData) resultCameras[0]);
    
}
} // nx::vms::server::api::test
