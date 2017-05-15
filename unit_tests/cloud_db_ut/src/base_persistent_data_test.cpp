#include "base_persistent_data_test.h"

#include <gtest/gtest.h>

#include <nx/fusion/serialization/sql.h>
#include <nx/fusion/serialization/sql_functions.h>
#include <nx/utils/test_support/utils.h>

#include <test_support/business_data_generator.h>

namespace nx {
namespace cdb {
namespace test {

using DbInstanceController = cdb::dao::rdb::DbInstanceController;
using AccountDataObject = cdb::dao::rdb::AccountDataObject;
using SystemDataObject = cdb::dao::rdb::SystemDataObject;
using SystemSharingDataObject = cdb::dao::rdb::SystemSharingDataObject;

BasePersistentDataTest::BasePersistentDataTest(DbInitializationType dbInitializationType):
    db::test::TestWithDbHelper("cdb", QString()),
    m_systemDbController(m_settings)
{
    if (dbInitializationType == DbInitializationType::immediate)
        initializeDatabase();
}

const std::unique_ptr<DbInstanceController>& BasePersistentDataTest::persistentDbManager() const
{
    return m_persistentDbManager;
}

api::AccountData BasePersistentDataTest::insertRandomAccount()
{
    using namespace std::placeholders;

    auto account = cdb::test::BusinessDataGenerator::generateRandomAccount();
    const auto dbResult = executeUpdateQuerySync(
        std::bind(&AccountDataObject::insert, &m_accountDbController,
            _1, account));
    NX_GTEST_ASSERT_EQ(nx::db::DBResult::ok, dbResult);
    m_accounts.push_back(account);
    return account;
}

api::SystemData BasePersistentDataTest::insertRandomSystem(const api::AccountData& account)
{
    using namespace std::placeholders;

    auto system = cdb::test::BusinessDataGenerator::generateRandomSystem(account);
    const auto dbResult = executeUpdateQuerySync(
        std::bind(&SystemDataObject::insert, &m_systemDbController,
            _1, system, account.id));
    NX_GTEST_ASSERT_EQ(nx::db::DBResult::ok, dbResult);
    m_systems.push_back(system);
    return system;
}

void BasePersistentDataTest::insertSystemSharing(const api::SystemSharingEx& sharing)
{
    using namespace std::placeholders;

    const auto dbResult = executeUpdateQuerySync(
        std::bind(&SystemSharingDataObject::insertOrReplaceSharing,
            &m_systemSharingController, _1, sharing));
    ASSERT_EQ(nx::db::DBResult::ok, dbResult);
}

void BasePersistentDataTest::deleteSystemSharing(const api::SystemSharingEx& sharing)
{
    const nx::db::InnerJoinFilterFields sqlFilter =
        {{"account_id", ":accountId",
           QnSql::serialized_field(sharing.accountId)}};

    using namespace std::placeholders;

    const auto dbResult = executeUpdateQuerySync(
        std::bind(&SystemSharingDataObject::deleteSharing,
            &m_systemSharingController, _1, sharing.systemId, sqlFilter));
    ASSERT_EQ(nx::db::DBResult::ok, dbResult);
}

const api::AccountData& BasePersistentDataTest::getAccount(std::size_t index) const
{
    return m_accounts[index];
}

const data::SystemData& BasePersistentDataTest::getSystem(std::size_t index) const
{
    return m_systems[index];
}

const std::vector<api::AccountData>& BasePersistentDataTest::accounts() const
{
    return m_accounts;
}

const std::vector<data::SystemData>& BasePersistentDataTest::systems() const
{
    return m_systems;
}

SystemSharingDataObject& BasePersistentDataTest::systemSharingDao()
{
    return m_systemSharingController;
}

void BasePersistentDataTest::initializeDatabase()
{
    m_persistentDbManager =
        std::make_unique<DbInstanceController>(dbConnectionOptions());
    ASSERT_TRUE(m_persistentDbManager->initialize());
}

} // namespace test
} // namespace cdb
} // namespace nx
