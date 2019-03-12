#include "system_sharing_data_object.h"

#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>

namespace nx::cloud::db {
namespace dao {
namespace rdb {

nx::sql::DBResult SystemSharingDataObject::insertOrReplaceSharing(
    nx::sql::QueryContext* const queryContext,
    const api::SystemSharingEx& sharing)
{
    QSqlQuery replaceSharingQuery(*queryContext->connection()->qtSqlConnection());
    replaceSharingQuery.prepare(R"sql(
        REPLACE INTO system_to_account(
            account_id, system_id, access_role_id, user_role_id, custom_permissions,
            is_enabled, vms_user_id, last_login_time_utc, usage_frequency)
        VALUES(:accountId, :systemId, :accessRole, :userRoleId, :customPermissions,
                :isEnabled, :vmsUserId, :lastLoginTime, :usageFrequency)
    )sql");
    QnSql::bind(sharing, &replaceSharingQuery);
    if (!replaceSharingQuery.exec())
    {
        NX_DEBUG(this, lm("Failed to update/remove sharing. system %1, account %2, access role %3. %4")
            .arg(sharing.systemId).arg(sharing.accountEmail)
            .arg(QnLexical::serialized(sharing.accessRole))
            .arg(replaceSharingQuery.lastError().text()));
        return nx::sql::DBResult::ioError;
    }

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult SystemSharingDataObject::fetchAllUserSharings(
    nx::sql::QueryContext* const queryContext,
    std::vector<api::SystemSharingEx>* const sharings)
{
    return fetchUserSharings(
        queryContext,
        nx::sql::InnerJoinFilterFields(),
        sharings);
}

nx::sql::DBResult SystemSharingDataObject::fetchUserSharingsByAccountEmail(
    nx::sql::QueryContext* const queryContext,
    const std::string& accountEmail,
    std::vector<api::SystemSharingEx>* const sharings)
{
    return fetchUserSharings(
        queryContext,
        {nx::sql::SqlFilterFieldEqual(
            "email", ":accountEmail", QnSql::serialized_field(accountEmail))},
        sharings);
}

nx::sql::DBResult SystemSharingDataObject::fetchSharing(
    nx::sql::QueryContext* const queryContext,
    const std::string& accountEmail,
    const std::string& systemId,
    api::SystemSharingEx* const sharing)
{
    const auto dbResult = fetchSharing(
        queryContext,
        {nx::sql::SqlFilterFieldEqual(
            "email", ":accountEmail", QnSql::serialized_field(accountEmail)),
         nx::sql::SqlFilterFieldEqual(
            "system_id", ":systemId", QnSql::serialized_field(systemId))},
        sharing);
    if (dbResult != nx::sql::DBResult::ok &&
        dbResult != nx::sql::DBResult::notFound)
    {
        NX_WARNING(this, lm("Error fetching sharing of system %1 to account %2")
            .arg(systemId).arg(accountEmail));
    }

    return dbResult;
}

nx::sql::DBResult SystemSharingDataObject::fetchSharing(
    nx::sql::QueryContext* const queryContext,
    const nx::sql::InnerJoinFilterFields& filterFields,
    api::SystemSharingEx* const sharing)
{
    std::vector<api::SystemSharingEx> sharings;
    const auto result = fetchUserSharings(
        queryContext,
        filterFields,
        &sharings);
    if (result != nx::sql::DBResult::ok)
        return result;
    if (sharings.empty())
        return nx::sql::DBResult::notFound;

    NX_ASSERT(sharings.size() == 1);
    *sharing = std::move(sharings[0]);

    return nx::sql::DBResult::ok;
}

std::vector<api::SystemSharingEx> SystemSharingDataObject::fetchSystemSharings(
    sql::QueryContext* queryContext,
    const std::string& systemId)
{
    std::vector<api::SystemSharingEx> systemSharings;

    const auto dbResult = fetchUserSharings(
        queryContext,
        { nx::sql::SqlFilterFieldEqual(
            "system_id", ":systemId", QnSql::serialized_field(systemId)) },
        &systemSharings);
    if (dbResult != sql::DBResult::ok)
        throw sql::Exception(dbResult);

    return systemSharings;
}

nx::sql::DBResult SystemSharingDataObject::deleteSharing(
    nx::sql::QueryContext* queryContext,
    const std::string& systemId,
    const nx::sql::InnerJoinFilterFields& filterFields)
{
    QSqlQuery removeSharingQuery(*queryContext->connection()->qtSqlConnection());
    std::string sqlQueryStr = R"sql(
        DELETE FROM system_to_account WHERE system_id=:systemId
    )sql";

    std::string filterStr;
    if (!filterFields.empty())
    {
        filterStr = nx::sql::joinFields(filterFields, " AND ");
        sqlQueryStr += " AND " + filterStr;
    }
    removeSharingQuery.prepare(sqlQueryStr.c_str());
    removeSharingQuery.bindValue(
        ":systemId",
        QnSql::serialized_field(systemId));
    nx::sql::bindFields(&removeSharingQuery, filterFields);
    if (!removeSharingQuery.exec())
    {
        NX_DEBUG(this,
            lm("Failed to remove sharing. system %1, filter \"%2\". %3")
                .args(systemId, filterStr, removeSharingQuery.lastError().text()));
        return nx::sql::DBResult::ioError;
    }
    return nx::sql::DBResult::ok;
}

nx::sql::DBResult SystemSharingDataObject::calculateUsageFrequencyForANewSystem(
    nx::sql::QueryContext* const queryContext,
    const std::string& accountId,
    const std::string& systemId,
    float* const newSystemInitialUsageFrequency)
{
    QSqlQuery calculateUsageFrequencyForTheNewSystem(
        *queryContext->connection()->qtSqlConnection());
    calculateUsageFrequencyForTheNewSystem.setForwardOnly(true);
    calculateUsageFrequencyForTheNewSystem.prepare(R"sql(
        SELECT MAX(usage_frequency) + 1
        FROM system_to_account
        WHERE account_id = :accountId
    )sql");
    calculateUsageFrequencyForTheNewSystem.bindValue(
        ":accountId",
        QnSql::serialized_field(accountId));
    if (!calculateUsageFrequencyForTheNewSystem.exec() ||
        !calculateUsageFrequencyForTheNewSystem.next())
    {
        NX_DEBUG(this, lm("Failed to fetch usage_frequency for the new system %1 of account %2. %3")
            .arg(systemId).arg(accountId)
            .arg(calculateUsageFrequencyForTheNewSystem.lastError().text()));
        return nx::sql::DBResult::ioError;
    }

    *newSystemInitialUsageFrequency =
        calculateUsageFrequencyForTheNewSystem.value(0).toFloat();
    return nx::sql::DBResult::ok;
}

nx::sql::DBResult SystemSharingDataObject::updateUserLoginStatistics(
    nx::sql::QueryContext* queryContext,
    const std::string& accountId,
    const std::string& systemId,
    std::chrono::system_clock::time_point lastloginTime,
    float usageFrequency)
{
    QSqlQuery updateUsageStatisticsQuery(
        *queryContext->connection()->qtSqlConnection());
    updateUsageStatisticsQuery.prepare(R"sql(
        UPDATE system_to_account
        SET last_login_time_utc=:last_login_time_utc, usage_frequency=:usage_frequency
        WHERE account_id=:account_id AND system_id=:system_id
    )sql");
    updateUsageStatisticsQuery.bindValue(
        ":last_login_time_utc",
        QnSql::serialized_field(lastloginTime));
    updateUsageStatisticsQuery.bindValue(":usage_frequency", usageFrequency);
    updateUsageStatisticsQuery.bindValue(
        ":account_id", QnSql::serialized_field(accountId));
    updateUsageStatisticsQuery.bindValue(
        ":system_id", QnSql::serialized_field(systemId));
    if (!updateUsageStatisticsQuery.exec())
    {
        NX_WARNING(this, lm("Error executing request to update system %1 usage by %2 in db. %3")
            .arg(systemId).arg(accountId)
            .arg(updateUsageStatisticsQuery.lastError().text()));
        return nx::sql::DBResult::ioError;
    }

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult SystemSharingDataObject::fetchUserSharings(
    nx::sql::QueryContext* const queryContext,
    const nx::sql::InnerJoinFilterFields& filterFields,
    std::vector<api::SystemSharingEx>* const sharings)
{
    std::string sqlRequestStr = R"sql(
        SELECT a.id as accountId,
               a.email as accountEmail,
               sa.system_id as systemId,
               sa.access_role_id as accessRole,
               sa.user_role_id as userRoleId,
               sa.custom_permissions as customPermissions,
               sa.is_enabled as isEnabled,
               sa.vms_user_id as vmsUserId,
               sa.last_login_time_utc as lastLoginTime,
               sa.usage_frequency as usageFrequency
        FROM system_to_account sa, account a
        WHERE sa.account_id=a.id
    )sql";

    std::string filterStr;
    if (!filterFields.empty())
    {
        filterStr = nx::sql::joinFields(filterFields, " AND ");
        sqlRequestStr += " AND " + filterStr;
    }

    QSqlQuery selectSharingQuery(*queryContext->connection()->qtSqlConnection());
    selectSharingQuery.setForwardOnly(true);
    selectSharingQuery.prepare(sqlRequestStr.c_str());
    nx::sql::bindFields(&selectSharingQuery, filterFields);
    if (!selectSharingQuery.exec())
    {
        NX_WARNING(this, lm("Error executing request to select sharings with filter \"%1\". %2")
            .arg(filterStr).arg(selectSharingQuery.lastError().text()));
        return nx::sql::DBResult::ioError;
    }
    QnSql::fetch_many(selectSharingQuery, sharings);

    return nx::sql::DBResult::ok;
}

} // namespace rdb
} // namespace dao
} // namespace nx::cloud::db
