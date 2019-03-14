#include <boost/optional.hpp>

#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/time.h>

#include <nx/cloud/db/dao/rdb/dao_rdb_user_authentication.h>
#include <nx/cloud/db/dao/memory/dao_memory_user_authentication.h>
#include <nx/cloud/db/test_support/base_persistent_data_test.h>

namespace nx::cloud::db {
namespace dao {
namespace rdb {
namespace test {

using BasePersistentDataTest = nx::cloud::db::test::BasePersistentDataTest;

template<typename DaoType>
class DaoUserAuthentication:
    public ::testing::Test,
    public BasePersistentDataTest
{
public:
    DaoUserAuthentication()
    {
        constexpr int kUserCount = 7;

        m_ownerAccount = insertRandomAccount();
        for (int i = 0; i < kUserCount; ++i)
            addAccount();

        m_system = insertRandomSystem(m_ownerAccount);

        prepareTestData();
    }

protected:
    void givenAccountWithMultipleSystems()
    {
        using namespace std::placeholders;

        for (int i = 0; i < 7; ++i)
        {
            const auto system = insertRandomSystem(m_ownerAccount);
            addUserToSystem(m_ownerAccount, system);
        }
    }

    void givenMultipleSystemsWithMultipleUsers()
    {
        constexpr int kSystemCount = 7;

        for (int i = 0; i < kSystemCount; ++i)
            addSystem();

        for (const auto& system: m_systems)
            for (const auto& account: m_accounts)
                addUserToSystem(account, system);
    }

    std::string getRandomSystemId()
    {
        return nx::utils::random::choice(m_systems).id;
    }

    void givenSystemWithExpiredUsers()
    {
        addSystem();
        for (const auto& account: m_accounts)
            addUserToSystem(account, m_systems.back(), std::chrono::hours(-1));
    }

    void givenSystemWithNotExpiredUsers()
    {
        addSystem();
        for (const auto& account: m_accounts)
            addUserToSystem(account, m_systems.back(), std::chrono::hours(1));
    }

    void whenDeleteEveryUserAuthInfoInSystem(const std::string& systemId)
    {
        using namespace std::placeholders;

        executeUpdateQuerySyncThrow(
            std::bind(&dao::AbstractUserAuthentication::deleteSystemAuthRecords,
                &m_dao, _1, systemId));
    }

    void whenFetchExpiredSystems()
    {
        using namespace std::placeholders;

        m_prevFetchSystemsWithExpiredAuthRecordsResult = executeSelectQuerySyncThrow(
            std::bind(&dao::AbstractUserAuthentication::fetchSystemsWithExpiredAuthRecords,
                &m_dao, _1, 1000));
    }

    void whenAddedAuthRecord()
    {
        using namespace std::placeholders;

        executeUpdateQuerySyncThrow(
            std::bind(&dao::AbstractUserAuthentication::insertUserAuthRecords,
                &m_dao, _1, m_system.id, m_ownerAccount.id, m_expectedAuthInfo));
    }

    void whenFetchedAuthRecords()
    {
        using namespace std::placeholders;

        m_fetchedAuthInfo = executeSelectQuerySyncThrow(
            std::bind(&dao::AbstractUserAuthentication::fetchUserAuthRecords,
                &m_dao, _1, m_system.id, m_ownerAccount.id));
    }

    void whenInsertedSystemNonce()
    {
        using namespace std::placeholders;

        m_expectedNonce = nx::utils::generateRandomName(12).toStdString();

        executeUpdateQuerySyncThrow(
            std::bind(&dao::AbstractUserAuthentication::insertOrReplaceSystemNonce,
                &m_dao, _1, m_system.id, m_expectedNonce));
    }

    void whenReplacedExistingNonce()
    {
        whenInsertedSystemNonce();
        whenInsertedSystemNonce();
    }

    void whenFetchedSystemNonce()
    {
        using namespace std::placeholders;

        m_fetchedNonce = executeSelectQuerySyncThrow(
            std::bind(&dao::AbstractUserAuthentication::fetchSystemNonce,
                &m_dao, _1, m_system.id));
    }

    void whenDeleteAllRecords()
    {
        using namespace std::placeholders;

        executeUpdateQuerySyncThrow(
            std::bind(&dao::AbstractUserAuthentication::deleteAccountAuthRecords,
                &m_dao, _1, m_ownerAccount.id));
    }

    void thenAuthRecordCanBeRead()
    {
        whenFetchedAuthRecords();
        ASSERT_EQ(m_expectedAuthInfo, m_fetchedAuthInfo);
    }

    void thenRecordListIsEmpty()
    {
        ASSERT_TRUE(m_fetchedAuthInfo.records.empty());
    }

    void thenNonceCanBeFetched()
    {
        whenFetchedSystemNonce();
        ASSERT_EQ(m_expectedNonce, *m_fetchedNonce);
    }

    void thenNoNonceFound()
    {
        ASSERT_FALSE(static_cast<bool>(m_fetchedNonce));
    }

    void thenEverySystemInformationCanBeSelected()
    {
        using namespace std::placeholders;

        auto systems = executeSelectQuerySyncThrow(
            std::bind(&dao::AbstractUserAuthentication::fetchAccountSystems,
                &m_dao, _1, m_ownerAccount.id));

        // Validating.
        ASSERT_EQ(m_expectedSystemInfo.size(), systems.size());
        for (const auto& system: systems)
        {
            auto it = m_expectedSystemInfo.find(system.systemId);
            ASSERT_NE(m_expectedSystemInfo.end(), it);
            ASSERT_EQ(it->second.nonce, system.nonce);
            //ASSERT_EQ(it->second.vmsUserId, system.vmsUserId);
        }
    }

    void thenAllRecordsHaveBeenDeleted()
    {
        using namespace std::placeholders;

        auto systems = executeSelectQuerySyncThrow(
            std::bind(&dao::AbstractUserAuthentication::fetchAccountSystems,
                &m_dao, _1, m_ownerAccount.id));
        for (const auto& system: systems)
        {
            auto fetchedAuthInfo = executeSelectQuerySyncThrow(
                std::bind(&dao::AbstractUserAuthentication::fetchUserAuthRecords,
                    &m_dao, _1, system.systemId, m_ownerAccount.id));
            ASSERT_TRUE(fetchedAuthInfo.records.empty());
        }
    }

    void thenOnlyExpiredSystemsAreReported()
    {
        ASSERT_FALSE(m_prevFetchSystemsWithExpiredAuthRecordsResult.empty());
        for (const auto& system: m_systems)
        {
            if (nx::utils::contains(m_prevFetchSystemsWithExpiredAuthRecordsResult, system.id))
                assertSystemContainsExpiredRecords(system.id);
            else
                assertSystemDoesNotContainExpiredRecords(system.id);
        }
    }

    void thenAllRecordsHaveBeenDeletedInSystem(const std::string& systemId)
    {
        assertNoAuthInfoInSystem(systemId);
    }

    void andThereAreAuthInfoInEverySystemExcept(
        const std::string& exceptionSystemId)
    {
        for (const auto& system: m_systems)
        {
            if (system.id == exceptionSystemId)
                assertNoAuthInfoInSystem(system.id);
            else
                assertThereIsAuthInfoInSystem(system.id);
        }
    }

private:
    DaoType m_dao;
    api::AccountData m_ownerAccount;
    api::SystemData m_system;
    api::AuthInfo m_expectedAuthInfo;
    api::AuthInfo m_fetchedAuthInfo;
    std::string m_expectedNonce;
    boost::optional<std::string> m_fetchedNonce;
    std::map<std::string, AbstractUserAuthentication::SystemInfo> m_expectedSystemInfo;
    std::vector<api::AccountData> m_accounts;
    std::vector<api::SystemData> m_systems;
    std::vector<std::string> m_prevFetchSystemsWithExpiredAuthRecordsResult;
    std::multimap<std::string /*systemId*/, std::string /*accountId*/> m_systemToUser;

    void addAccount()
    {
        m_accounts.push_back(insertRandomAccount());
    }

    void addSystem()
    {
        m_systems.push_back(insertRandomSystem(m_ownerAccount));
    }

    void prepareTestData()
    {
        m_expectedAuthInfo = generateAuthInfo();
    }

    void addUserToSystem(
        const api::AccountData& account,
        const api::SystemData& system,
        const std::chrono::milliseconds expirationPeriod = std::chrono::milliseconds::zero())
    {
        using namespace std::placeholders;

        auto authInfo = generateAuthInfo(expirationPeriod);

        AbstractUserAuthentication::SystemInfo systemInfo;
        systemInfo.systemId = system.id;
        systemInfo.nonce = authInfo.records[0].nonce;
        //systemInfo.vmsUserId = ;
        m_expectedSystemInfo.emplace(system.id, systemInfo);

        executeUpdateQuerySyncThrow(
            std::bind(&dao::AbstractUserAuthentication::insertUserAuthRecords,
                &m_dao, _1, system.id, account.id, authInfo));
        executeUpdateQuerySyncThrow(
            std::bind(&dao::AbstractUserAuthentication::insertOrReplaceSystemNonce,
                &m_dao, _1, system.id, authInfo.records[0].nonce));

        m_systemToUser.emplace(system.id, account.id);
    }

    api::AuthInfo generateAuthInfo(
        const std::chrono::milliseconds expirationPeriod = std::chrono::milliseconds::zero())
    {
        api::AuthInfo authInfo;
        authInfo.records.resize(1);
        authInfo.records[0].expirationTime =
            nx::utils::floor<std::chrono::milliseconds>(nx::utils::utcTime()) +
            expirationPeriod;
        authInfo.records[0].nonce = nx::utils::generateRandomName(7).toStdString();
        authInfo.records[0].intermediateResponse =
            nx::utils::generateRandomName(14).toStdString();
        return authInfo;
    }

    void assertNoAuthInfoInSystem(const std::string& systemId)
    {
        assertConditionInSystemAuthRecords(
            systemId,
            [this](const api::AuthInfo& authInfo)
            {
                ASSERT_TRUE(authInfo.records.empty());
            });
    }

    void assertThereIsAuthInfoInSystem(const std::string& systemId)
    {
        assertConditionInSystemAuthRecords(
            systemId,
            [this](const api::AuthInfo& authInfo)
            {
                ASSERT_FALSE(authInfo.records.empty());
            });
    }

    void assertSystemContainsExpiredRecords(const std::string& systemId)
    {
        ASSERT_TRUE(isSystemExpired(systemId));
    }

    void assertSystemDoesNotContainExpiredRecords(const std::string& systemId)
    {
        ASSERT_FALSE(isSystemExpired(systemId));
    }

    bool isSystemExpired(const std::string& systemId)
    {
        bool isExpired = false;
        assertConditionInSystemAuthRecords(
            systemId,
            [this, &isExpired](const api::AuthInfo& authInfo)
            {
                for (const auto& record: authInfo.records)
                {
                    if (record.expirationTime < nx::utils::utcTime())
                        isExpired = true;
                }
            });

        return isExpired;
    }

    template<typename Cond>
    void assertConditionInSystemAuthRecords(const std::string& systemId, Cond cond)
    {
        using namespace std::placeholders;

        const auto systemUserRange = m_systemToUser.equal_range(systemId);
        for (auto it = systemUserRange.first; it != systemUserRange.second; ++it)
        {
            auto fetchedAuthInfo = executeSelectQuerySyncThrow(
                std::bind(&dao::AbstractUserAuthentication::fetchUserAuthRecords,
                    &m_dao, _1, systemId, it->second));
            cond(fetchedAuthInfo);
        }
    }
};

TYPED_TEST_CASE_P(DaoUserAuthentication);

//-------------------------------------------------------------------------------------------------
// Test cases.

TYPED_TEST_P(DaoUserAuthentication, saved_user_auth_records_can_be_read_later)
{
    this->whenAddedAuthRecord();
    this->thenAuthRecordCanBeRead();
}

TYPED_TEST_P(DaoUserAuthentication, fetching_empty_auth_record_list)
{
    this->whenFetchedAuthRecords();
    this->thenRecordListIsEmpty();
}

TYPED_TEST_P(DaoUserAuthentication, inserting_system_nonce)
{
    this->whenInsertedSystemNonce();
    this->thenNonceCanBeFetched();
}

TYPED_TEST_P(DaoUserAuthentication, fetching_not_existing_nonce)
{
    this->whenFetchedSystemNonce();
    this->thenNoNonceFound();
}

TYPED_TEST_P(DaoUserAuthentication, replacing_system_nonce)
{
    this->whenReplacedExistingNonce();
    this->thenNonceCanBeFetched();
}

TYPED_TEST_P(DaoUserAuthentication, fetching_every_system_of_an_account)
{
    this->givenAccountWithMultipleSystems();
    this->thenEverySystemInformationCanBeSelected();
}

TYPED_TEST_P(DaoUserAuthentication, deleting_every_auth_record_of_an_account)
{
    this->givenAccountWithMultipleSystems();
    this->whenDeleteAllRecords();
    this->thenAllRecordsHaveBeenDeleted();
}

TYPED_TEST_P(DaoUserAuthentication, deleting_every_auth_record_of_a_system)
{
    this->givenMultipleSystemsWithMultipleUsers();
    const auto systemId = this->getRandomSystemId();

    this->whenDeleteEveryUserAuthInfoInSystem(systemId);

    this->thenAllRecordsHaveBeenDeletedInSystem(systemId);
    this->andThereAreAuthInfoInEverySystemExcept(systemId);
}

TYPED_TEST_P(DaoUserAuthentication, fetchExpiredSystems_returns_only_expired_systems)
{
    this->givenSystemWithExpiredUsers();
    this->givenSystemWithNotExpiredUsers();

    this->whenFetchExpiredSystems();

    this->thenOnlyExpiredSystemsAreReported();
}

REGISTER_TYPED_TEST_CASE_P(DaoUserAuthentication,
    saved_user_auth_records_can_be_read_later,
    fetching_empty_auth_record_list,
    inserting_system_nonce,
    fetching_not_existing_nonce,
    replacing_system_nonce,
    fetching_every_system_of_an_account,
    deleting_every_auth_record_of_an_account,
    deleting_every_auth_record_of_a_system,
    fetchExpiredSystems_returns_only_expired_systems);

//-------------------------------------------------------------------------------------------------

INSTANTIATE_TYPED_TEST_CASE_P(
    DaoRdbUserAuthentication,
    DaoUserAuthentication,
    rdb::UserAuthentication);

INSTANTIATE_TYPED_TEST_CASE_P(
    DaoMemoryUserAuthentication,
    DaoUserAuthentication,
    memory::UserAuthentication);

} // namespace test
} // namespace rdb
} // namespace dao
} // namespace nx::cloud::db
