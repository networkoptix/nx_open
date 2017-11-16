#pragma once

#include <type_traits>

#include <nx/cloud/cdb/dao/rdb/rdb_account_data_object.h>
#include <nx/cloud/cdb/dao/rdb/db_instance_controller.h>
#include <nx/cloud/cdb/dao/rdb/system_data_object.h>
#include <nx/cloud/cdb/dao/rdb/system_sharing_data_object.h>
#include <nx/cloud/cdb/dao/rdb/dao_rdb_user_authentication.h>

#include <nx/utils/db/test_support/test_with_db_helper.h>

#include <nx/cloud/cdb/settings.h>

namespace nx {
namespace cdb {
namespace test {

class DaoHelper
{
public:
    DaoHelper(const nx::utils::db::ConnectionOptions& dbConnectionOptions);

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

    nx::utils::db::AsyncSqlQueryExecutor& queryExecutor();

protected:
    const std::unique_ptr<dao::rdb::DbInstanceController>& persistentDbManager() const;

    template<typename QueryFunc, typename... OutputData>
    nx::utils::db::DBResult executeUpdateQuerySync(QueryFunc queryFunc)
    {
        nx::utils::promise<nx::utils::db::DBResult> queryDonePromise;
        m_persistentDbManager->queryExecutor().executeUpdate(
            queryFunc,
            [&queryDonePromise](
                nx::utils::db::QueryContext*,
                nx::utils::db::DBResult dbResult,
                OutputData... outputData)
            {
                queryDonePromise.set_value(dbResult);
            });
        return queryDonePromise.get_future().get();
    }

    template<typename QueryFunc>
    nx::utils::db::DBResult executeSelectQuerySync(QueryFunc queryFunc)
    {
        nx::utils::promise<nx::utils::db::DBResult> queryDonePromise;
        m_persistentDbManager->queryExecutor().executeSelect(
            queryFunc,
            [&queryDonePromise](
                nx::utils::db::QueryContext*,
                nx::utils::db::DBResult dbResult)
            {
                queryDonePromise.set_value(dbResult);
            });
        return queryDonePromise.get_future().get();
    }

    /**
     * @param queryFunc throws nx::utils::db::Exception.
     */
    template<typename QueryFunc, typename... OutputData>
    void executeUpdateQuerySyncThrow(QueryFunc queryFunc)
    {
        m_persistentDbManager->queryExecutor()
            .executeUpdateQuerySync(std::move(queryFunc));
    }

    /**
     * @param queryFunc throws nx::utils::db::Exception.
     */
    template<typename QueryFunc>
    typename std::result_of<QueryFunc(nx::utils::db::QueryContext*)>::type
        executeSelectQuerySyncThrow(QueryFunc queryFunc)
    {
        return m_persistentDbManager->queryExecutor()
            .executeSelectQuerySync(std::move(queryFunc));
    }

private:
    nx::utils::db::ConnectionOptions m_dbConnectionOptions;
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
    public nx::utils::db::test::TestWithDbHelper,
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
} // namespace cdb
} // namespace nx
