#pragma once

#include <type_traits>

#include <nx/cloud/db/dao/rdb/rdb_account_data_object.h>
#include <nx/cloud/db/dao/rdb/db_instance_controller.h>
#include <nx/cloud/db/dao/rdb/system_data_object.h>
#include <nx/cloud/db/dao/rdb/system_sharing_data_object.h>
#include <nx/cloud/db/dao/rdb/dao_rdb_user_authentication.h>

#include <nx/sql/test_support/test_with_db_helper.h>

#include <nx/cloud/db/settings.h>

namespace nx::cloud::db {
namespace test {

class DaoHelper
{
public:
    DaoHelper(const nx::sql::ConnectionOptions& dbConnectionOptions);

    void initializeDatabase();

    api::AccountData insertRandomAccount();
    api::SystemData insertRandomSystem(const api::AccountData& account);
    void insertSystem(const api::AccountData& account, const data::SystemData& system);

    void insertSystemSharing(const api::SystemSharingEx& sharing);
    void deleteSystemSharing(const api::SystemSharingEx& sharing);

    const api::AccountData& getAccount(std::size_t index) const;
    const data::SystemData& getSystem(std::size_t index) const;
    const std::vector<api::AccountData>& accounts() const;
    const std::vector<data::SystemData>& systems() const;
    dao::rdb::SystemSharingDataObject& systemSharingDao();
    dao::rdb::UserAuthentication& userAuthentication();

    void setDbVersionToUpdateTo(unsigned int dbVersion);

    nx::sql::AsyncSqlQueryExecutor& queryExecutor();

protected:
    const std::unique_ptr<dao::rdb::DbInstanceController>& persistentDbManager() const;

    template<typename QueryFunc, typename... OutputData>
    nx::sql::DBResult executeUpdateQuerySync(QueryFunc queryFunc)
    {
        nx::utils::promise<nx::sql::DBResult> queryDonePromise;
        m_persistentDbManager->queryExecutor().executeUpdate(
            queryFunc,
            [&queryDonePromise](
                nx::sql::DBResult dbResult,
                OutputData... /*outputData*/)
            {
                queryDonePromise.set_value(dbResult);
            });
        return queryDonePromise.get_future().get();
    }

    template<typename QueryFunc>
    nx::sql::DBResult executeSelectQuerySync(QueryFunc queryFunc)
    {
        nx::utils::promise<nx::sql::DBResult> queryDonePromise;
        m_persistentDbManager->queryExecutor().executeSelect(
            queryFunc,
            [&queryDonePromise](nx::sql::DBResult dbResult)
            {
                queryDonePromise.set_value(dbResult);
            });
        return queryDonePromise.get_future().get();
    }

    /**
     * @param queryFunc throws nx::sql::Exception.
     */
    template<typename QueryFunc, typename... OutputData>
    void executeUpdateQuerySyncThrow(QueryFunc queryFunc)
    {
        m_persistentDbManager->queryExecutor()
            .executeUpdateQuerySync(std::move(queryFunc));
    }

    /**
     * @param queryFunc throws nx::sql::Exception.
     */
    template<typename QueryFunc>
    typename std::result_of<QueryFunc(nx::sql::QueryContext*)>::type
        executeSelectQuerySyncThrow(QueryFunc queryFunc)
    {
        return m_persistentDbManager->queryExecutor()
            .executeSelectQuerySync(std::move(queryFunc));
    }

private:
    nx::sql::ConnectionOptions m_dbConnectionOptions;
    conf::Settings m_settings;
    std::unique_ptr<dao::rdb::DbInstanceController> m_persistentDbManager;
    dao::rdb::AccountDataObject m_accountDbController;
    dao::rdb::SystemDataObject m_systemDbController;
    dao::rdb::SystemSharingDataObject m_systemSharingController;
    dao::rdb::UserAuthentication m_userAuthentication;
    std::vector<api::AccountData> m_accounts;
    std::vector<data::SystemData> m_systems;
    boost::optional<unsigned int> m_dbVersionToUpdateTo;
};

//-------------------------------------------------------------------------------------------------

class BasePersistentDataTest:
    public nx::sql::test::TestWithDbHelper,
    public DaoHelper
{
public:
    enum class DbInitializationType
    {
        immediate,
        delayed
    };

    BasePersistentDataTest(
        DbInitializationType dbInitializationType = DbInitializationType::immediate);
};

} // namespace test
} // namespace nx::cloud::db
