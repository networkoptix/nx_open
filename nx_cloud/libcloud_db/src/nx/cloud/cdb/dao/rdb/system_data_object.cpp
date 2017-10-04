#include "system_data_object.h"

#include <chrono>

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>
#include <nx/utils/time.h>

#include "../../settings.h"

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {

SystemDataObject::SystemDataObject(const conf::Settings& settings):
    m_settings(settings)
{
}

nx::utils::db::DBResult SystemDataObject::insert(
    nx::utils::db::QueryContext* const queryContext,
    const data::SystemData& system,
    const std::string& accountId)
{
    QSqlQuery insertSystemQuery(*queryContext->connection());
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
        NX_LOG(lm("Could not insert system %1 (%2) into DB. %3")
            .arg(system.name).arg(system.id).arg(insertSystemQuery.lastError().text()),
            cl_logDEBUG1);
        return nx::utils::db::DBResult::ioError;
    }

    return nx::utils::db::DBResult::ok;
}

nx::utils::db::DBResult SystemDataObject::selectSystemSequence(
    nx::utils::db::QueryContext* const queryContext,
    const std::string& systemId,
    std::uint64_t* const sequence)
{
    QSqlQuery selectSystemSequence(*queryContext->connection());
    selectSystemSequence.setForwardOnly(true);
    selectSystemSequence.prepare(
        R"sql(
        SELECT seq FROM system WHERE id=?
        )sql");
    selectSystemSequence.bindValue(0, QnSql::serialized_field(systemId));
    if (!selectSystemSequence.exec() || !selectSystemSequence.next())
    {
        NX_LOG(lm("Error selecting sequence of system %1. %2")
            .arg(systemId).arg(selectSystemSequence.lastError().text()),
            cl_logDEBUG1);
        return nx::utils::db::DBResult::ioError;
    }
    *sequence = selectSystemSequence.value(0).toULongLong();

    return nx::utils::db::DBResult::ok;
}

nx::utils::db::DBResult SystemDataObject::markSystemAsDeleted(
    nx::utils::db::QueryContext* const queryContext,
    const std::string& systemId)
{
    QSqlQuery markSystemAsRemoved(*queryContext->connection());
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
        NX_LOG(lm("Error marking system %1 as deleted. %2")
            .arg(systemId).arg(markSystemAsRemoved.lastError().text()),
            cl_logDEBUG1);
        return nx::utils::db::DBResult::ioError;
    }

    return nx::utils::db::DBResult::ok;
}

nx::utils::db::DBResult SystemDataObject::deleteSystem(
    nx::utils::db::QueryContext* const queryContext,
    const std::string& systemId)
{
    QSqlQuery removeSystem(*queryContext->connection());
    removeSystem.prepare("DELETE FROM system WHERE id=:systemId");
    QnSql::bind(systemId, &removeSystem);
    if (!removeSystem.exec())
    {
        NX_LOG(lm("Could not delete system %1. %2")
            .arg(systemId).arg(removeSystem.lastError().text()),
            cl_logDEBUG1);
        return nx::utils::db::DBResult::ioError;
    }

    return nx::utils::db::DBResult::ok;
}

nx::utils::db::DBResult SystemDataObject::execSystemNameUpdate(
    nx::utils::db::QueryContext* const queryContext,
    const data::SystemAttributesUpdate& data)
{
    QSqlQuery updateSystemNameQuery(*queryContext->connection());
    updateSystemNameQuery.prepare(
        "UPDATE system "
        "SET name=:name "
        "WHERE id=:systemId");
    updateSystemNameQuery.bindValue(":name", QnSql::serialized_field(data.name.get()));
    updateSystemNameQuery.bindValue(":systemId", QnSql::serialized_field(data.systemId));
    if (!updateSystemNameQuery.exec())
    {
        NX_LOGX(lm("Failed to update system %1 name in DB to %2. %3")
            .arg(data.systemId).arg(data.name.get())
            .arg(updateSystemNameQuery.lastError().text()), cl_logWARNING);
        return nx::utils::db::DBResult::ioError;
    }

    return nx::utils::db::DBResult::ok;
}

