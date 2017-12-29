#include <gtest/gtest.h>

#include <nx/core/access/access_types.h>
#include <nx/vms/cloud_integration/cloud_manager_group.h>

#include <api/global_settings.h>
#include <api/model/cloud_credentials_data.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <network/auth/generic_user_data_provider.h>
#include <network/auth/time_based_nonce_provider.h>
#include <rest/handlers/save_cloud_system_credentials.h>
#include <settings.h>

#include <media_server/settings.h>
#include <cloud/cloud_integration_manager.h>

namespace test {

class MediaServerRestHandlerTestBase
{
public:
    MediaServerRestHandlerTestBase():
        m_commonModule(false, nx::core::access::Mode::direct)
    {
        m_commonModule.setModuleGUID(QnUuid::createUuid());

        insertSelfServerResource();
        insertAdminUser();
    }

    ~MediaServerRestHandlerTestBase()
    {
    }

protected:
    QnCommonModule m_commonModule;

private:
    ec2::Settings m_ec2Settings;

    void insertSelfServerResource()
    {
        auto selfServer = QnMediaServerResourcePtr(new QnMediaServerResource(&m_commonModule));
        selfServer->setId(m_commonModule.moduleGUID());
        selfServer->setServerFlags(Qn::SF_HasPublicIP);
        m_commonModule.resourcePool()->addResource(selfServer);
    }

    void insertAdminUser()
    {
        auto admin = QnUserResourcePtr(new QnUserResource(QnUserType::Local));
        admin->setId(QnUserResource::kAdminGuid);
        admin->setName("admin");
        admin->setPasswordAndGenerateHash("admin");
        admin->setOwner(true);
        m_commonModule.resourcePool()->addResource(admin);
    }
};

class QnSaveCloudSystemCredentialsHandler:
    public MediaServerRestHandlerTestBase,
    public ::testing::Test
{
public:
    QnSaveCloudSystemCredentialsHandler():
        m_cloudIntegrationManager(
            &m_commonModule,
            nullptr,
            &m_timeBasedNonceProvider),
        m_restHandler(&m_cloudIntegrationManager.cloudManagerGroup())
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
            nx::network::http::StatusCode::ok,
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
    CloudIntegrationManager m_cloudIntegrationManager;
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
