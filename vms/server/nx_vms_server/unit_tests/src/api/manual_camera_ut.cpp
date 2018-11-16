#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <api/model/manual_camera_data.h>

#include "test_api_requests.h"

namespace nx {
namespace test {

TEST(ManualCamera, add)
{
    MediaServerLauncher launcher;
    launcher.addSetting(QnServer::kNoInitStoragesOnStartup, "1");
    ASSERT_TRUE(launcher.start());

    ManualCameraData camera;
    camera.url = "test url";
    camera.uniqueId = "test unique id";
    camera.manufacturer = "ONVIF"; //< Should be present in Resource Pool.
    AddManualCameraData manualCameras;
    manualCameras.user = "test user";
    manualCameras.password = "test password";
    manualCameras.cameras.append(camera);

    NX_INFO(this, "Add a camera via /api/manualCamera/add.");
    NX_TEST_API_POST(&launcher, lit("/api/manualCamera/add"), manualCameras);

    // NOTE: We cannot easily test that the camera has been added here, because it is only
    // registered within Discovery Manager to be checked and added later asynchronously.
}

} // namespace test
} // namespace nx
