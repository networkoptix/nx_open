#include <gtest/gtest.h>

#include "cloud/mediaserver_cloud_integration_test_setup.h"

class FtConfiguringNewSystem:
    public MediaServerCloudIntegrationTest,
    public ::testing::Test
{
protected:
    void givenSystemCredentialsRegisteredInCloud()
    {
        ASSERT_TRUE(registerRandomCloudAccount());
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
};

TEST_F(FtConfiguringNewSystem, cloud_system)
{
    givenSystemCredentialsRegisteredInCloud();
    whenConfiguredMediaServerAsCloudSystem();
    thenSystemMustBeAccessibleWithCloudOwnerCredentials();
}
