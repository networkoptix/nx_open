#include <boost/optional.hpp>

#include <gtest/gtest.h>

#include <nx/utils/time.h>

#include <nx/cloud/cdb/dao/rdb/dao_rdb_user_authentication.h>

#include "base_persistent_data_test.h"

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {
namespace test {

using BasePersistentDataTest = cdb::test::BasePersistentDataTest;

class DaoRdbUserAuthentication:
    public ::testing::Test,
    public BasePersistentDataTest
{
public:
    DaoRdbUserAuthentication()
    {
        m_ownerAccount = insertRandomAccount();
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
            auto authInfo = generateAuthInfo();

            AbstractUserAuthentication::SystemInfo systemInfo;
            systemInfo.systemId = system.id;
            systemInfo.nonce = authInfo.records[0].nonce;
            //systemInfo.vmsUserId = ;
            m_expectedSystemInfo.emplace(system.id, systemInfo);

            executeUpdateQuerySyncThrow(
                std::bind(&dao::rdb::UserAuthentication::insertUserAuthRecords,
                    m_dao, _1, system.id, m_ownerAccount.id, authInfo));
            executeUpdateQuerySyncThrow(
                std::bind(&dao::rdb::UserAuthentication::insertOrReplaceSystemNonce,
                    m_dao, _1, system.id, authInfo.records[0].nonce));
        }
    }

    void whenAddedAuthRecord()
    {
        using namespace std::placeholders;

        executeUpdateQuerySyncThrow(
            std::bind(&dao::rdb::UserAuthentication::insertUserAuthRecords,
                m_dao, _1, m_system.id, m_ownerAccount.id, m_expectedAuthInfo));
    }

    void whenFetchedAuthRecords()
    {
        using namespace std::placeholders;

        m_fetchedAuthInfo = executeSelectQuerySyncThrow(
            std::bind(&dao::rdb::UserAuthentication::fetchUserAuthRecords,
                m_dao, _1, m_system.id, m_ownerAccount.id));
    }

    void whenInsertedSystemNonce()
    {
        using namespace std::placeholders;

        m_expectedNonce = nx::utils::generateRandomName(12).toStdString();

        executeUpdateQuerySyncThrow(
            std::bind(&dao::rdb::UserAuthentication::insertOrReplaceSystemNonce,
                m_dao, _1, m_system.id, m_expectedNonce));
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
            std::bind(&dao::rdb::UserAuthentication::fetchSystemNonce,
                m_dao, _1, m_system.id));
    }

    void whenDeleteAllRecords()
    {
        using namespace std::placeholders;

        executeUpdateQuerySyncThrow(
            std::bind(&dao::rdb::UserAuthentication::deleteAccountAuthRecords,
                m_dao, _1, m_ownerAccount.id));
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
            std::bind(&dao::rdb::UserAuthentication::fetchAccountSystems,
                m_dao, _1, m_ownerAccount.id));

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
            std::bind(&dao::rdb::UserAuthentication::fetchAccountSystems,
                m_dao, _1, m_ownerAccount.id));
        for (const auto& system: systems)
        {
            auto fetchedAuthInfo = executeSelectQuerySyncThrow(
                std::bind(&dao::rdb::UserAuthentication::fetchUserAuthRecords,
                    m_dao, _1, system.systemId, m_ownerAccount.id));
            ASSERT_TRUE(fetchedAuthInfo.records.empty());
        }
    }

private:
    rdb::UserAuthentication m_dao;
    api::AccountData m_ownerAccount;
    api::SystemData m_system;
    api::AuthInfo m_expectedAuthInfo;
    api::AuthInfo m_fetchedAuthInfo;
    std::string m_expectedNonce;
    boost::optional<std::string> m_fetchedNonce;
    std::map<std::string, AbstractUserAuthentication::SystemInfo> m_expectedSystemInfo;

    void prepareTestData()
    {
        m_expectedAuthInfo = generateAuthInfo();
    }

    api::AuthInfo generateAuthInfo()
    {
        api::AuthInfo authInfo;
        authInfo.records.resize(1);
        authInfo.records[0].expirationTime =
            nx::utils::floor<std::chrono::milliseconds>(nx::utils::utcTime());
        authInfo.records[0].nonce = nx::utils::generateRandomName(7).toStdString();
        authInfo.records[0].intermediateResponse = 
            nx::utils::generateRandomName(14).toStdString();
        return authInfo;
    }
};

//-------------------------------------------------------------------------------------------------
// Test cases.

TEST_F(DaoRdbUserAuthentication, saved_user_auth_records_can_be_read_later)
{
    whenAddedAuthRecord();
    thenAuthRecordCanBeRead();
}

TEST_F(DaoRdbUserAuthentication, fetching_empty_auth_record_list)
{
    whenFetchedAuthRecords();
    thenRecordListIsEmpty();
}

TEST_F(DaoRdbUserAuthentication, inserting_system_nonce)
{
    whenInsertedSystemNonce();
    thenNonceCanBeFetched();
}

TEST_F(DaoRdbUserAuthentication, fetching_not_existing_nonce)
{
    whenFetchedSystemNonce();
    thenNoNonceFound();
}

TEST_F(DaoRdbUserAuthentication, replacing_system_nonce)
{
    whenReplacedExistingNonce();
    thenNonceCanBeFetched();
}

TEST_F(DaoRdbUserAuthentication, fetching_every_system_of_an_account)
{
    givenAccountWithMultipleSystems();
    thenEverySystemInformationCanBeSelected();
}

TEST_F(DaoRdbUserAuthentication, deleting_every_auth_record_of_an_account)
{
    givenAccountWithMultipleSystems();
    whenDeleteAllRecords();
    thenAllRecordsHaveBeenDeleted();
}

} // namespace test
} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
