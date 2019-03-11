#include <memory>
#include <vector>

#include <gtest/gtest.h>
#include <test_support/peer_wrapper.h>
#include <test_support/merge_test_fixture.h>

#include <nx/cloud/db/ec2/data_conversion.h>
#include <nx/cloud/db/test_support/cdb_launcher.h>
#include <nx/network/app_info.h>
#include <nx/network/address_resolver.h>
#include <nx/network/cloud/abstract_cloud_system_credentials_provider.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/test_support/test_options.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>

namespace test {

namespace {
static const QnUuid kUserResourceTypeGuid("{774e6ecd-ffc6-ae88-0165-8f4a6d0eafa7}");
} // namespace

class CloudMerge:
    public ::testing::Test,
    public nx::utils::test::TestWithTemporaryDirectory
{
public:
    CloudMerge():
        nx::utils::test::TestWithTemporaryDirectory(
            "vms_cloud_integration.cloudMerge",
            QString()),
        m_systemMergeFixture(testDataDir().toStdString() + "/merge_test.data")
    {
    }

    static void SetUpTestCase()
    {
        s_staticCommonModule =
            std::make_unique<QnStaticCommonModule>(nx::vms::api::PeerType::server);
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

        bindAllSystemsToCloud(m_cloudAccounts[0]);
    }

    void givenTwoCloudSystemsWithDifferentOwners()
    {
        const int numberOfSystemsToCreate = 2;

        ASSERT_TRUE(m_systemMergeFixture.initializeSingleServerSystems(numberOfSystemsToCreate));

        while (m_cloudAccounts.size() < numberOfSystemsToCreate)
            m_cloudAccounts.push_back(m_cdb.addActivatedAccount2());

        bindAllSystemsToCloud(m_cloudAccounts);
    }

    void givenSystemProducedByAMerge()
    {
        givenTwoCloudSystemsWithTheSameOwner();
        whenMergeSystems();
        thenMergeFullyCompleted();
    }

    nx::cloud::db::AccountWithPassword registerCloudUser()
    {
        return m_cdb.addActivatedAccount2();
    }

    void addRandomCloudUserToEachSystem()
    {
        for (int i = 0; i < m_systemMergeFixture.peerCount(); ++i)
        {
            const auto someCloudUser = m_cdb.addActivatedAccount2();
            nx::vms::api::UserData vmsUserData;

            vmsUserData.id = guidFromArbitraryData(someCloudUser.email);
            vmsUserData.typeId = kUserResourceTypeGuid;
            vmsUserData.email = someCloudUser.email.c_str();
            vmsUserData.name = someCloudUser.email.c_str();
            vmsUserData.fullName = someCloudUser.fullName.c_str();
            vmsUserData.isCloud = true;
            vmsUserData.isEnabled = true;
            vmsUserData.realm = nx::network::AppInfo::realm();
            vmsUserData.hash = nx::vms::api::UserData::kCloudPasswordStub;
            vmsUserData.digest = nx::vms::api::UserData::kCloudPasswordStub;
            vmsUserData.permissions = GlobalPermission::adminPermissions;

            auto mediaServerClient = m_systemMergeFixture.peer(i).mediaServerClient();
            ASSERT_EQ(::ec2::ErrorCode::ok, mediaServerClient->ec2SaveUser(vmsUserData));
        }
    }

    void waitForEverySystemToGoOnline()
    {
        for (;;)
        {
            std::vector<nx::cloud::db::api::SystemDataEx> systems;
            ASSERT_EQ(
                nx::cloud::db::api::ResultCode::ok,
                m_cdb.getSystems(m_cloudAccounts[0].email, m_cloudAccounts[0].password, &systems));
            ASSERT_GT(systems.size(), 0U);
            bool everySystemIsOnline = true;
            for (const auto& system: systems)
                everySystemIsOnline &= system.stateOfHealth == nx::cloud::db::api::SystemHealth::online;

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
            nx::cloud::db::api::ResultCode::ok,
            m_cdb.mergeSystems(
                m_cloudAccounts[0],
                m_systemMergeFixture.peer(0).getCloudCredentials().systemId.toStdString(),
                m_systemMergeFixture.peer(1).getCloudCredentials().systemId.toStdString()));
    }

    void whenGoneOffline()
    {
        m_cdb.stop();
    }

    void thenMergeSucceeded()
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
        const nx::vms::api::SystemMergeHistoryRecordList systemMergeHistory =
            m_systemMergeFixture.waitUntilMergeHistoryIsAdded();
        ASSERT_GE(systemMergeHistory.size(), 1U);
        if (!systemMergeHistory.front().mergedSystemCloudId.isEmpty())
        {
            ASSERT_TRUE(systemMergeHistory.front().verify(m_systemCloudCredentials.back().key));
        }
    }

