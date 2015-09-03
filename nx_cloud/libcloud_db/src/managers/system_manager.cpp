/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#include "system_manager.h"

#include <QtSql/QSqlQuery>

#include <utils/common/log.h>
#include <utils/serialization/sql.h>
#include <utils/serialization/sql_functions.h>

#include "access_control/authorization_manager.h"
#include "data/cdb_ns.h"


namespace nx {
namespace cdb {

SystemManager::SystemManager(nx::db::DBManager* const dbManager) throw(std::runtime_error)
:
    m_dbManager(dbManager)
{
    //pre-filling cache
    if (fillCache() != db::DBResult::ok)
        throw std::runtime_error("Failed to pre-load systems cache");
}

void SystemManager::bindSystemToAccount(
    const AuthorizationInfo& authzInfo,
    data::SystemRegistrationData registrationData,
    std::function<void(api::ResultCode, data::SystemData)> completionHandler)
{
    QnUuid accountID;
    if (!authzInfo.get(cdb::param::accountID, &accountID))
    {
        completionHandler(api::ResultCode::notAuthorized, data::SystemData());
        return;
    }

    data::SystemRegistrationDataWithAccountID registrationDataWithAccountID(
        std::move(registrationData));
    registrationDataWithAccountID.accountID = std::move(accountID);

    using namespace std::placeholders;
    m_dbManager->executeUpdate<data::SystemRegistrationDataWithAccountID, data::SystemData>(
        std::bind(&SystemManager::insertSystemToDB, this, _1, _2, _3),
        std::move(registrationDataWithAccountID),
        std::bind(&SystemManager::systemAdded, this, _1, _2, _3, std::move(completionHandler)));
}

void SystemManager::unbindSystem(
    const AuthorizationInfo& authzInfo,
    data::SystemID systemID,
    std::function<void(api::ResultCode)> completionHandler)
{
    using namespace std::placeholders;
    m_dbManager->executeUpdate<data::SystemID>(
        std::bind(&SystemManager::deleteSystemFromDB, this, _1, _2),
        std::move(systemID),
        std::bind(&SystemManager::systemDeleted, this, _1, _2, std::move(completionHandler)));
}

namespace {
//!Returns \a true, if \a record contains every single resource present in \a filter 
bool applyFilter(
    const stree::AbstractResourceReader& record,
    const stree::AbstractIteratableContainer& filter)
{
    //TODO #ak this method should be moved to stree and have linear complexity, not n*log(n)!
    for (auto it = filter.begin(); !it->atEnd(); it->next())
    {
        auto val = record.get(it->resID());
        if (!val || val.get() != it->value())
            return false;
    }

    return true;
}
}

void SystemManager::getSystems(
    const AuthorizationInfo& authzInfo,
    DataFilter filter,
    std::function<void(api::ResultCode, data::SystemDataList)> completionHandler )
{
    stree::MultiIteratableResourceReader wholeFilter(filter, authzInfo);
    stree::MultiSourceResourceReader wholeFilterMap(filter, authzInfo);

    data::SystemDataList resultData;
    if (auto systemID = wholeFilterMap.get(cdb::param::systemID))
    {
        //selecting system by id
        auto system = m_cache.find(systemID.get().value<QnUuid>());
        if (!system || !applyFilter(system.get(), wholeFilter))
            return completionHandler(api::ResultCode::notFound, resultData);
        resultData.systems.emplace_back(std::move(system.get()));
    }
    //    else if(auto accountID = wholeFilterMap.get<QnUuid>(cdb::param::accountID)) 
    //        ;//TODO #ak effectively selecting systems with specified account id
    else
    {
        //filtering full system list
        m_cache.forEach(
            [&](const std::pair<const QnUuid, data::SystemData>& data) {
                if (applyFilter(data.second, wholeFilter))
                    resultData.systems.push_back(data.second);
            });
    }

    completionHandler(
        api::ResultCode::ok,
        resultData);
}

void SystemManager::shareSystem(
    const AuthorizationInfo& authzInfo,
    data::SystemSharing sharingData,
    std::function<void(api::ResultCode)> completionHandler)
{
    using namespace std::placeholders;
    m_dbManager->executeUpdate<data::SystemSharing>(
        std::bind(&SystemManager::insertSystemSharingToDB, this, _1, _2),
        std::move(sharingData),
        std::bind(&SystemManager::systemSharingAdded, this, _1, _2, std::move(completionHandler)));
}

boost::optional<data::SystemData> SystemManager::findSystemByID(const QnUuid& id) const
{
    return m_cache.find(id);
}

api::SystemAccessRole::Value SystemManager::getAccountRightsForSystem(
    const QnUuid& accountID, const QnUuid& systemID) const
{
    //TODO #ak
    return api::SystemAccessRole::none;
}

nx::db::DBResult SystemManager::insertSystemToDB(
    QSqlDatabase* const connection,
    const data::SystemRegistrationDataWithAccountID& newSystem,
    data::SystemData* const systemData)
{
    QSqlQuery insertSystemQuery(*connection);
    insertSystemQuery.prepare(
        "INSERT INTO system( id, name, auth_key, owner_account_id, status_code ) "
        " VALUES( :id, :name, :authKey, :ownerAccountID, :status )");
    systemData->id = QnUuid::createUuid();
    systemData->name = newSystem.name;
    systemData->authKey = QnUuid::createUuid().toStdString();
    systemData->ownerAccountID = newSystem.accountID;
    systemData->status = api::SystemStatus::ssNotActivated;
    QnSql::bind(*systemData, &insertSystemQuery);
    if (!insertSystemQuery.exec())
    {
        NX_LOG(lit("Could not insert system %1 into DB. %2").
            arg(QString::fromStdString(newSystem.name)).arg(connection->lastError().text()), cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    //adding binding system -> account
    QSqlQuery insertSystemToAccountBinding(*connection);
    insertSystemToAccountBinding.prepare(
        "INSERT INTO system_to_account( account_id, system_id, access_role_id ) "
        " VALUES( :accountID, :systemID, :accessRole )");
    insertSystemToAccountBinding.bindValue(":accountID", QnSql::serialized_field(newSystem.accountID));
    insertSystemToAccountBinding.bindValue(":systemID", QnSql::serialized_field(systemData->id));
    insertSystemToAccountBinding.bindValue(":accessRole", api::SystemAccessRole::owner);
    if (!insertSystemToAccountBinding.exec())
    {
        NX_LOG(lit("Could not insert system %1 to account %2 binding into DB. %3").
            arg(QString::fromStdString(newSystem.name)).arg(newSystem.accountID.toString()).
            arg(connection->lastError().text()), cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    return nx::db::DBResult::ok;
}

void SystemManager::systemAdded(
    nx::db::DBResult dbResult,
    data::SystemRegistrationDataWithAccountID systemRegistrationData,
    data::SystemData systemData,
    std::function<void(api::ResultCode, data::SystemData)> completionHandler)
{
    if (dbResult == nx::db::DBResult::ok)
    {
        //updating cache
        m_cache.insert(systemData.id, systemData);
        //TODO #ak updating "systems by account id" index
    }
    completionHandler(
        dbResult == nx::db::DBResult::ok
        ? api::ResultCode::ok
        : api::ResultCode::dbError,
        std::move(systemData));
}

nx::db::DBResult SystemManager::insertSystemSharingToDB(
    QSqlDatabase* const connection,
    const data::SystemSharing& systemSharing)
{
    QSqlQuery insertSystemToAccountBinding(*connection);
    insertSystemToAccountBinding.prepare(
        "INSERT INTO system_to_account( account_id, system_id, access_role_id ) "
        " VALUES( :accountID, :systemID, :accessRole )");
    QnSql::bind(systemSharing, &insertSystemToAccountBinding);
    if (!insertSystemToAccountBinding.exec())
    {
        NX_LOG(lit("Could not insert system %1 to account %2 binding into DB. %3").
            arg(systemSharing.systemID.toString()).
            arg(systemSharing.accountID.toString()).
            arg(connection->lastError().text()), cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    return nx::db::DBResult::ok;
}

void SystemManager::systemSharingAdded(
    nx::db::DBResult dbResult,
    data::SystemSharing sytemSharing,
    std::function<void(api::ResultCode)> completionHandler)
{
    if (dbResult == nx::db::DBResult::ok)
    {
        //TODO #ak updating "systems by account id" index
    }
    completionHandler(
        dbResult == nx::db::DBResult::ok
        ? api::ResultCode::ok
        : api::ResultCode::dbError);
}

nx::db::DBResult SystemManager::deleteSystemFromDB(
    QSqlDatabase* const connection,
    const data::SystemID& systemID)
{
    QSqlQuery removeSystemToAccountBinding(*connection);
    removeSystemToAccountBinding.prepare(
        "DELETE FROM system_to_account WHERE system_id=:id");
    QnSql::bind(systemID, &removeSystemToAccountBinding);
    if (!removeSystemToAccountBinding.exec())
    {
        NX_LOG(lit("Could not delete system %1 from system_to_account. %2").
            arg(systemID.id.toString()).arg(connection->lastError().text()), cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    QSqlQuery removeSystem(*connection);
    removeSystem.prepare(
        "DELETE FROM system WHERE id=:id");
    QnSql::bind(systemID, &removeSystem);
    if (!removeSystem.exec())
    {
        NX_LOG(lit("Could not delete system %1. %2").
            arg(systemID.id.toString()).arg(connection->lastError().text()), cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    return nx::db::DBResult::ok;
}

void SystemManager::systemDeleted(
    nx::db::DBResult dbResult,
    data::SystemID systemID,
    std::function<void(api::ResultCode)> completionHandler)
{
    if (dbResult == nx::db::DBResult::ok)
    {
        m_cache.erase(systemID.id);
        //TODO #ak updating "systems by account id" index
    }
    completionHandler(
        dbResult == nx::db::DBResult::ok
        ? api::ResultCode::ok
        : api::ResultCode::dbError);
}

nx::db::DBResult SystemManager::fillCache()
{
    std::promise<db::DBResult> cacheFilledPromise;
    auto future = cacheFilledPromise.get_future();

    //starting async operation
    using namespace std::placeholders;
    m_dbManager->executeSelect<int>(
        std::bind(&SystemManager::fetchSystems, this, _1, _2),
        [&cacheFilledPromise](db::DBResult dbResult, int /*dummy*/) {
        cacheFilledPromise.set_value(dbResult);
    });

    //TODO #ak fetching system_to_account binding

    //waiting for completion
    future.wait();
    return future.get();
}

nx::db::DBResult SystemManager::fetchSystems(QSqlDatabase* connection, int* const /*dummy*/)
{
    QSqlQuery readSystemsQuery(*connection);
    readSystemsQuery.prepare(
        "SELECT id, name, auth_key as authKey, "
        "owner_account_id as ownerAccountID, status_code as status "
        "FROM system");
    if (!readSystemsQuery.exec())
    {
        NX_LOG(lit("Failed to read system list from DB. %1").
            arg(connection->lastError().text()), cl_logWARNING);
        return db::DBResult::ioError;
    }

    std::vector<data::SystemData> systems;
    QnSql::fetch_many(readSystemsQuery, &systems);

    for (auto& system : systems)
    {
        auto idCopy = system.id;
        if (!m_cache.insert(std::move(idCopy), std::move(system)))
        {
            assert(false);
        }

        //TODO #ak other indexes may be required also (e.g., systems by account, systems by name, etc..)
    }

    return db::DBResult::ok;
}

}   //cdb
}   //nx
