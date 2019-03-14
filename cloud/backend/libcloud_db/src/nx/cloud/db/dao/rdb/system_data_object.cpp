#include "system_data_object.h"

#include <chrono>

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>
#include <nx/utils/time.h>

#include "../../settings.h"

namespace nx::cloud::db {
namespace dao {
namespace rdb {

SystemDataObject::SystemDataObject(const conf::Settings& settings):
    m_settings(settings)
{
}

nx::sql::DBResult SystemDataObject::insert(
    nx::sql::QueryContext* const queryContext,
    const data::SystemData& system,
    const std::string& accountId)
{
    QSqlQuery insertSystemQuery(*queryContext->connection()->qtSqlConnection());
    insertSystemQuery.prepare(
        R"sql(
        INSERT INTO system(
                id, name, customization, auth_key, owner_account_id,
                status_code, expiration_utc_timestamp, opaque, registration_time_utc)
        VALUES(:id, :name, :customization, :authKey, :ownerAccountID,
               :status, :expirationTimeUtc, :opaque, :registrationTime)
        )sql");

    QnSql::bind(system, &insertSystemQuery);
    insertSystemQuery.bindValue(":ownerAccountID", QnSql::serialized_field(accountId));
    insertSystemQuery.bindValue(":expirationTimeUtc", system.expirationTimeUtc);
    if (!insertSystemQuery.exec())
    {
        NX_DEBUG(this, lm("Could not insert system %1 (%2) into DB. %3")
            .arg(system.name).arg(system.id).arg(insertSystemQuery.lastError().text()));
        return nx::sql::DBResult::ioError;
    }

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult SystemDataObject::selectSystemSequence(
    nx::sql::QueryContext* const queryContext,
    const std::string& systemId,
    std::uint64_t* const sequence)
{
    QSqlQuery selectSystemSequence(*queryContext->connection()->qtSqlConnection());
    selectSystemSequence.setForwardOnly(true);
    selectSystemSequence.prepare(
        R"sql(
        SELECT seq FROM system WHERE id=?
        )sql");
    selectSystemSequence.bindValue(0, QnSql::serialized_field(systemId));
    if (!selectSystemSequence.exec() || !selectSystemSequence.next())
    {
        NX_DEBUG(this, lm("Error selecting sequence of system %1. %2")
            .arg(systemId).arg(selectSystemSequence.lastError().text()));
        return nx::sql::DBResult::ioError;
    }
    *sequence = selectSystemSequence.value(0).toULongLong();

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult SystemDataObject::markSystemForDeletion(
    nx::sql::QueryContext* const queryContext,
    const std::string& systemId)
{
    QSqlQuery markSystemAsRemoved(*queryContext->connection()->qtSqlConnection());
    markSystemAsRemoved.prepare(
        R"sql(
        UPDATE system
        SET status_code=:statusCode,
            expiration_utc_timestamp=:expiration_utc_timestamp
        WHERE id=:id
        )sql");
    markSystemAsRemoved.bindValue(
        ":statusCode",
        QnSql::serialized_field(static_cast<int>(api::SystemStatus::deleted_)));
    markSystemAsRemoved.bindValue(
        ":expiration_utc_timestamp",
        (int)(nx::utils::timeSinceEpoch().count() +
            std::chrono::duration_cast<std::chrono::seconds>(
                m_settings.systemManager().reportRemovedSystemPeriod).count()));
    markSystemAsRemoved.bindValue(
        ":id",
        QnSql::serialized_field(systemId));
    if (!markSystemAsRemoved.exec())
    {
        NX_DEBUG(this, lm("Error marking system %1 as deleted. %2")
            .arg(systemId).arg(markSystemAsRemoved.lastError().text()));
        return nx::sql::DBResult::ioError;
    }

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult SystemDataObject::deleteSystem(
    nx::sql::QueryContext* const queryContext,
    const std::string& systemId)
{
    QSqlQuery removeSystem(*queryContext->connection()->qtSqlConnection());
    removeSystem.prepare("DELETE FROM system WHERE id=:systemId");
    QnSql::bind(systemId, &removeSystem);
    if (!removeSystem.exec())
    {
        NX_DEBUG(this, lm("Could not delete system %1. %2")
            .arg(systemId).arg(removeSystem.lastError().text()));
        return nx::sql::DBResult::ioError;
    }

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult SystemDataObject::execSystemNameUpdate(
    nx::sql::QueryContext* const queryContext,
    const data::SystemAttributesUpdate& data)
{
    QSqlQuery updateSystemNameQuery(*queryContext->connection()->qtSqlConnection());
    updateSystemNameQuery.prepare(
        "UPDATE system "
        "SET name=:name "
        "WHERE id=:systemId");
    updateSystemNameQuery.bindValue(":name", QnSql::serialized_field(data.name.get()));
    updateSystemNameQuery.bindValue(":systemId", QnSql::serialized_field(data.systemId));
    if (!updateSystemNameQuery.exec())
    {
        NX_WARNING(this, lm("Failed to update system %1 name in DB to %2. %3")
            .arg(data.systemId).arg(data.name.get())
            .arg(updateSystemNameQuery.lastError().text()));
        return nx::sql::DBResult::ioError;
    }

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult SystemDataObject::execSystemOpaqueUpdate(
    nx::sql::QueryContext* const queryContext,
    const data::SystemAttributesUpdate& data)
{
    // TODO: #ak: this is a copy-paste of a previous method. Refactor!

    QSqlQuery updateSystemOpaqueQuery(*queryContext->connection()->qtSqlConnection());
    updateSystemOpaqueQuery.prepare(
        "UPDATE system "
        "SET opaque=:opaque "
        "WHERE id=:systemId");
    updateSystemOpaqueQuery.bindValue(":opaque", QnSql::serialized_field(data.opaque.get()));
    updateSystemOpaqueQuery.bindValue(":systemId", QnSql::serialized_field(data.systemId));
    if (!updateSystemOpaqueQuery.exec())
    {
        NX_WARNING(this, lm("Error updating system %1. %2")
            .arg(data.systemId).arg(updateSystemOpaqueQuery.lastError().text()));
        return nx::sql::DBResult::ioError;
    }

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult SystemDataObject::updateSystemStatus(
    nx::sql::QueryContext* const queryContext,
    const std::string& systemId,
    api::SystemStatus systemStatus)
{
    QSqlQuery updateSystemStatusQuery(*queryContext->connection()->qtSqlConnection());
    updateSystemStatusQuery.prepare(
        "UPDATE system "
        "SET status_code=:statusCode, expiration_utc_timestamp=:expirationTimeUtc "
        "WHERE id=:id");
    updateSystemStatusQuery.bindValue(
        ":statusCode",
        QnSql::serialized_field(static_cast<int>(systemStatus)));
    updateSystemStatusQuery.bindValue(
        ":id",
        QnSql::serialized_field(systemId));
    updateSystemStatusQuery.bindValue(
        ":expirationTimeUtc",
        std::numeric_limits<int>::max());
    if (!updateSystemStatusQuery.exec())
    {
        NX_WARNING(this, lm("Failed to update system %1 status to %2 from DB. %3")
            .args(systemId, QnLexical::serialized(systemStatus),
                updateSystemStatusQuery.lastError().text()));
        return nx::sql::DBResult::ioError;
    }

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult SystemDataObject::fetchSystems(
    nx::sql::QueryContext* queryContext,
    const nx::sql::InnerJoinFilterFields& filterFields,
    std::vector<data::SystemData>* const systems)
{
    constexpr const char kSelectAllSystemsQuery[] =
        R"sql(
        SELECT system.id, system.name, system.customization, system.auth_key as authKey,
               account.email as ownerAccountEmail, system.status_code as status,
               system.expiration_utc_timestamp as expirationTimeUtc,
               system.seq as systemSequence, system.opaque as opaque,
               system.registration_time_utc as registrationTime
        FROM system, account
        WHERE system.owner_account_id = account.id
        )sql";

    std::string sqlQueryStr = kSelectAllSystemsQuery;
    std::string filterStr;
    if (!filterFields.empty())
    {
        filterStr = nx::sql::joinFields(filterFields, " AND ");
        sqlQueryStr += " AND " + filterStr;
    }

    QSqlQuery readSystemsQuery(*queryContext->connection()->qtSqlConnection());
    readSystemsQuery.setForwardOnly(true);
    readSystemsQuery.prepare(sqlQueryStr.c_str());
    nx::sql::bindFields(&readSystemsQuery, filterFields);
    if (!readSystemsQuery.exec())
    {
        NX_WARNING(this, lm("Failed to read system list with filter \"%1\" from DB. %2")
            .args(filterStr, readSystemsQuery.lastError().text()));
        return nx::sql::DBResult::ioError;
    }

    QnSql::fetch_many(readSystemsQuery, systems);
    return nx::sql::DBResult::ok;
}

boost::optional<data::SystemData> SystemDataObject::fetchSystemById(
    nx::sql::QueryContext* queryContext,
    const std::string& systemId)
{
    const nx::sql::InnerJoinFilterFields sqlFilter =
        {nx::sql::SqlFilterFieldEqual(
            "system.id", ":systemId", QnSql::serialized_field(systemId))};

    std::vector<data::SystemData> systems;
    const auto result = fetchSystems(queryContext, sqlFilter, &systems);
    if (result != nx::sql::DBResult::ok)
        throw nx::sql::Exception(result);

    if (systems.empty())
        return boost::none;

    NX_ASSERT(systems.size() == 1);

    return std::move(systems[0]);
}

nx::sql::DBResult SystemDataObject::deleteExpiredSystems(
    nx::sql::QueryContext* queryContext)
{
    //dropping expired not-activated systems and expired marked-for-removal systems
    QSqlQuery dropExpiredSystems(*queryContext->connection()->qtSqlConnection());
    dropExpiredSystems.prepare(
        "DELETE FROM system "
        "WHERE (status_code=:notActivatedStatusCode OR status_code=:deletedStatusCode) "
        "AND expiration_utc_timestamp < :currentTime");
    dropExpiredSystems.bindValue(
        ":notActivatedStatusCode",
        static_cast<int>(api::SystemStatus::notActivated));
    dropExpiredSystems.bindValue(
        ":deletedStatusCode",
        static_cast<int>(api::SystemStatus::deleted_));
    dropExpiredSystems.bindValue(
        ":currentTime",
        (int)nx::utils::timeSinceEpoch().count());
    if (!dropExpiredSystems.exec())
    {
        NX_WARNING(this, lit("Error deleting expired systems from DB. %1").
            arg(dropExpiredSystems.lastError().text()));
        return nx::sql::DBResult::ioError;
    }

    return nx::sql::DBResult::ok;
}

} // namespace rdb
} // namespace dao
} // namespace nx::cloud::db