    void andSlaveSystemDisappearedFromCloudDb()
    {
        std::vector<nx::cloud::db::api::SystemDataEx> systems;
        ASSERT_EQ(
            nx::cloud::db::api::ResultCode::ok,
            m_cdb.getSystems(
                m_cloudAccounts[0].email, m_cloudAccounts[0].password,
                &systems));
        ASSERT_EQ(1U, systems.size());
    }

    void thenMergedSystemDisappearedFromCloud()
    {
        waitUntilAllCloudCredentialsAreNotValidExcept(m_systemCloudCredentials[0]);
    }

    void thenCloudCredentialsAreValid(
        const nx::hpm::api::SystemCredentials& cloudCredentials)
    {
        nx::cloud::db::api::SystemDataEx systemData;
        ASSERT_EQ(
            nx::cloud::db::api::ResultCode::ok,
            m_cdb.getSystem(
                cloudCredentials.systemId.toStdString(), cloudCredentials.key.toStdString(),
                cloudCredentials.systemId.toStdString(), &systemData));
    }

    void thenCloudCredentialsAreNotValid(
        const nx::hpm::api::SystemCredentials& cloudCredentials)
    {
        nx::cloud::db::api::SystemDataEx systemData;
        ASSERT_NE(
            nx::cloud::db::api::ResultCode::ok,
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

            nx::cloud::db::api::SystemDataEx systemData;
            const auto resultCode = m_cdb.getSystem(
                cloudCredentials.systemId.toStdString(), cloudCredentials.key.toStdString(),
                cloudCredentials.systemId.toStdString(), &systemData);

            if (resultCode == nx::cloud::db::api::ResultCode::ok)
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
        nx::vms::api::UserDataList vmsUsers;
        ASSERT_EQ(::ec2::ErrorCode::ok, mediaServerClient->ec2GetUsers(&vmsUsers));

        // Waiting until cloud has all that users vms has.
        for (;;)
        {
            std::vector<nx::cloud::db::api::SystemSharingEx> cloudUsers;
            ASSERT_EQ(
                nx::cloud::db::api::ResultCode::ok,
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
        thenMergeSucceeded();
        andAllServersAreInterconnected();
        andAllServersSynchronizedData();
        andMergeHistoryRecordIsAdded();

        waitUntilVmsTransactionLogMatchesCloudOne();

        andSlaveSystemDisappearedFromCloudDb();
    }

    void waitToSystemToBeOnline()
    {
        for (;;)
        {
            if (systemIsOnline(m_systemMergeFixture.peer(0)
                    .getCloudCredentials().systemId.toStdString()))
            {
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void waitUntilVmsTransactionLogMatchesCloudOne(int peerIndex = 0)
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

            std::sort(
                vmsTransactionLog.begin(), vmsTransactionLog.end(),
                [](const auto& left, const auto& right) { return left.tranGuid < right.tranGuid; });
            std::sort(
                cloudTransactionLog.begin(), cloudTransactionLog.end(),
                [](const auto& left, const auto& right) { return left.tranGuid < right.tranGuid; });

            if (vmsTransactionLog == cloudTransactionLog)
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void shareSystem(int index, const nx::cloud::db::AccountWithPassword& cloudUser)
    {
        ASSERT_EQ(
            nx::cloud::db::api::ResultCode::ok,
            m_cdb.shareSystem(
                m_cloudAccounts[index],
                m_systemMergeFixture.peer(index).getCloudCredentials().systemId.toStdString(),
                cloudUser.email,
                nx::cloud::db::api::SystemAccessRole::cloudAdmin));
    }

    void disconnectFromCloud(int index)
    {
        ASSERT_TRUE(m_systemMergeFixture.peer(index).detachFromCloud());
    }

    void assertCloudOwnerIsAbleToLogin()
    {
        assertUserIsAbleToLogin(0, m_cloudAccounts.front());
    }

    void assertUserIsAbleToLogin(int peerIndex, const nx::cloud::db::AccountWithPassword& cloudUser)
    {
        ASSERT_TRUE(testUserLogin(peerIndex, cloudUser));
    }

    void assertUserIsNotAbleToLogin(int peerIndex, const nx::cloud::db::AccountWithPassword& cloudUser)
    {
        ASSERT_FALSE(testUserLogin(peerIndex, cloudUser));
    }

    bool testUserLogin(int index, const nx::cloud::db::AccountWithPassword& cloudUser)
    {
        using namespace nx::network::http;

        auto mediaServerClient = m_systemMergeFixture.peer(index).mediaServerClient();
        mediaServerClient->setUserCredentials(Credentials(
            cloudUser.email.c_str(),
            PasswordAuthToken(cloudUser.password.c_str())));
        nx::vms::api::UserDataList users;
        return mediaServerClient->ec2GetUsers(&users) == ::ec2::ErrorCode::ok;
    }

    void generateTemporaryCloudCredentials()
    {
        nx::cloud::db::api::TemporaryCredentialsParams params;
        params.type = "short";
        nx::cloud::db::api::TemporaryCredentials temporaryCredentials;

        ASSERT_EQ(
            nx::cloud::db::api::ResultCode::ok,
            m_cdb.createTemporaryCredentials(
                owner().email, owner().password,
                params,
                &temporaryCredentials));

        m_temporaryCredentials.push_back(std::move(temporaryCredentials));
    }

    void assertOwnerTemporaryCredentialsAreAcceptedByEveryServer()
    {
        auto client = m_systemMergeFixture.peer(0).mediaServerClient();
        client->setUserCredentials(nx::network::http::Credentials(
            m_temporaryCredentials.front().login.c_str(),
            nx::network::http::PasswordAuthToken(m_temporaryCredentials.front().password.c_str())));

        // TODO: Check every server.

        nx::vms::api::UserDataList users;
        ASSERT_EQ(::ec2::ErrorCode::ok, client->ec2GetUsers(&users));
    }

    const nx::cloud::db::AccountWithPassword& owner() const
    {
        return m_cloudAccounts.front();
    }

private:
    nx::cloud::db::CdbLauncher m_cdb;
    ::ec2::test::SystemMergeFixture m_systemMergeFixture;
    std::vector<nx::cloud::db::AccountWithPassword> m_cloudAccounts;
    std::vector<nx::hpm::api::SystemCredentials> m_systemCloudCredentials;
    nx::network::http::TestHttpServer m_httpProxy;
    std::vector<nx::cloud::db::api::TemporaryCredentials> m_temporaryCredentials;

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

    void bindAllSystemsToCloud(const nx::cloud::db::AccountWithPassword& account)
    {
        std::vector<nx::cloud::db::AccountWithPassword> accounts(
            m_systemMergeFixture.peerCount(),
            account);

        bindAllSystemsToCloud(accounts);
    }

    void bindAllSystemsToCloud(
        const std::vector<nx::cloud::db::AccountWithPassword>& accounts)
    {
        m_systemCloudCredentials.resize(m_systemMergeFixture.peerCount());

        auto bindPeer = [this, &accounts](int index)
        {
            ASSERT_TRUE(connectToCloud(m_systemMergeFixture.peer(index), accounts[index]));
            m_systemCloudCredentials[index] =
                m_systemMergeFixture.peer(index).getCloudCredentials();
        };

        // NOTE: Randomizing "bind to cloud" order since there is a class of bugs related to this.

        if (::testing::UnitTest::GetInstance()->random_seed() & 1)
        {
            for (int i = 0; i < m_systemMergeFixture.peerCount(); ++i)
                bindPeer(i);
        }
        else
        {
            for (int i = m_systemMergeFixture.peerCount() - 1; i >= 0; --i)
                bindPeer(i);
        }
    }

    bool connectToCloud(
        ::ec2::test::PeerWrapper& peerWrapper,
        const nx::cloud::db::AccountWithPassword& ownerAccount)
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

    bool systemIsOnline(const std::string& systemId)
    {
        nx::cloud::db::api::SystemDataEx system;
        if (m_cdb.getSystem(
                m_cloudAccounts[0].email, m_cloudAccounts[0].password,
                systemId,
                &system) != nx::cloud::db::api::ResultCode::ok)
        {
            return false;
        }

        return system.stateOfHealth == nx::cloud::db::api::SystemHealth::online;
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
    waitUntilVmsTransactionLogMatchesCloudOne();
}

TEST_F(CloudMerge, system_disconnected_from_cloud_is_properly_merged_with_a_cloud_system)
{
    givenTwoCloudSystemsWithDifferentOwners();
    const int newerSystemIndex = 1;
    const int olderSystemIndex = 0;
    const auto someCloudUser = registerCloudUser();

    shareSystem(newerSystemIndex, someCloudUser);
    waitUntilVmsTransactionLogMatchesCloudOne(newerSystemIndex);

    disconnectFromCloud(newerSystemIndex); //< Every cloud user is removed from system.
    assertUserIsNotAbleToLogin(newerSystemIndex, someCloudUser);

    waitUntilVmsTransactionLogMatchesCloudOne(olderSystemIndex);

    whenMergeSystems();
    thenMergeFullyCompleted();

    waitUntilVmsTransactionLogMatchesCloudOne(olderSystemIndex);

    shareSystem(olderSystemIndex, someCloudUser);
    waitUntilVmsTransactionLogMatchesCloudOne(olderSystemIndex);
    assertUserIsAbleToLogin(olderSystemIndex, someCloudUser);
}

TEST_F(CloudMerge, resulting_system_can_be_accessed_via_temporary_cloud_credentials)
{
    givenSystemProducedByAMerge();
    assertCloudOwnerIsAbleToLogin();

    generateTemporaryCloudCredentials();
    assertOwnerTemporaryCredentialsAreAcceptedByEveryServer();
}

} // namespace test
