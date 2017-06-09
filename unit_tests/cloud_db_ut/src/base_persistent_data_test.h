#pragma once

#include <nx/cloud/cdb/dao/rdb/db_instance_controller.h>
#include <nx/cloud/cdb/dao/rdb/account_data_object.h>
#include <nx/cloud/cdb/dao/rdb/system_data_object.h>
#include <nx/cloud/cdb/dao/rdb/system_sharing_data_object.h>

#include <utils/db/test_support/test_with_db_helper.h>

#include <nx/cloud/cdb/settings.h>

namespace nx {
namespace cdb {
namespace test {

class BasePersistentDataTest:
    public db::test::TestWithDbHelper
{
public:
    enum class DbInitializationType
    {
        immediate,
        delayed
    };

    BasePersistentDataTest(
        DbInitializationType dbInitializationType = DbInitializationType::immediate);

protected:
    const std::unique_ptr<dao::rdb::DbInstanceController>& persistentDbManager() const;
    api::AccountData insertRandomAccount();
    api::SystemData insertRandomSystem(const api::AccountData& account);

    void insertSystemSharing(const api::SystemSharingEx& sharing);
    void deleteSystemSharing(const api::SystemSharingEx& sharing);

    const api::AccountData& getAccount(std::size_t index) const;
    const data::SystemData& getSystem(std::size_t index) const;
    const std::vector<api::AccountData>& accounts() const;
    const std::vector<data::SystemData>& systems() const;
    dao::rdb::SystemSharingDataObject& systemSharingDao();
    void initializeDatabase();

    template<typename QueryFunc, typename... OutputData>
    nx::db::DBResult executeUpdateQuerySync(QueryFunc queryFunc)
    {
        nx::utils::promise<nx::db::DBResult> queryDonePromise;
        m_persistentDbManager->queryExecutor().executeUpdate(
            queryFunc,
            [&queryDonePromise](
                nx::db::QueryContext*,
                nx::db::DBResult dbResult,
                OutputData... outputData)
            {
                queryDonePromise.set_value(dbResult);
            });
        return queryDonePromise.get_future().get();
    }

    template<typename QueryFunc>
    nx::db::DBResult executeSelectQuerySync(QueryFunc queryFunc)
    {
        nx::utils::promise<nx::db::DBResult> queryDonePromise;
        m_persistentDbManager->queryExecutor().executeSelect(
            queryFunc,
            [&queryDonePromise](
                nx::db::QueryContext*,
                nx::db::DBResult dbResult)
            {
                queryDonePromise.set_value(dbResult);
            });
        return queryDonePromise.get_future().get();
    }

private:
    conf::Settings m_settings;
    std::unique_ptr<dao::rdb::DbInstanceController> m_persistentDbManager;
    dao::rdb::AccountDataObject m_accountDbController;
    dao::rdb::SystemDataObject m_systemDbController;
    dao::rdb::SystemSharingDataObject m_systemSharingController;
    std::vector<api::AccountData> m_accounts;
    std::vector<data::SystemData> m_systems;
};

} // namespace test
} // namespace cdb
} // namespace nx
