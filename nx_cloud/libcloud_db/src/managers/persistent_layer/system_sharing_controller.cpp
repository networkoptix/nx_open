#include "system_sharing_controller.h"

#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace cdb {
namespace persistent_layer {

nx::db::DBResult SystemSharingController::insertOrReplaceSharing(
    nx::db::QueryContext* const queryContext,
    const api::SystemSharingEx& sharing)
{
    QSqlQuery replaceSharingQuery(*queryContext->connection());
    replaceSharingQuery.prepare(
        R"sql(
        REPLACE INTO system_to_account(
            account_id, system_id, access_role_id, group_id, custom_permissions,
            is_enabled, vms_user_id, last_login_time_utc, usage_frequency)
        VALUES(:accountId, :systemId, :accessRole, :groupId, :customPermissions,
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
        return db::DBResult::ioError;
    }

    return db::DBResult::ok;
}

} // namespace persistent_layer
} // namespace cdb
} // namespace nx
