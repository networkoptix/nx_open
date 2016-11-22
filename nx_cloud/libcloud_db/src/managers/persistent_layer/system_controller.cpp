#include "system_controller.h"

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace cdb {
namespace persistent_layer {

nx::db::DBResult SystemController::insert(
    nx::db::QueryContext* const queryContext,
    const data::SystemData& system,
    const std::string& accountId)
{
    QSqlQuery insertSystemQuery(*queryContext->connection());
    insertSystemQuery.prepare(
        R"sql(
        INSERT INTO system(
                id, name, customization, auth_key, owner_account_id,
                status_code, expiration_utc_timestamp, opaque)
        VALUES(:id, :name, :customization, :authKey, :ownerAccountID,
               :status, :expirationTimeUtc, :opaque)
        )sql");

    QnSql::bind(system, &insertSystemQuery);
    insertSystemQuery.bindValue(":ownerAccountID", QnSql::serialized_field(accountId));
    insertSystemQuery.bindValue(":expirationTimeUtc", system.expirationTimeUtc);
    if (!insertSystemQuery.exec())
    {
        NX_LOG(lm("Could not insert system %1 (%2) into DB. %3")
            .arg(system.name).arg(system.id).arg(insertSystemQuery.lastError().text()),
            cl_logDEBUG1);
        return nx::db::DBResult::ioError;
    }

    return nx::db::DBResult::ok;
}

nx::db::DBResult SystemController::selectSystemSequence(
    nx::db::QueryContext* const queryContext,
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
        return db::DBResult::ioError;
    }
    *sequence = selectSystemSequence.value(0).toULongLong();

    return db::DBResult::ok;
}

} // namespace persistent_layer
} // namespace cdb
} // namespace nx
