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
                m_dao, _1, m_system.id, m_ownerAccount.id, m_authInfo));
    }

    void thenAuthRecordCanBeRead()
    {
        using namespace std::placeholders;

        api::AuthInfo authInfo = executeSelectQuerySyncThrow(
            std::bind(&dao::rdb::UserAuthentication::fetchUserAuthRecords,
                m_dao, _1, m_system.id, m_ownerAccount.id));

        ASSERT_EQ(m_authInfo, authInfo);
    }

private:
    rdb::UserAuthentication m_dao;
    api::AccountData m_ownerAccount;
    api::SystemData m_system;
    api::AuthInfo m_authInfo;

    void prepareTestData()
    {
        m_authInfo.records.resize(1);
        m_authInfo.records[0].expirationTime = 
            nx::utils::floor<std::chrono::milliseconds>(nx::utils::utcTime());
        m_authInfo.records[0].nonce = nx::utils::generateRandomName(7);
        m_authInfo.records[0].intermediateResponse = nx::utils::generateRandomName(14);
    }
};

//-------------------------------------------------------------------------------------------------
// Test cases.

TEST_F(DaoRdbUserAuthentication, saved_user_auth_records_can_be_read_later)
{
    whenAddedAuthRecord();
    thenAuthRecordCanBeRead();
}

} // namespace test
} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
