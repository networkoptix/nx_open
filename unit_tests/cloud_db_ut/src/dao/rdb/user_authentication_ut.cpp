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
        ASSERT_EQ(m_expectedNonce, m_fetchedNonce);
    }

private:
    rdb::UserAuthentication m_dao;
    api::AccountData m_ownerAccount;
    api::SystemData m_system;
    api::AuthInfo m_expectedAuthInfo;
    api::AuthInfo m_fetchedAuthInfo;
    std::string m_expectedNonce;
    std::string m_fetchedNonce;

    void prepareTestData()
    {
        m_expectedAuthInfo.records.resize(1);
        m_expectedAuthInfo.records[0].expirationTime = 
            nx::utils::floor<std::chrono::milliseconds>(nx::utils::utcTime());
        m_expectedAuthInfo.records[0].nonce = nx::utils::generateRandomName(7).toStdString();
        m_expectedAuthInfo.records[0].intermediateResponse = nx::utils::generateRandomName(14).toStdString();
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
    ASSERT_THROW(whenFetchedSystemNonce(), nx::db::Exception);
}

TEST_F(DaoRdbUserAuthentication, replacing_system_nonce)
{
    whenReplacedExistingNonce();
    thenNonceCanBeFetched();
}

} // namespace test
} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
