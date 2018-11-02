#include <memory>
#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <nx/vms/api/data/resource_data.h>
#include <rest/server/json_rest_result.h>

#include "test_api_requests.h"

namespace nx::test {

class CameraAuth: public ::testing::Test
{
protected:
    using LauncherPtr = std::unique_ptr<MediaServerLauncher>;

    LauncherPtr givenServer()
    {
        LauncherPtr result = std::unique_ptr<MediaServerLauncher>(new MediaServerLauncher());
        result->addSetting(QnServer::kNoInitStoragesOnStartup, "1");
        return result;
    }

    void whenServerLaunched(const LauncherPtr& server)
    {
        ASSERT_TRUE(server->start());
    }

private:
    LauncherPtr m_server;
};

TEST_F(CameraAuth, SetGetViaNetworkResource)
{
    whenServerLaunched();
    whenCameraSaved();
    whenAuthSetViaResource();
    whenAuthRequestedViaResource();
    thenItShouldBeOk();
}

TEST_F(CameraAuth, SetViaAPIGetViaNetworkResource)
{
}

TEST_F(CameraAuth, SetGetViaAPI)
{
}

} // namespace nx::test
