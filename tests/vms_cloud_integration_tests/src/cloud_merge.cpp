#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <nx/network/cloud/abstract_cloud_system_credentials_provider.h>
#include <nx/network/address_resolver.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/test_support/test_options.h>

#include <nx/cloud/cdb/ec2/data_conversion.h>
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
    void givenCloudSystem()
    {
        ASSERT_TRUE(m_systemMergeFixture.initializeSingleServerSystems(1));
        ASSERT_TRUE(connectToCloud(
            m_systemMergeFixture.peer(m_systemMergeFixture.peerCount()-1), m_cloudAccounts[0]));
        m_systemCloudCredentials.push_back(
            m_systemMergeFixture.peer(m_systemMergeFixture.peerCount()-1).getCloudCredentials());
    }

    void givenNonCloudSystem()
    {
        ASSERT_TRUE(m_systemMergeFixture.initializeSingleServerSystems(1));
    }

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

    nx::cdb::AccountWithPassword registerCloudUser()
    {
        return m_cdb.addActivatedAccount2();
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

    void waitForEverySystemToGoOnline()
    {
        for (;;)
        {
            std::vector<nx::cdb::api::SystemDataEx> systems;
            ASSERT_EQ(
                nx::cdb::api::ResultCode::ok,
                m_cdb.getSystems(m_cloudAccounts[0].email, m_cloudAccounts[0].password, &systems));
            ASSERT_GT(systems.size(), 0U);
            bool everySystemIsOnline = true;
            for (const auto& system: systems)
                everySystemIsOnline &= system.stateOfHealth == nx::cdb::api::SystemHealth::online;

            if (everySystemIsOnline)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void whenMergeSystems()
    {
        m_systemMergeFixture.mergeSystems();
    }

    void whenMergeSystemsWithCloudDbRequest()
    {
        ASSERT_EQ(
            nx::cdb::api::ResultCode::ok,
            m_cdb.mergeSystems(
                m_cloudAccounts[0],
                m_systemMergeFixture.peer(0).getCloudCredentials().systemId.toStdString(),
                m_systemMergeFixture.peer(1).getCloudCredentials().systemId.toStdString()));
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
        m_systemMergeFixture.waitUntilAllServersAreInterconnected();
    }

    void andAllServersSynchronizedData()
    {
        m_systemMergeFixture.waitUntilAllServersSynchronizedData();
    }

    void andMergeHistoryRecordIsAdded()
    {
        const ::ec2::ApiSystemMergeHistoryRecordList systemMergeHistory =
            m_systemMergeFixture.waitUntilMergeHistoryIsAdded();
        ASSERT_GE(systemMergeHistory.size(), 1U);
        if (!systemMergeHistory.front().mergedSystemCloudId.isEmpty())
        {
            ASSERT_TRUE(systemMergeHistory.front().verify(m_systemCloudCredentials.back().key));
        }
    }

    void thenMergedSystemDisappearedFromCloud()
    {
        waitUntilAllCloudCredentialsAreNotValidExcept(m_systemCloudCredentials[0]);
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
            int cloudUsersInVms = 0;
            for (const auto& vmsUser: vmsUsers)
            {
                if (!vmsUser.isCloud)
                    continue;
                ++cloudUsersInVms;
                for (const auto& cloudUser: cloudUsers)
                {
                    if (cloudUser.accountEmail == vmsUser.name.toStdString())
                    {
                        ++usersFound;
                        break;
                    }
                }
            }

            if (usersFound == cloudUsersInVms)
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void thenMergeFails()
    {
        ASSERT_NE(QnRestResult::Error::NoError, m_systemMergeFixture.prevMergeResult());
    }

    void thenMergeFullyCompleted()
    {
        thenMergeSucceded();
        andAllServersAreInterconnected();
        andAllServersSynchronizedData();
        andMergeHistoryRecordIsAdded();
    }

    void waitUntilVmsTranscationLogMatchesCloudOne(int peerIndex = 0)
    {
        auto mediaServerClient = m_systemMergeFixture.peer(peerIndex).mediaServerClient();

        for (;;)
        {
            ::ec2::ApiTranLogFilter filter;
            filter.cloudOnly = true;
            ::ec2::ApiTransactionDataList vmsTransactionLog;
            ASSERT_EQ(
                ::ec2::ErrorCode::ok,
                mediaServerClient->ec2GetTransactionLog(filter, &vmsTransactionLog));

            ::ec2::ApiTransactionDataList fullVmsTransactionLog;
            ASSERT_EQ(
                ::ec2::ErrorCode::ok,
                mediaServerClient->ec2GetTransactionLog(::ec2::ApiTranLogFilter(), &fullVmsTransactionLog));

            ::ec2::ApiTransactionDataList cloudTransactionLog;
            m_cdb.getTransactionLog(
                m_cloudAccounts[peerIndex].email,
                m_cloudAccounts[peerIndex].password,
                m_systemMergeFixture.peer(peerIndex).getCloudCredentials().systemId.toStdString(),
                &cloudTransactionLog);

            if (vmsTransactionLog == cloudTransactionLog)
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void shareSystem(int index, const nx::cdb::AccountWithPassword& cloudUser)
    {
        ASSERT_EQ(
            nx::cdb::api::ResultCode::ok,
            m_cdb.shareSystem(
                m_cloudAccounts[index],
                m_systemMergeFixture.peer(index).getCloudCredentials().systemId.toStdString(),
                cloudUser.email,
                nx::cdb::api::SystemAccessRole::cloudAdmin));
    }

    void disconnectFromCloud(int index)
    {
        ASSERT_TRUE(m_systemMergeFixture.peer(index).detachFromCloud());
    }

    void assertUserIsAbleToLogin(int peerIndex, const nx::cdb::AccountWithPassword& cloudUser)
    {
        ASSERT_TRUE(testUserLogin(peerIndex, cloudUser));
    }

    void assertUserIsNotAbleToLogin(int peerIndex, const nx::cdb::AccountWithPassword& cloudUser)
    {
        ASSERT_FALSE(testUserLogin(peerIndex, cloudUser));
    }

    bool testUserLogin(int index, const nx::cdb::AccountWithPassword& cloudUser)
    {
        using namespace nx::network::http;

        auto mediaServerClient = m_systemMergeFixture.peer(index).mediaServerClient();
        mediaServerClient->setUserCredentials(Credentials(
            cloudUser.email.c_str(),
            PasswordAuthToken(cloudUser.password.c_str())));
        ::ec2::ApiUserDataList users;
        return mediaServerClient->ec2GetUsers(&users) == ::ec2::ErrorCode::ok;
    }

private:
    nx::cdb::CdbLauncher m_cdb;
    ::ec2::test::SystemMergeFixture m_systemMergeFixture;
    std::vector<nx::cdb::AccountWithPassword> m_cloudAccounts;
    std::vector<nx::hpm::api::SystemCredentials> m_systemCloudCredentials;
    nx::network::http::TestHttpServer m_httpProxy;

    static std::unique_ptr<QnStaticCommonModule> s_staticCommonModule;

    virtual void SetUp() override
    {
        ASSERT_TRUE(m_httpProxy.bindAndListen());

        m_cdb.addArg(
            "-vmsGateway/url",
            lm("http://%1/{targetSystemId}").arg(m_httpProxy.serverAddress()).toStdString().c_str());

        ASSERT_TRUE(m_cdb.startAndWaitUntilStarted());

        m_cloudAccounts.push_back(m_cdb.addActivatedAccount2());

        m_systemMergeFixture.addSetting(
            "-cloudIntegration/cloudDbUrl",
            nx::network::url::Builder().setScheme(nx::network::http::kUrlSchemeName)
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
        if (!peerWrapper.saveCloudSystemCredentials(
                system.id,
                system.authKey,
                ownerAccount.email))
        {
            return false;
        }

        if (!m_httpProxy.registerRedirectHandler(
                lm("/%1/api/mergeSystems").args(system.id),
                nx::network::url::Builder().setScheme(nx::network::http::kUrlSchemeName)
                    .setEndpoint(peerWrapper.endpoint()).setPath("/api/mergeSystems")))
        {
            return false;
        }

        nx::network::SocketGlobals::addressResolver().addFixedAddress(
            system.id,
            peerWrapper.endpoint());
        return true;
    }
};

std::unique_ptr<QnStaticCommonModule> CloudMerge::s_staticCommonModule;

TEST_F(CloudMerge, cloud_systems_with_the_same_owner_can_be_merged)
{
    givenTwoCloudSystemsWithTheSameOwner();
    addRandomCloudUserToEachSystem();

    whenMergeSystems();

    thenMergeFullyCompleted();
}

TEST_F(CloudMerge, merging_cloud_systems_through_cloud_db)
{
    givenTwoCloudSystemsWithTheSameOwner();
    addRandomCloudUserToEachSystem();
    waitForEverySystemToGoOnline();

    whenMergeSystemsWithCloudDbRequest();

    thenMergeFullyCompleted();
}

TEST_F(CloudMerge, cloud_systems_with_different_owners_cannot_be_merged)
{
    givenTwoCloudSystemsWithDifferentOwners();
    whenMergeSystems();
    thenMergeFails();
}

TEST_F(CloudMerge, merged_system_removed_in_cloud_after_merge)
{
    givenTwoCloudSystemsWithTheSameOwner();
    whenMergeSystems();
    thenMergedSystemDisappearedFromCloud();
}

TEST_F(CloudMerge, merging_non_cloud_system_to_a_cloud_one_does_not_affect_data_in_cloud)
{
    givenCloudSystem();
    givenNonCloudSystem();

    whenMergeSystems();

    thenMergeFullyCompleted();
    waitUntilVmsTranscationLogMatchesCloudOne();
}

TEST_F(CloudMerge, system_disconnected_from_cloud_is_properly_merged_with_a_cloud_system)
{
    givenTwoCloudSystemsWithDifferentOwners();
    const int newerSystemIndex = 1;
    const int olderSystemIndex = 0;
    const auto someCloudUser = registerCloudUser();

    shareSystem(newerSystemIndex, someCloudUser);
    waitUntilVmsTranscationLogMatchesCloudOne(newerSystemIndex);

    disconnectFromCloud(newerSystemIndex); //< Every cloud user is removed from system.
    assertUserIsNotAbleToLogin(newerSystemIndex, someCloudUser);

    waitUntilVmsTranscationLogMatchesCloudOne(olderSystemIndex);

    whenMergeSystems();
    thenMergeFullyCompleted();

    waitUntilVmsTranscationLogMatchesCloudOne(olderSystemIndex);

    shareSystem(olderSystemIndex, someCloudUser);
    waitUntilVmsTranscationLogMatchesCloudOne(olderSystemIndex);
    assertUserIsAbleToLogin(olderSystemIndex, someCloudUser);
}

} // namespace test
