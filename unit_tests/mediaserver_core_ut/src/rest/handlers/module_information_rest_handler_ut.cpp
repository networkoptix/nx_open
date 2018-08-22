#include <gtest/gtest.h>

#include <test_support/mediaserver_launcher.h>

#include <api/mediaserver_client.h>

#include <nx/network/socket_global.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/app_info.h>

namespace test {

class ModuleInformationRestHandlerTest: public ::testing::Test
{
public:
    virtual void SetUp() override
    {
        m_mediaServerLauncher.reset(new MediaServerLauncher());
        const bool started = m_mediaServerLauncher->start();
        ASSERT_TRUE(started);

        const auto url = nx::network::url::Builder()
            .setScheme(nx::network::http::kUrlSchemeName)
            .setEndpoint(m_mediaServerLauncher->endpoint());

        m_mediaServerClient.reset(new MediaServerClient(url));
        m_mediaServerClient->setUserCredentials(
            nx::network::http::Credentials("admin", nx::network::http::PasswordAuthToken("admin")));
    }

    virtual void TearDown() override
    {
        m_mediaServerClient.reset();
        m_mediaServerLauncher.reset();
    }

    MediaServerClient* client() const
    {
        return m_mediaServerClient.get();
    }

private:
    std::unique_ptr<MediaServerLauncher> m_mediaServerLauncher;
    std::unique_ptr<MediaServerClient> m_mediaServerClient;
};

TEST_F(ModuleInformationRestHandlerTest, checkModuleInformation)
{
    const auto validCloudHost = nx::network::SocketGlobals::cloud().cloudHost();
    const auto validRealm = nx::network::AppInfo::realm();
    ASSERT_NE(client(), nullptr);

    nx::vms::api::ModuleInformation result;
    ASSERT_EQ(result.cloudHost, QString());

    client()->getModuleInformation(&result);
    ASSERT_EQ(result.cloudHost, validCloudHost);
    ASSERT_EQ(result.realm, validRealm);
}

} // namespace test
