#include <gtest/gtest.h>

#include "mediaserver_cloud_integration_test_setup.h"

class FtConfiguringNewSystem:
    public MediaServerCloudIntegrationTest
{
public:
    FtConfiguringNewSystem()
    {
        init();
    }

protected:
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
            client.setupCloudSystem(std::move(request)).error);
    }

    void thenSystemMustBeAccessibleWithCloudOwnerCredentials()
    {
        MediaServerClient client(mediaServerEndpoint());
        client.setUserName(QString::fromStdString(accountEmail()));
        client.setPassword(QString::fromStdString(accountPassword()));
        ec2::ApiResourceParamDataList vmsSettings;
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
        MediaServerClient client(mediaServerEndpoint());
        client.setUserName(QString::fromStdString(accountEmail()));
        client.setPassword(QString::fromStdString(accountPassword()));

        ASSERT_EQ(
            QnJsonRestResult::NoError,
            client.detachFromCloud(DetachFromCloudData()).error);
    }

    void assertSystemAcceptsDefaultCredentials()
    {
        MediaServerClient client(mediaServerEndpoint());
        client.setUserName("admin");
        client.setPassword("admin");

        ec2::ApiResourceParamDataList vmsSettings;
        ASSERT_EQ(
            ec2::ErrorCode::ok,
            client.ec2GetSettings(&vmsSettings));
    }

    void assertIfSystemHasNotBecameNew()
    {
        MediaServerClient client(mediaServerEndpoint());
        client.setUserName("admin");
        client.setPassword("admin");

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

private:
    void init()
    {
        ASSERT_TRUE(registerRandomCloudAccount());
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
