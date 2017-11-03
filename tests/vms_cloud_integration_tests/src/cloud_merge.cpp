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
            ASSERT_TRUE(connectToCloud(m_systemMergeFixture.peer(i), m_cloudAccounts[0]));
            m_systemCloudCredentials.push_back(
                m_systemMergeFixture.peer(i).getCloudCredentials());
        }
    }

    void givenTwoCloudSystemsWithDifferentOwners()
    {
        const int numberOfSystemsToCreate = 2;

        ASSERT_TRUE(m_systemMergeFixture.initializeSingleServerSystems(numberOfSystemsToCreate));

        while (m_cloudAccounts.size() < numberOfSystemsToCreate)
            m_cloudAccounts.push_back(m_cdb.addActivatedAccount2());

        for (int i = 0; i < m_systemMergeFixture.peerCount(); ++i)
        {
            ASSERT_TRUE(connectToCloud(m_systemMergeFixture.peer(i), m_cloudAccounts[i]));
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

    void whenGoneOffline()
    {
        m_cdb.stop();
    }

    void thenMergeSucceded()
    {
        ASSERT_EQ(QnRestResult::Error::NoError, m_systemMergeFixture.prevMergeResult());
    }
    
    void andAllServersAreInterconnected()
    {
        m_systemMergeFixture.thenAllServersAreInterconnected();
    }
    
    void andAllServersSynchronizedData()
    {
        m_systemMergeFixture.thenAllServersSynchronizedData();
    }

    void andMergeHistoryRecordIsAdded()
    {
        const ::ec2::ApiSystemMergeHistoryRecordList systemMergeHistory =
            m_systemMergeFixture.waitUntilMergeHistoryIsAdded();
        ASSERT_GE(systemMergeHistory.size(), 1U);
        ASSERT_TRUE(
            ::ec2::ApiSystemMergeHistoryRecord::verifyRecordSignature(
                systemMergeHistory[0],
                m_systemCloudCredentials.back().key));
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

    bool areAllCloudCredentialsNotValidExcept(
        const nx::hpm::api::SystemCredentials& cloudCredentialsException)
    {
        for (const auto& cloudCredentials: m_systemCloudCredentials)
        {
            if (cloudCredentials.systemId == cloudCredentialsException.systemId)
                continue;

            nx::cdb::api::SystemDataEx systemData;
            const auto resultCode = m_cdb.getSystem(
                cloudCredentials.systemId.toStdString(), cloudCredentials.key.toStdString(),
                cloudCredentials.systemId.toStdString(), &systemData);

            if (resultCode == nx::cdb::api::ResultCode::ok)
                return false;
        }

        return true;
    }

    void waitUntilAllCloudCredentialsAreNotValidExcept(
        const nx::hpm::api::SystemCredentials& cloudCredentialsException)
    {
        while (!areAllCloudCredentialsNotValidExcept(cloudCredentialsException))
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    void thenSystemDataIsSynchronizedToCloud(
        const nx::hpm::api::SystemCredentials& cloudCredentials)
    {
        auto mediaServerClient = m_systemMergeFixture.peer(0).mediaServerClient();
        ::ec2::ApiUserDataList vmsUsers;
        ASSERT_EQ(::ec2::ErrorCode::ok, mediaServerClient->ec2GetUsers(&vmsUsers));

        // Waiting until cloud has all that users vms has.
        for (;;)
        {
            std::vector<nx::cdb::api::SystemSharingEx> cloudUsers;
            ASSERT_EQ(
                nx::cdb::api::ResultCode::ok,
                m_cdb.getSystemSharings(
                    cloudCredentials.systemId.toStdString(),
                    cloudCredentials.key.toStdString(),
                    &cloudUsers));

            int usersFound = 0;
            for (const auto& vmsUser: vmsUsers)
            {
                if (!vmsUser.isCloud)
                    continue;
                for (const auto& cloudUser: cloudUsers)
                {
                    if (cloudUser.accountEmail == vmsUser.name.toStdString())
                    {
                        ++usersFound;
                        break;
                    }
                }
            }

            if (usersFound == (int) cloudUsers.size())
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void thenMergeFails()
    {
        ASSERT_NE(QnRestResult::Error::NoError, m_systemMergeFixture.prevMergeResult());
    }

private:
    nx::cdb::CdbLauncher m_cdb;
    ::ec2::test::SystemMergeFixture m_systemMergeFixture;
    std::vector<nx::cdb::AccountWithPassword> m_cloudAccounts;
    std::vector<nx::hpm::api::SystemCredentials> m_systemCloudCredentials;

    static std::unique_ptr<QnStaticCommonModule> s_staticCommonModule;

    virtual void SetUp() override
    {
        ASSERT_TRUE(m_cdb.startAndWaitUntilStarted());

        m_cloudAccounts.push_back(m_cdb.addActivatedAccount2());

        m_systemMergeFixture.addSetting(
            "-cloudIntegration/cloudDbUrl",
            nx::network::url::Builder().setScheme(nx_http::kUrlSchemeName)
                .setEndpoint(m_cdb.endpoint()).toString().toStdString());

        m_systemMergeFixture.addSetting(
            "-cloudIntegration/delayBeforeSettingMasterFlag",
            "100ms");
    }

    bool connectToCloud(
        ::ec2::test::PeerWrapper& peerWrapper,
        const nx::cdb::AccountWithPassword& ownerAccount)
    {
        const auto system = m_cdb.addRandomSystemToAccount(ownerAccount);
        return peerWrapper.saveCloudSystemCredentials(
            system.id,
            system.authKey,
            ownerAccount.email);
    }
};

std::unique_ptr<QnStaticCommonModule> CloudMerge::s_staticCommonModule;

TEST_F(CloudMerge, cloud_systems_with_the_same_owner_can_be_merged)
{
    givenTwoCloudSystemsWithTheSameOwner();
    addRandomCloudUserToEachSystem();

    whenMergeSystems();

    thenMergeSucceded();
    andAllServersAreInterconnected();
    andAllServersSynchronizedData();
    andMergeHistoryRecordIsAdded();
}

TEST_F(CloudMerge, cloud_systems_with_different_owners_cannot_be_merged)
{
    givenTwoCloudSystemsWithDifferentOwners();
    whenMergeSystems();
    thenMergeFails();
}

} // namespace test
