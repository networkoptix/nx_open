#include "system_sharing_data_object.h"

#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {

nx::utils::db::DBResult SystemSharingDataObject::insertOrReplaceSharing(
    nx::utils::db::QueryContext* const queryContext,
    const api::SystemSharingEx& sharing)
{
    QSqlQuery replaceSharingQuery(*queryContext->connection());
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
        NX_LOG(lm("Failed to update/remove sharing. system %1, account %2, access role %3. %4")
            .arg(sharing.systemId).arg(sharing.accountEmail)
            .arg(QnLexical::serialized(sharing.accessRole))
            .arg(replaceSharingQuery.lastError().text()),
            cl_logDEBUG1);
        return nx::utils::db::DBResult::ioError;
    }

    return nx::utils::db::DBResult::ok;
}

nx::utils::db::DBResult SystemSharingDataObject::fetchAllUserSharings(
    nx::utils::db::QueryContext* const queryContext,
    std::deque<api::SystemSharingEx>* const sharings)
{
    return fetchUserSharings(
        queryContext,
        nx::utils::db::InnerJoinFilterFields(),
        sharings);
}

nx::utils::db::DBResult SystemSharingDataObject::fetchUserSharingsByAccountEmail(
    nx::utils::db::QueryContext* const queryContext,
    const std::string& accountEmail,
    std::deque<api::SystemSharingEx>* const sharings)
{
    return fetchUserSharings(
        queryContext,
        {nx::utils::db::SqlFilterFieldEqual(
            "email", ":accountEmail", QnSql::serialized_field(accountEmail))},
        sharings);
}

nx::utils::db::DBResult SystemSharingDataObject::fetchSharing(
    nx::utils::db::QueryContext* const queryContext,
    const std::string& accountEmail,
    const std::string& systemId,
    api::SystemSharingEx* const sharing)
{
    const auto dbResult = fetchSharing(
        queryContext,
        {nx::utils::db::SqlFilterFieldEqual(
            "email", ":accountEmail", QnSql::serialized_field(accountEmail)),
         nx::utils::db::SqlFilterFieldEqual(
            "system_id", ":systemId", QnSql::serialized_field(systemId))},
        sharing);
    if (dbResult != nx::utils::db::DBResult::ok &&
        dbResult != nx::utils::db::DBResult::notFound)
    {
        NX_LOGX(lm("Error fetching sharing of system %1 to account %2")
            .arg(systemId).arg(accountEmail),
            cl_logWARNING);
    }

    return dbResult;
}

nx::utils::db::DBResult SystemSharingDataObject::fetchSharing(
    nx::utils::db::QueryContext* const queryContext,
    const nx::utils::db::InnerJoinFilterFields& filterFields,
    api::SystemSharingEx* const sharing)
{
    std::deque<api::SystemSharingEx> sharings;
    const auto result = fetchUserSharings(
        queryContext,
        filterFields,
        &sharings);
    if (result != nx::utils::db::DBResult::ok)
        return result;
    if (sharings.empty())
        return nx::utils::db::DBResult::notFound;

    NX_ASSERT(sharings.size() == 1);
    *sharing = std::move(sharings[0]);

    return nx::utils::db::DBResult::ok;
}

nx::utils::db::DBResult SystemSharingDataObject::deleteSharing(
    nx::utils::db::QueryContext* queryContext,
    const std::string& systemId,
    const nx::utils::db::InnerJoinFilterFields& filterFields)
{
    QSqlQuery removeSharingQuery(*queryContext->connection());
    QString sqlQueryStr = R"sql(
        DELETE FROM system_to_account WHERE system_id=:systemId
    )sql";

    QString filterStr;
    if (!filterFields.empty())
    {
        filterStr = nx::utils::db::joinFields(filterFields, " AND ");
        sqlQueryStr += " AND " + filterStr;
    }
    removeSharingQuery.prepare(sqlQueryStr);
    removeSharingQuery.bindValue(
        ":systemId",
        QnSql::serialized_field(systemId));
    nx::utils::db::bindFields(&removeSharingQuery, filterFields);
    if (!removeSharingQuery.exec())
    {
        NX_LOGX(
            QnLog::EC2_TRAN_LOG,
            lm("Failed to remove sharing. system %1, filter \"%2\". %3")
            .arg(systemId).arg(filterStr).arg(removeSharingQuery.lastError().text()),
            cl_logDEBUG1);
        return nx::utils::db::DBResult::ioError;
    }
    return nx::utils::db::DBResult::ok;
}

nx::utils::db::DBResult SystemSharingDataObject::calculateUsageFrequencyForANewSystem(
    nx::utils::db::QueryContext* const queryContext,
    const std::string& accountId,
    const std::string& systemId,
    float* const newSystemInitialUsageFrequency)
{
    QSqlQuery calculateUsageFrequencyForTheNewSystem(*queryContext->connection());
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
        NX_LOGX(lm("Failed to fetch usage_frequency for the new system %1 of account %2. %3")
            .arg(systemId).arg(accountId)
            .arg(calculateUsageFrequencyForTheNewSystem.lastError().text()),
            cl_logDEBUG1);
        return nx::utils::db::DBResult::ioError;
    }

    *newSystemInitialUsageFrequency =
        calculateUsageFrequencyForTheNewSystem.value(0).toFloat();
    return nx::utils::db::DBResult::ok;
}

nx::utils::db::DBResult SystemSharingDataObject::updateUserLoginStatistics(
    nx::utils::db::QueryContext* queryContext,
    const std::string& accountId,
    const std::string& systemId,
    std::chrono::system_clock::time_point lastloginTime,
    float usageFrequency)
{
    QSqlQuery updateUsageStatisticsQuery(*queryContext->connection());
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
        NX_LOGX(lm("Error executing request to update system %1 usage by %2 in db. %3")
            .arg(systemId).arg(accountId)
            .arg(updateUsageStatisticsQuery.lastError().text()), cl_logWARNING);
        return nx::utils::db::DBResult::ioError;
    }

    return nx::utils::db::DBResult::ok;
}

nx::utils::db::DBResult SystemSharingDataObject::fetchUserSharings(
    nx::utils::db::QueryContext* const queryContext,
    const nx::utils::db::InnerJoinFilterFields& filterFields,
    std::deque<api::SystemSharingEx>* const sharings)
{
    QString sqlRequestStr = R"sql(
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

    QString filterStr;
    if (!filterFields.empty())
    {
        filterStr = nx::utils::db::joinFields(filterFields, " AND ");
        sqlRequestStr += " AND " + filterStr;
    }

    QSqlQuery selectSharingQuery(*queryContext->connection());
    selectSharingQuery.setForwardOnly(true);
    selectSharingQuery.prepare(sqlRequestStr);
    nx::utils::db::bindFields(&selectSharingQuery, filterFields);
    if (!selectSharingQuery.exec())
    {
        NX_LOGX(lm("Error executing request to select sharings with filter \"%1\". %2")
            .arg(filterStr).arg(selectSharingQuery.lastError().text()),
            cl_logWARNING);
        return nx::utils::db::DBResult::ioError;
    }
    QnSql::fetch_many(selectSharingQuery, sharings);

    return nx::utils::db::DBResult::ok;
}

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
