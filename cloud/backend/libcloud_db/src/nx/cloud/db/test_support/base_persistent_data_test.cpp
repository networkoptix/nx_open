#include "base_persistent_data_test.h"

#include <nx/fusion/serialization/sql.h>
#include <nx/fusion/serialization/sql_functions.h>
#include <nx/utils/test_support/utils.h>

#include <utils/common/id.h>

#include "business_data_generator.h"

namespace nx::cloud::db {
namespace test {

using DbInstanceController = nx::cloud::db::dao::rdb::DbInstanceController;
using AccountDataObject = nx::cloud::db::dao::rdb::AccountDataObject;
using SystemDataObject = nx::cloud::db::dao::rdb::SystemDataObject;
using SystemSharingDataObject = nx::cloud::db::dao::rdb::SystemSharingDataObject;

DaoHelper::DaoHelper(const nx::sql::ConnectionOptions& dbConnectionOptions):
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

    auto account = nx::cloud::db::test::BusinessDataGenerator::generateRandomAccount();
    executeUpdateQuerySyncThrow(
        std::bind(&AccountDataObject::insert, &m_accountDbController,
            _1, account));
    m_accounts.push_back(account);
    return account;
}

api::SystemData DaoHelper::insertRandomSystem(const api::AccountData& account)
{
    auto system = nx::cloud::db::test::BusinessDataGenerator::generateRandomSystem(account);
    insertSystem(account, system);
    return system;
}

void DaoHelper::insertSystem(const api::AccountData& account, const data::SystemData& system)
{
    using namespace std::placeholders;

    executeUpdateQuerySyncThrow(
        std::bind(&SystemDataObject::insert, &m_systemDbController,
            _1, system, account.id));

    api::SystemSharingEx sharing;
    sharing.systemId = system.id;
    sharing.accountId = account.id;
    sharing.accessRole = api::SystemAccessRole::owner;
    sharing.isEnabled = true;
    sharing.vmsUserId = guidFromArbitraryData(account.email).toSimpleByteArray().toStdString();
    executeUpdateQuerySyncThrow(
        std::bind(&SystemSharingDataObject::insertOrReplaceSharing, &m_systemSharingController,
            _1, sharing));

    m_systems.push_back(system);
}

void DaoHelper::insertSystemSharing(const api::SystemSharingEx& sharing)
{
    using namespace std::placeholders;

    executeUpdateQuerySyncThrow(
        std::bind(&SystemSharingDataObject::insertOrReplaceSharing,
            &m_systemSharingController, _1, sharing));
}

void DaoHelper::deleteSystemSharing(const api::SystemSharingEx& sharing)
{
    using namespace std::placeholders;

    const nx::sql::InnerJoinFilterFields sqlFilter =
        {nx::sql::SqlFilterFieldEqual("account_id", ":accountId",
            QnSql::serialized_field(sharing.accountId))};

    executeUpdateQuerySyncThrow(
        std::bind(&SystemSharingDataObject::deleteSharing,
            &m_systemSharingController, _1, sharing.systemId, sqlFilter));
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

nx::sql::AsyncSqlQueryExecutor& DaoHelper::queryExecutor()
{
    return m_persistentDbManager->queryExecutor();
}

//-------------------------------------------------------------------------------------------------

BasePersistentDataTest::BasePersistentDataTest(DbInitializationType dbInitializationType):
    nx::sql::test::TestWithDbHelper("cdb", QString()),
    DaoHelper(dbConnectionOptions())
{
    if (dbInitializationType == DbInitializationType::immediate)
        initializeDatabase();
}

} // namespace test
} // namespace nx::cloud::db
