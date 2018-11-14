#include "dao_rdb_user_authentication.h"

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/sql.h>
#include <nx/sql/query.h>
#include <nx/utils/log/log.h>
#include <nx/utils/time.h>

#include <nx/cloud/db/client/data/auth_data.h>

namespace nx::cloud::db {
namespace dao {
namespace rdb {

boost::optional<std::string> UserAuthentication::fetchSystemNonce(
    nx::sql::QueryContext* const queryContext,
    const std::string& systemId)
{
    QString sqlRequestStr = R"sql(
        SELECT nonce
        FROM system_auth_info
        WHERE system_id=:systemId
    )sql";

    nx::sql::SqlQuery query(queryContext->connection());
    query.setForwardOnly(true);
    query.prepare(sqlRequestStr);
    query.bindValue(":systemId", QnSql::serialized_field(systemId));
    try
    {
        query.exec();
    }
    catch (const nx::sql::Exception& e)
    {
        NX_WARNING(this, lm("Error selecting system %1 nonce. %2")
            .arg(systemId).arg(e.what()));
        throw;
    }

    if (!query.next())
        return boost::none;

    return query.value(0).toString().toStdString();
}

void UserAuthentication::insertOrReplaceSystemNonce(
    nx::sql::QueryContext* const queryContext,
    const std::string& systemId,
    const std::string& nonce)
{
    nx::sql::SqlQuery query(queryContext->connection());
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
    catch (const nx::sql::Exception& e)
    {
        NX_WARNING(this, lm("Error inserting system %1 nonce %2. %3")
            .arg(systemId).arg(nonce).arg(e.what()));
        throw;
    }
}

api::AuthInfo UserAuthentication::fetchUserAuthRecords(
    nx::sql::QueryContext* const queryContext,
    const std::string& systemId,
    const std::string& accountId)
{
    QString sqlRequestStr = R"sql(
        SELECT nonce, intermediate_response as intermediateResponse,
            expiration_time_utc as expirationTime
        FROM system_user_auth_info
        WHERE account_id=:accountId AND system_id=:systemId
    )sql";

    nx::sql::SqlQuery query(queryContext->connection());
    query.setForwardOnly(true);
    query.prepare(sqlRequestStr);
    query.bindValue(":systemId", QnSql::serialized_field(systemId));
    query.bindValue(":accountId", QnSql::serialized_field(accountId));
    try
    {
        query.exec();
    }
    catch (const nx::sql::Exception& e)
    {
        NX_WARNING(this, lm("Error selecting user %1 auth records for system %2. %3")
            .arg(accountId).arg(systemId).arg(e.what()));
        throw;
    }

    api::AuthInfo result;
    QnSql::fetch_many(query.impl(), &result.records);
    return result;
}

std::vector<std::string> UserAuthentication::fetchSystemsWithExpiredAuthRecords(
    nx::sql::QueryContext* const queryContext,
    int systemCountLimit)
{
    const auto currentTime = std::chrono::floor<std::chrono::milliseconds>(
        nx::utils::utcTime().time_since_epoch());

    nx::sql::SqlQuery query(queryContext->connection());
    query.setForwardOnly(true);
    query.prepare(R"sql(
        SELECT DISTINCT system_id
        FROM system_user_auth_info
        WHERE expiration_time_utc < :curTime
        LIMIT :maxSystemsToReturn
    )sql");
    query.bindValue(":curTime", (long long)currentTime.count());
    query.bindValue(":maxSystemsToReturn", systemCountLimit);
    try
    {
        query.exec();
    }
    catch (const nx::sql::Exception& e)
    {
        NX_WARNING(this, lm("Error selecting systems with expired auth records. %1")
            .args(e.what()));
        throw;
    }

    std::vector<std::string> systems;
    while (query.next())
        systems.push_back(query.value(0).toString().toStdString());

    return systems;
}

void UserAuthentication::insertUserAuthRecords(
    nx::sql::QueryContext* const queryContext,
    const std::string& systemId,
    const std::string& accountId,
    const api::AuthInfo& userAuthRecords)
{
    for (const auto& authRecord: userAuthRecords.records)
    {
        nx::sql::SqlQuery query(queryContext->connection());
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
        catch(const nx::sql::Exception& e)
        {
            NX_WARNING(this, lm("Error inserting user %1 auth records (%2) for system %3. %4")
                .arg(accountId).arg(userAuthRecords.records.size())
                .arg(systemId).arg(e.what()));
            throw;
        }
    }
}

std::vector<AbstractUserAuthentication::SystemInfo> UserAuthentication::fetchAccountSystems(
    nx::sql::QueryContext* const queryContext,
    const std::string& accountId)
{
    nx::sql::SqlQuery query(queryContext->connection());
    query.setForwardOnly(true);
    query.prepare(R"sql(
        SELECT sta.system_id AS systemId, sta.vms_user_id AS vmsUserId, sai.nonce AS nonce
        FROM system_to_account sta, system_auth_info sai
        WHERE sta.account_id = :accountId AND sta.system_id = sai.system_id
    )sql");
    query.bindValue(":accountId", QnSql::serialized_field(accountId));

    try
    {
        query.exec();
    }
    catch (const nx::sql::Exception& e)
    {
        NX_WARNING(this, lm("Error selecting every system auth info for account %1. %2")
            .arg(accountId).arg(e.what()));
        throw;
    }

    std::vector<SystemInfo> result;
    while (query.next())
    {
        SystemInfo systemInfo;
        systemInfo.systemId = query.value("systemId").toString().toStdString();
        systemInfo.vmsUserId = query.value("vmsUserId").toString().toStdString();
        systemInfo.nonce = query.value("nonce").toString().toStdString();
        result.push_back(std::move(systemInfo));
    }

    return result;
}

void UserAuthentication::deleteAccountAuthRecords(
    nx::sql::QueryContext* const queryContext,
    const std::string& accountId)
{
    nx::sql::SqlQuery query(queryContext->connection());
    query.prepare(R"sql(
        DELETE FROM system_user_auth_info
        WHERE account_id = :accountId
    )sql");
    query.bindValue(":accountId", QnSql::serialized_field(accountId));

    try
    {
        query.exec();
    }
    catch (const nx::sql::Exception& e)
    {
        NX_WARNING(this, lm("Error deleting account %1 authentication records. %2")
            .arg(accountId).arg(e.what()));
        throw;
    }
}

void UserAuthentication::deleteSystemAuthRecords(
    nx::sql::QueryContext* const queryContext,
    const std::string& systemId)
{
    nx::sql::SqlQuery query(queryContext->connection());
    query.prepare(R"sql(
        DELETE FROM system_user_auth_info
        WHERE system_id = :systemId
    )sql");
    query.bindValue(":systemId", QnSql::serialized_field(systemId));

    try
    {
        query.exec();
    }
    catch (const nx::sql::Exception& e)
    {
        NX_WARNING(this, lm("Error deleting system %1 authentication records. %2")
            .args(systemId, e.what()));
        throw;
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

} // namespace nx::cloud::db
