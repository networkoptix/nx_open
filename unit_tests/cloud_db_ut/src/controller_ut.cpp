#include <memory>

#include <gtest/gtest.h>

#include <nx/utils/db/async_sql_query_executor.h>
#include <nx/utils/db/test_support/test_with_db_helper.h>

#include <nx/cloud/cdb/controller.h>

#include "base_persistent_data_test.h"

namespace nx {
namespace cdb {
namespace test {

class Controller:
    public nx::utils::db::test::TestWithDbHelper,
    public ::testing::Test
{
public:
    Controller():
        nx::utils::db::test::TestWithDbHelper("cdb", QString())
    {
    }

protected:
    void givenDbBeforeIntroducingUserAuthRecords()
    {
        const int dbWithoutAuthRecordsVersion = 35;

        DaoHelper daoHelper(dbConnectionOptions());
        daoHelper.setDbVersionToUpdateTo(dbWithoutAuthRecordsVersion);
        daoHelper.initializeDatabase();
        m_account = daoHelper.insertRandomAccount();
        m_system  = daoHelper.insertRandomSystem(m_account);
    }

    void whenMigratedData()
    {
        const std::string dbName = dbConnectionOptions().dbName.toStdString();

        std::array<const char*, 4> args = {
            "-db/driverName", "QSQLITE",
            "-db/name", dbName.c_str()
        };
        m_settings.load(static_cast<int>(args.size()), args.data());
        m_controller = std::make_unique<cdb::Controller>(m_settings);
        m_controller.reset();
    }

    void thenAuthRecordsHasBeenAddedToExistingUsers()
    {
        using namespace std::placeholders;

        DaoHelper daoHelper(dbConnectionOptions());
        daoHelper.initializeDatabase();

        const api::AuthInfo authInfo = daoHelper.queryExecutor().executeSelectQuerySync(
            std::bind(&dao::rdb::UserAuthentication::fetchUserAuthRecords,
                &daoHelper.userAuthentication(), _1, m_system.id, m_account.id));
        ASSERT_FALSE(authInfo.records.empty());
    }

private:
    conf::Settings m_settings;
    std::unique_ptr<cdb::Controller> m_controller;
    api::AccountData m_account;
    api::SystemData m_system;
};

TEST_F(Controller, migration_to_offline_login_support)
{
    givenDbBeforeIntroducingUserAuthRecords();
    whenMigratedData();
    thenAuthRecordsHasBeenAddedToExistingUsers();
}

} // namespace test
} // namespace cdb
} // namespace nx
