#include <gtest/gtest.h>

#include <nx/utils/string.h>

#include <api/global_settings.h>
#include <core/resource/user_resource.h>
#include <nx/utils/sync_call.h>

#include <test_support/mediaserver_launcher.h>
#include "mediaserver_cloud_integration_test_setup.h"

namespace {

static constexpr auto kRetryRequestDelay = std::chrono::seconds(1);

} // namespace

class FtConnectSystemToCloud:
    public MediaServerCloudIntegrationTest,
    public ::testing::Test
{
protected:
    void whenSetCloudSystemIdToEmptyString()
    {
        auto mediaServerClient = prepareMediaServerClient();
        
        ec2::ApiResourceParamWithRefDataList params;
        params.resize(1);
        params.back().resourceId = QnUserResource::kAdminGuid;
        params.back().name = nx::settings_names::kNameCloudSystemId;
        params.back().value = QString();

        ec2::ErrorCode resultCode = ec2::ErrorCode::ok;
        std::tie(resultCode) =
            makeSyncCall<ec2::ErrorCode>(
                std::bind(
                    &MediaServerClient::ec2SetResourceParams,
                    &mediaServerClient,
                    std::move(params),
                    std::placeholders::_1));
        ASSERT_EQ(ec2::ErrorCode::ok, resultCode);

        switchToDefaultCredentials();
    }

    void thenSystemStateShouldBecomeNew()
    {
        auto mediaServerClient = prepareMediaServerClient();
        for (;;)
        {
            QnModuleInformation moduleInformation;
            QnJsonRestResult resultCode = 
                mediaServerClient.getModuleInformation(&moduleInformation);
            ASSERT_EQ(QnJsonRestResult::NoError, resultCode.error);

            if (moduleInformation.serverFlags.testFlag(Qn::ServerFlag::SF_NewSystem))
                break;
            std::this_thread::sleep_for(kRetryRequestDelay);
        }
    }

    void thenCloudUsersShouldBeRemoved()
    {
        auto mediaServerClient = prepareMediaServerClient();
        for (;;)
        {
            ec2::ApiUserDataList users;
            if (mediaServerClient.ec2GetUsers(&users) == ec2::ErrorCode::ok)
            {
                bool foundCloudUser = false;
                for (const auto& user: users)
                    foundCloudUser |= user.isCloud;

                if (!foundCloudUser)
                    break;
            }
            std::this_thread::sleep_for(kRetryRequestDelay);
        }
    }
    
    void thenCloudAttributesShouldBeRemoved()
    {
        auto mediaServerClient = prepareMediaServerClient();
        for (;;)
        {
            ec2::ApiResourceParamDataList vmsSettings;
            ec2::ErrorCode resultCode = mediaServerClient.ec2GetSettings(&vmsSettings);
            ASSERT_EQ(ec2::ErrorCode::ok, resultCode);

            const auto cloudSettingsAreEmpty = 
                getValueByName(vmsSettings, nx::settings_names::kNameCloudAccountName).isEmpty() &&
                getValueByName(vmsSettings, nx::settings_names::kNameCloudSystemId).isEmpty() &&
                getValueByName(vmsSettings, nx::settings_names::kNameCloudAuthKey).isEmpty();

            if (cloudSettingsAreEmpty)
                break;
            std::this_thread::sleep_for(kRetryRequestDelay);
        }
    }

private:
    QString getValueByName(const ec2::ApiResourceParamDataList& values, const QString& name)
    {
        for (const auto& nameAndValue: values)
        {
            if (nameAndValue.name == name)
                return nameAndValue.value;
        }

        return QString();
    }
};

//-------------------------------------------------------------------------------------------------
// FtConnectSystemToCloudOnlyCloudOwner

class FtConnectSystemToCloudOnlyCloudOwner:
    public FtConnectSystemToCloud
{
protected:
    void givenServerConnectedToTheCloud()
    {
        connectSystemToCloud();
    }
};

// TODO: #ak CLOUD-734
TEST_F(FtConnectSystemToCloudOnlyCloudOwner, DISABLED_removing_cloud_system_id_resets_system_to_new_state)
{
    givenServerConnectedToTheCloud();

    whenSetCloudSystemIdToEmptyString();

    thenCloudUsersShouldBeRemoved();
    thenCloudAttributesShouldBeRemoved();
    thenSystemStateShouldBecomeNew();
}

//-------------------------------------------------------------------------------------------------
// FtConnectSystemToCloudWithLocalOwner

class FtConnectSystemToCloudWithLocalOwner:
    public FtConnectSystemToCloud
{
protected:
    void givenServerConnectedToTheCloud()
    {
        configureSystemAsLocal();
        connectSystemToCloud();
    }
};

// TODO: #ak CLOUD-734
TEST_F(FtConnectSystemToCloudWithLocalOwner, DISABLED_removing_cloud_system_id_cleans_up_cloud_data)
{
    givenServerConnectedToTheCloud();

    whenSetCloudSystemIdToEmptyString();

    thenCloudUsersShouldBeRemoved();
    thenCloudAttributesShouldBeRemoved();
}

//-------------------------------------------------------------------------------------------------
// FtDisconnectSystemFromCloud

class FtDisconnectSystemFromCloud:
    public FtConnectSystemToCloud
{
protected:
    void givenServerWithLocalAdminConnectedToTheCloud()
    {
        configureSystemAsLocal();
        connectSystemToCloud();
    }

    void givenServerConnectedToTheCloudWithCloudOwnerOnly()
    {
        connectSystemToCloud();
    }

    void whenInvokedDetachFromCloudRestMethod()
    {
        auto mediaServerClient = prepareMediaServerClient();
        DetachFromCloudData params;
        QnJsonRestResult resultCode = mediaServerClient.detachFromCloud(std::move(params));
        ASSERT_EQ(QnJsonRestResult::NoError, resultCode.error);
    }

    void whenUsingDefaultCredentials()
    {
        switchToDefaultCredentials();
    }
};

TEST_F(FtDisconnectSystemFromCloud, disconnect_by_mserver_api_call_local_admin_present)
{
    givenServerWithLocalAdminConnectedToTheCloud();

    whenInvokedDetachFromCloudRestMethod();

    thenCloudUsersShouldBeRemoved();
    thenCloudAttributesShouldBeRemoved();
}

TEST_F(FtDisconnectSystemFromCloud, disconnect_by_mserver_api_call_cloud_owner_only)
{
    givenServerConnectedToTheCloudWithCloudOwnerOnly();

    whenInvokedDetachFromCloudRestMethod();
    whenUsingDefaultCredentials();

    thenCloudUsersShouldBeRemoved();
    thenCloudAttributesShouldBeRemoved();
    thenSystemStateShouldBecomeNew();
}