nx::utils::db::DBResult SystemDataObject::execSystemOpaqueUpdate(
    nx::utils::db::QueryContext* const queryContext,
    const data::SystemAttributesUpdate& data)
{
    // TODO: #ak: this is a copy-paste of a previous method. Refactor!

    QSqlQuery updateSystemOpaqueQuery(*queryContext->connection());
    updateSystemOpaqueQuery.prepare(
        "UPDATE system "
        "SET opaque=:opaque "
        "WHERE id=:systemId");
    updateSystemOpaqueQuery.bindValue(":opaque", QnSql::serialized_field(data.opaque.get()));
    updateSystemOpaqueQuery.bindValue(":systemId", QnSql::serialized_field(data.systemId));
    if (!updateSystemOpaqueQuery.exec())
    {
        NX_LOGX(lm("Error updating system %1. %2")
            .arg(data.systemId).arg(updateSystemOpaqueQuery.lastError().text()),
            cl_logWARNING);
        return nx::utils::db::DBResult::ioError;
    }

    return nx::utils::db::DBResult::ok;
}

nx::utils::db::DBResult SystemDataObject::activateSystem(
    nx::utils::db::QueryContext* const queryContext,
    const std::string& systemId)
{
    QSqlQuery updateSystemStatusQuery(*queryContext->connection());
    updateSystemStatusQuery.prepare(
        "UPDATE system "
        "SET status_code=:statusCode, expiration_utc_timestamp=:expirationTimeUtc "
        "WHERE id=:id");
    updateSystemStatusQuery.bindValue(
        ":statusCode",
        QnSql::serialized_field(static_cast<int>(api::SystemStatus::activated)));
    updateSystemStatusQuery.bindValue(
        ":id",
        QnSql::serialized_field(systemId));
    updateSystemStatusQuery.bindValue(
        ":expirationTimeUtc",
        std::numeric_limits<int>::max());
    if (!updateSystemStatusQuery.exec())
    {
        NX_LOG(lit("Failed to read system list from DB. %1").
            arg(updateSystemStatusQuery.lastError().text()), cl_logWARNING);
        return nx::utils::db::DBResult::ioError;
    }

    return nx::utils::db::DBResult::ok;
}

nx::utils::db::DBResult SystemDataObject::fetchSystems(
    nx::utils::db::QueryContext* queryContext,
    const nx::utils::db::InnerJoinFilterFields& filterFields,
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

    QString sqlQueryStr = QString::fromLatin1(kSelectAllSystemsQuery);
    QString filterStr;
    if (!filterFields.empty())
    {
        filterStr = nx::utils::db::joinFields(filterFields, " AND ");
        sqlQueryStr += " AND " + filterStr;
    }

    QSqlQuery readSystemsQuery(*queryContext->connection());
    readSystemsQuery.setForwardOnly(true);
    readSystemsQuery.prepare(sqlQueryStr);
    nx::utils::db::bindFields(&readSystemsQuery, filterFields);
    if (!readSystemsQuery.exec())
    {
        NX_LOG(lit("Failed to read system list with filter \"%1\" from DB. %2")
            .arg(filterStr).arg(readSystemsQuery.lastError().text()),
            cl_logWARNING);
        return nx::utils::db::DBResult::ioError;
    }

    QnSql::fetch_many(readSystemsQuery, systems);
    return nx::utils::db::DBResult::ok;
}

boost::optional<data::SystemData> SystemDataObject::fetchSystemById(
    nx::utils::db::QueryContext* queryContext,
    const std::string& systemId)
{
    const nx::utils::db::InnerJoinFilterFields sqlFilter =
        {{"system.id", ":systemId", QnSql::serialized_field(systemId)}};

    std::vector<data::SystemData> systems;
    const auto result = fetchSystems(queryContext, sqlFilter, &systems);
    if (result != nx::utils::db::DBResult::ok)
        throw nx::utils::db::Exception(result);

    if (systems.empty())
        return boost::none;

    NX_ASSERT(systems.size() == 1);

    return std::move(systems[0]);
}

nx::utils::db::DBResult SystemDataObject::deleteExpiredSystems(
    nx::utils::db::QueryContext* queryContext)
{
    //dropping expired not-activated systems and expired marked-for-removal systems
    QSqlQuery dropExpiredSystems(*queryContext->connection());
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
        NX_LOGX(lit("Error deleting expired systems from DB. %1").
            arg(dropExpiredSystems.lastError().text()), cl_logWARNING);
        return nx::utils::db::DBResult::ioError;
    }

    return nx::utils::db::DBResult::ok;
}

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
