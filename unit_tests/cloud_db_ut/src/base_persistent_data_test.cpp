#include "base_persistent_data_test.h"

#include <gtest/gtest.h>

#include <nx/fusion/serialization/sql.h>
#include <nx/fusion/serialization/sql_functions.h>
#include <nx/utils/test_support/utils.h>

#include <nx/cloud/cdb/test_support/business_data_generator.h>

namespace nx {
namespace cdb {
namespace test {

using DbInstanceController = cdb::dao::rdb::DbInstanceController;
using AccountDataObject = cdb::dao::rdb::AccountDataObject;
using SystemDataObject = cdb::dao::rdb::SystemDataObject;
using SystemSharingDataObject = cdb::dao::rdb::SystemSharingDataObject;

DaoHelper::DaoHelper(const nx::utils::db::ConnectionOptions& dbConnectionOptions):
    m_dbConnectionOptions(dbConnectionOptions),
    m_systemDbController(m_settings)
{
}

void DaoHelper::initializeDatabase()
{
    m_persistentDbManager = std::make_unique<DbInstanceController>(
        m_dbConnectionOptions,
        m_dbVersionToUpdateTo);
}

const std::unique_ptr<DbInstanceController>& DaoHelper::persistentDbManager() const
{
    return m_persistentDbManager;
}

api::AccountData DaoHelper::insertRandomAccount()
{
    using namespace std::placeholders;

    auto account = cdb::test::BusinessDataGenerator::generateRandomAccount();
    const auto dbResult = executeUpdateQuerySync(
        std::bind(&AccountDataObject::insert, &m_accountDbController,
            _1, account));
    NX_GTEST_ASSERT_EQ(nx::utils::db::DBResult::ok, dbResult);
    m_accounts.push_back(account);
    return account;
}

api::SystemData DaoHelper::insertRandomSystem(const api::AccountData& account)
{
    auto system = cdb::test::BusinessDataGenerator::generateRandomSystem(account);
    insertSystem(account, system);
    return system;
}

void DaoHelper::insertSystem(const api::AccountData& account, const data::SystemData& system)
{
    using namespace std::placeholders;

    auto dbResult = executeUpdateQuerySync(
        std::bind(&SystemDataObject::insert, &m_systemDbController,
            _1, system, account.id));
    NX_GTEST_ASSERT_EQ(nx::utils::db::DBResult::ok, dbResult);

    api::SystemSharingEx sharing;
    sharing.systemId = system.id;
    sharing.accountId = account.id;
    sharing.accessRole = api::SystemAccessRole::owner;
    sharing.isEnabled = true;
    sharing.vmsUserId = guidFromArbitraryData(account.email).toSimpleByteArray().toStdString();
    dbResult = executeUpdateQuerySync(
        std::bind(&SystemSharingDataObject::insertOrReplaceSharing, &m_systemSharingController,
            _1, sharing));

    NX_GTEST_ASSERT_EQ(nx::utils::db::DBResult::ok, dbResult);
    m_systems.push_back(system);
}

void DaoHelper::insertSystemSharing(const api::SystemSharingEx& sharing)
{
    using namespace std::placeholders;

    const auto dbResult = executeUpdateQuerySync(
        std::bind(&SystemSharingDataObject::insertOrReplaceSharing,
            &m_systemSharingController, _1, sharing));
    ASSERT_EQ(nx::utils::db::DBResult::ok, dbResult);
}

void DaoHelper::deleteSystemSharing(const api::SystemSharingEx& sharing)
{
    using namespace std::placeholders;

    const nx::utils::db::InnerJoinFilterFields sqlFilter =
        {nx::utils::db::SqlFilterFieldEqual("account_id", ":accountId",
            QnSql::serialized_field(sharing.accountId))};

    const auto dbResult = executeUpdateQuerySync(
        std::bind(&SystemSharingDataObject::deleteSharing,
            &m_systemSharingController, _1, sharing.systemId, sqlFilter));
    ASSERT_EQ(nx::utils::db::DBResult::ok, dbResult);
}

const api::AccountData& DaoHelper::getAccount(std::size_t index) const
{
    return m_accounts[index];
}

const data::SystemData& DaoHelper::getSystem(std::size_t index) const
{
    return m_systems[index];
}

const std::vector<api::AccountData>& DaoHelper::accounts() const
{
    return m_accounts;
}

const std::vector<data::SystemData>& DaoHelper::systems() const
{
    return m_systems;
}

SystemSharingDataObject& DaoHelper::systemSharingDao()
{
    return m_systemSharingController;
}

dao::rdb::UserAuthentication& DaoHelper::userAuthentication()
{
    return m_userAuthentication;
}

void DaoHelper::setDbVersionToUpdateTo(unsigned int dbVersion)
{
    m_dbVersionToUpdateTo = dbVersion;
}

nx::utils::db::AsyncSqlQueryExecutor& DaoHelper::queryExecutor()
{
    return m_persistentDbManager->queryExecutor();
}

//-------------------------------------------------------------------------------------------------

BasePersistentDataTest::BasePersistentDataTest(DbInitializationType dbInitializationType):
    nx::utils::db::test::TestWithDbHelper("cdb", QString()),
    DaoHelper(dbConnectionOptions())
{
    if (dbInitializationType == DbInitializationType::immediate)
        initializeDatabase();
}

} // namespace test
} // namespace cdb
} // namespace nx
