#include "dao_rdb_user_authentication.h"

#include <utils/db/query.h>
#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/sql.h>
#include <nx/utils/log/log.h>

#include <nx/cloud/cdb/client/data/auth_data.h>

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {

std::string UserAuthentication::fetchSystemNonce(
    nx::db::QueryContext* const queryContext,
    const std::string& systemId)
{
    QString sqlRequestStr = R"sql(
        SELECT nonce
        FROM system_auth_info
        WHERE system_id=:systemId
    )sql";

    nx::db::SqlQuery query(*queryContext->connection());
    query.setForwardOnly(true);
    query.prepare(sqlRequestStr);
    query.bindValue(":systemId", QnSql::serialized_field(systemId));
    try
    {
        query.exec();
    }
    catch (const nx::db::Exception e)
    {
        NX_WARNING(this, lm("Error selecting system %1 nonce. %2")
            .arg(systemId).arg(e.what()));
        throw;
    }

    if (!query.next())
    {
        NX_DEBUG(this, lm("There is no nonce of system %1").arg(systemId));
        throw nx::db::Exception(nx::db::DBResult::notFound);
    }

    return query.value(0).toString().toStdString();
}

void UserAuthentication::insertOrReplaceSystemNonce(
    nx::db::QueryContext* const queryContext,
    const std::string& systemId,
    const std::string& nonce)
{
    nx::db::SqlQuery query(*queryContext->connection());
    query.prepare(R"sql(
        REPLACE INTO system_auth_info(system_id, nonce)
        VALUES(:systemId, :nonce)
    )sql");
    query.bindValue(":systemId", QnSql::serialized_field(systemId));
    query.bindValue(":nonce", QnSql::serialized_field(nonce));

    try
    {
        query.exec();
    }
    catch (const nx::db::Exception e)
    {
        NX_WARNING(this, lm("Error inserting system %1 nonce %2. %3")
            .arg(systemId).arg(nonce).arg(e.what()));
        throw;
    }
}

api::AuthInfo UserAuthentication::fetchUserAuthRecords(
    nx::db::QueryContext* const queryContext,
    const std::string& systemId,
    const std::string& accountId)
{
    QString sqlRequestStr = R"sql(
        SELECT nonce, intermediate_response as intermediateResponse, 
            expiration_time_utc as expirationTime
        FROM system_user_auth_info
        WHERE account_id=:accountId AND system_id=:systemId
    )sql";

    nx::db::SqlQuery query(*queryContext->connection());
    query.setForwardOnly(true);
    query.prepare(sqlRequestStr);
    query.bindValue(":systemId", QnSql::serialized_field(systemId));
    query.bindValue(":accountId", QnSql::serialized_field(accountId));
    try
    {
        query.exec();
    }
    catch (const nx::db::Exception e)
    {
        NX_WARNING(this, lm("Error selecting user %1 auth records for system %2. %3")
            .arg(accountId).arg(systemId).arg(e.what()));
        throw;
    }

    api::AuthInfo result;
    QnSql::fetch_many(query.impl(), &result.records);
    return result;
}

void UserAuthentication::insertUserAuthRecords(
    nx::db::QueryContext* const queryContext,
    const std::string& systemId,
    const std::string& accountId,
    const api::AuthInfo& userAuthRecords)
{
    for (const auto& authRecord: userAuthRecords.records)
    {
        nx::db::SqlQuery query(*queryContext->connection());
        query.prepare(R"sql(
            INSERT INTO system_user_auth_info(
                system_id, account_id, nonce,   
                intermediate_response, expiration_time_utc)
            VALUES(:systemId, :accountId, :nonce, :intermediateResponse, :expirationTime)
        )sql");
        query.bindValue(":systemId", QnSql::serialized_field(systemId));
        query.bindValue(":accountId", QnSql::serialized_field(accountId));
        QnSql::bind(authRecord, &query.impl());

        try
        {
            query.exec();
        }
        catch(const nx::db::Exception e)
        {
            NX_WARNING(this, lm("Error inserting user %1 auth records (%2) for system %3. %4")
                .arg(accountId).arg(userAuthRecords.records.size())
                .arg(systemId).arg(e.what()));
            throw;
        }
    }
}

} // namespace rdb
} // namespace dao

namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (AuthInfoRecord),
    (sql_record),
    _Fields)

} // namespace api

} // namespace cdb
} // namespace nx
