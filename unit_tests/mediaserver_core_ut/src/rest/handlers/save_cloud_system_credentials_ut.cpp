#include <gtest/gtest.h>

#include <api/global_settings.h>
#include <api/model/cloud_credentials_data.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <network/auth/time_based_nonce_provider.h>

#include <cloud/cloud_manager_group.h>
#include <media_server/settings.h>
#include <rest/handlers/save_cloud_system_credentials.h>
#include <settings.h>

namespace test {

class MediaServerRestHandlerTestBase
{
public:
    MediaServerRestHandlerTestBase()
    {
        MSSettings::initializeROSettings();
        MSSettings::initializeRunTimeSettings();

        qnCommon->setModuleGUID(QnUuid::createUuid());

        insertSelfServerResource();
        insertAdminUser();
    }

    ~MediaServerRestHandlerTestBase()
    {
    }

private:
    QnCommonModule m_commonModule;
    ec2::Settings m_ec2Settings;
    QnGlobalSettings m_globalSettings;

    void insertSelfServerResource()
    {
        auto selfServer = QnMediaServerResourcePtr(new QnMediaServerResource());
        selfServer->setId(qnCommon->moduleGUID());
        selfServer->setServerFlags(Qn::SF_HasPublicIP);
        resourcePool()->addResource(selfServer);
    }

    void insertAdminUser()
    {
        auto admin = QnUserResourcePtr(new QnUserResource(QnUserType::Local));
        admin->setId(QnUserResource::kAdminGuid);
        admin->setName("admin");
        admin->setPassword("admin");
        admin->setOwner(true);
        resourcePool()->addResource(admin);
    }
};

class QnSaveCloudSystemCredentialsHandler:
    public MediaServerRestHandlerTestBase,
    public ::testing::Test
{
public:
    QnSaveCloudSystemCredentialsHandler():
        m_cloudManagerGroup(&m_timeBasedNonceProvider),
        m_restHandler(&m_cloudManagerGroup)
    {
    }

    ~QnSaveCloudSystemCredentialsHandler()
    {
    }

protected:
    void whenInvokedHandler()
    {
        CloudCredentialsData input;
        input.cloudSystemID = QnUuid::createUuid().toSimpleString();
        input.cloudAuthKey = QnUuid::createUuid().toSimpleString();
        input.cloudAccountName = "test@nx.com";

        QnJsonRestResult result;
        ASSERT_EQ(
            nx_http::StatusCode::ok,
            m_restHandler.execute(input, result, nullptr));
    }

    void assertIfCloudCredentialsWereNotSaved()
    {
        // TODO: #ak
    }

    void assertIfNotEveryCloudManagerHasBeenInitiailized()
    {
        // TODO: #ak
    }

private:
    TimeBasedNonceProvider m_timeBasedNonceProvider;
    CloudManagerGroup m_cloudManagerGroup;
    ::QnSaveCloudSystemCredentialsHandler m_restHandler;
};

// TODO: #ak Making these tests work require some refactoring in mediaserver, so leaving it until 3.1.

TEST_F(QnSaveCloudSystemCredentialsHandler, DISABLED_cloud_credentials_are_saved)
{
    whenInvokedHandler();
    assertIfCloudCredentialsWereNotSaved();
}

TEST_F(QnSaveCloudSystemCredentialsHandler, DISABLED_cloud_managers_are_initialized)
{
    whenInvokedHandler();
    assertIfNotEveryCloudManagerHasBeenInitiailized();
}

} // namespace test
