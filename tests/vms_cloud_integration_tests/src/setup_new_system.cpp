#include <memory>

#include <gtest/gtest.h>

#include <nx/network/url/url_builder.h>

#include "mediaserver_cloud_integration_test_setup.h"

class FtConfiguringNewSystem:
    public MediaServerCloudIntegrationTest
{
    using base_type = MediaServerCloudIntegrationTest;

protected:
    void SetUp() override
    {
        base_type::SetUp();

        ASSERT_TRUE(registerRandomCloudAccount());
    }

    void givenSystemCredentialsRegisteredInCloud()
    {
        ASSERT_TRUE(registerCloudSystem());
    }

    void whenConfiguredMediaServerAsCloudSystem()
    {
        auto client = prepareMediaServerClient();
        SetupCloudSystemData request;
        request.systemName = QString::fromStdString(cloudSystem().name);
        request.cloudSystemID = QString::fromStdString(cloudSystem().id);
        request.cloudAuthKey = QString::fromStdString(cloudSystem().authKey);
        request.cloudAccountName = QString::fromStdString(accountEmail());

        ASSERT_EQ(
            QnJsonRestResult::NoError,
            client->setupCloudSystem(std::move(request)).error);
    }

    void thenSystemMustBeAccessibleWithCloudOwnerCredentials()
    {
        MediaServerClient client(
            nx::network::url::Builder().setScheme(nx::network::http::kUrlSchemeName)
                .setEndpoint(mediaServerEndpoint()));

        client.setUserCredentials(nx::network::http::Credentials(
            accountEmail().c_str(),
            nx::network::http::PasswordAuthToken(accountPassword().c_str())));
        nx::vms::api::ResourceParamDataList vmsSettings;
        ASSERT_EQ(ec2::ErrorCode::ok, client.ec2GetSettings(&vmsSettings));
    }

    void configureCloudSystem()
    {
        givenSystemCredentialsRegisteredInCloud();
        whenConfiguredMediaServerAsCloudSystem();
        thenSystemMustBeAccessibleWithCloudOwnerCredentials();
    }

    void detachSystemFromCloud()
    {
        MediaServerClient client(
            nx::network::url::Builder().setScheme(nx::network::http::kUrlSchemeName)
                .setEndpoint(mediaServerEndpoint()));
        client.setUserCredentials(nx::network::http::Credentials(
            accountEmail().c_str(),
            nx::network::http::PasswordAuthToken(accountPassword().c_str())));

        ASSERT_EQ(
            QnJsonRestResult::NoError,
            client.detachFromCloud(DetachFromCloudData()).error);
    }

    void assertSystemAcceptsDefaultCredentials()
    {
        MediaServerClient client(
            nx::network::url::Builder().setScheme(nx::network::http::kUrlSchemeName)
                .setEndpoint(mediaServerEndpoint()));
        client.setUserCredentials(nx::network::http::Credentials(
            "admin",
            nx::network::http::PasswordAuthToken("admin")));

        nx::vms::api::ResourceParamDataList vmsSettings;
        ASSERT_EQ(
            ec2::ErrorCode::ok,
            client.ec2GetSettings(&vmsSettings));
    }

    void assertIfSystemHasNotBecameNew()
    {
        MediaServerClient client(
            nx::network::url::Builder().setScheme(nx::network::http::kUrlSchemeName)
                .setEndpoint(mediaServerEndpoint()));
        client.setUserCredentials(nx::network::http::Credentials(
            "admin",
            nx::network::http::PasswordAuthToken("admin")));

        for (;;)
        {
            QnModuleInformation moduleInformation;
            ASSERT_EQ(
                QnJsonRestResult::NoError,
                client.getModuleInformation(&moduleInformation).error);
            if (moduleInformation.serverFlags.testFlag(Qn::ServerFlag::SF_NewSystem))
                break;
        }
    }
};

TEST_P(FtConfiguringNewSystem, DISABLED_cloud_system)
{
    givenSystemCredentialsRegisteredInCloud();
    whenConfiguredMediaServerAsCloudSystem();
    thenSystemMustBeAccessibleWithCloudOwnerCredentials();
}

TEST_P(FtConfiguringNewSystem, DISABLED_reconfiguring_cloud_system)
{
    configureCloudSystem();
    detachSystemFromCloud();
    assertSystemAcceptsDefaultCredentials();
    configureCloudSystem();
}

INSTANTIATE_TEST_CASE_P(P2pMode, FtConfiguringNewSystem,
    ::testing::Values(TestParams(false), TestParams(true)
));
