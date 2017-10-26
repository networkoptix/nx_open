#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <nx/network/cloud/abstract_cloud_system_credentials_provider.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/test_support/test_options.h>

#include <nx/cloud/cdb/test_support/cdb_launcher.h>

#include <test_support/peer_wrapper.h>
#include <test_support/merge_test_fixture.h>

namespace test {

namespace {
static const QnUuid kUserResourceTypeGuid("{774e6ecd-ffc6-ae88-0165-8f4a6d0eafa7}");
} // namespace

class CloudMerge:
    public ::testing::Test
{
public:
    static void SetUpTestCase()
    {
        s_staticCommonModule = 
            std::make_unique<QnStaticCommonModule>(Qn::PeerType::PT_Server);
    }

    static void TearDownTestCase()
    {
        s_staticCommonModule.reset();
    }

protected:
    void givenTwoCloudSystemsWithTheSameOwner()
    {
        ASSERT_TRUE(m_systemMergeFixture.initializeSingleServerSystems(2));
       
        for (int i = 0; i < m_systemMergeFixture.peerCount(); ++i)
        {
            ASSERT_TRUE(connectToCloud(m_systemMergeFixture.peer(i)));
            m_systemCloudCredentials.push_back(
                m_systemMergeFixture.peer(i).getCloudCredentials());
        }
    }

    void addRandomCloudUserToEachSystem()
    {
        for (int i = 0; i < m_systemMergeFixture.peerCount(); ++i)
        {
            const auto someCloudUser = m_cdb.addActivatedAccount2();
            ::ec2::ApiUserData vmsUserData;

            vmsUserData.id = guidFromArbitraryData(someCloudUser.email);
            vmsUserData.typeId = kUserResourceTypeGuid;
            vmsUserData.email = someCloudUser.email.c_str();
            vmsUserData.name = someCloudUser.email.c_str();
            vmsUserData.fullName = someCloudUser.fullName.c_str();
            vmsUserData.isCloud = true;
            vmsUserData.isEnabled = true;
            vmsUserData.realm = nx::network::AppInfo::realm();
            vmsUserData.hash = "password_is_in_cloud";
            vmsUserData.digest = "password_is_in_cloud";
            vmsUserData.permissions = Qn::GlobalAdminPermissionSet;

            auto mediaServerClient = m_systemMergeFixture.peer(i).mediaServerClient();
            ASSERT_EQ(::ec2::ErrorCode::ok, mediaServerClient->ec2SaveUser(vmsUserData));
        }
    }

    void whenMergeSystems()
    {
        m_systemMergeFixture.whenMergeSystems();
    }

    void thenSystemsMerged()
    {
        ASSERT_EQ(QnRestResult::Error::NoError, m_systemMergeFixture.prevMergeResult());
        m_systemMergeFixture.thenAllServersSynchronizedData();

        const auto cloudSystemCredentials = 
            m_systemMergeFixture.peer(0).getCloudCredentials();
        thenCloudCredentialsAreValid(cloudSystemCredentials);
        thenAllCloudCredentialsAreNotValidExcept(cloudSystemCredentials);
        thenSystemDataIsSynchronizedToCloud(cloudSystemCredentials);
    }

    void thenCloudCredentialsAreValid(
        const nx::hpm::api::SystemCredentials& cloudCredentials)
    {
        nx::cdb::api::SystemDataEx systemData;
        ASSERT_EQ(
            nx::cdb::api::ResultCode::ok,
            m_cdb.getSystem(
                cloudCredentials.systemId.toStdString(), cloudCredentials.key.toStdString(),
                cloudCredentials.systemId.toStdString(), &systemData));
    }

    void thenCloudCredentialsAreNotValid(
        const nx::hpm::api::SystemCredentials& cloudCredentials)
    {
        nx::cdb::api::SystemDataEx systemData;
        ASSERT_NE(
            nx::cdb::api::ResultCode::ok,
            m_cdb.getSystem(
                cloudCredentials.systemId.toStdString(), cloudCredentials.key.toStdString(),
                cloudCredentials.systemId.toStdString(), &systemData));
    }

    void thenAllCloudCredentialsAreNotValidExcept(
        const nx::hpm::api::SystemCredentials& cloudCredentialsException)
    {
        for (const auto& cloudCredentials: m_systemCloudCredentials)
        {
            if (cloudCredentials.systemId == cloudCredentialsException.systemId)
                thenCloudCredentialsAreValid(cloudCredentials);
            else
                thenCloudCredentialsAreNotValid(cloudCredentials);
        }
    }

    void thenSystemDataIsSynchronizedToCloud(
        const nx::hpm::api::SystemCredentials& cloudCredentials)
    {
        // TODO
    }

private:
    nx::cdb::CdbLauncher m_cdb;
    ::ec2::test::SystemMergeFixture m_systemMergeFixture;
    nx::cdb::AccountWithPassword m_ownerAccount;
    std::vector<nx::hpm::api::SystemCredentials> m_systemCloudCredentials;

    static std::unique_ptr<QnStaticCommonModule> s_staticCommonModule;

    virtual void SetUp() override
    {
        ASSERT_TRUE(m_cdb.startAndWaitUntilStarted());

        m_ownerAccount = m_cdb.addActivatedAccount2();

        m_systemMergeFixture.addSetting(
            "-cloudIntegration/cloudDbUrl",
            nx::network::url::Builder().setScheme(nx_http::kUrlSchemeName)
                .setEndpoint(m_cdb.endpoint()).toString().toStdString());

        m_systemMergeFixture.addSetting(
            "-cloudIntegration/delayBeforeSettingMasterFlag",
            "100ms");
    }

    bool connectToCloud(::ec2::test::PeerWrapper& peerWrapper)
    {
        const auto system = m_cdb.addRandomSystemToAccount(m_ownerAccount);
        return peerWrapper.saveCloudSystemCredentials(
            system.id,
            system.authKey,
            m_ownerAccount.email);
    }
};

std::unique_ptr<QnStaticCommonModule> CloudMerge::s_staticCommonModule;

TEST_F(CloudMerge, DISABLED_cloud_systems_with_the_same_owner_can_be_merged)
{
    givenTwoCloudSystemsWithTheSameOwner();
    addRandomCloudUserToEachSystem();

    whenMergeSystems();

    thenSystemsMerged();
}

} // namespace test
