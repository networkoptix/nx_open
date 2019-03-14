#include "user_permissions_db_migration.h"

#include <QtCore/QVariant>

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

#include <common/common_globals.h>
#include <compatibility/user_permissions.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>
#include <utils/db/db_helper.h>
#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>

namespace ec2 {
namespace db {
namespace detail {

struct UserPermissionsRemapData
{
    UserPermissionsRemapData() {}
    UserPermissionsRemapData(int id, int permissions):
        id(id),
        permissions(permissions)
    {
    }

    int id = 0;
    int permissions = 0;
};

bool doRemap(const QSqlDatabase& database, const UserPermissionsRemapData& data)
{
    QSqlQuery query(database);
    query.setForwardOnly(true);
    QString sqlText("UPDATE vms_userprofile set rights = :permissions where user_id = :id");
    if (!nx::sql::SqlQueryExecutionHelper::prepareSQLQuery(&query, sqlText, Q_FUNC_INFO))
        return false;
    query.bindValue(":id", data.id);
    query.bindValue(":permissions", data.permissions);
    return nx::sql::SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO);
}

bool doMigration(const QSqlDatabase& database, std::function<int(int)> migrateFunc)
{
    QSqlQuery query(database);
    query.setForwardOnly(true);
    QString sqlText = "SELECT user_id, rights from vms_userprofile";
    if (!nx::sql::SqlQueryExecutionHelper::prepareSQLQuery(&query, sqlText, Q_FUNC_INFO))
        return false;

    if (!nx::sql::SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    std::vector<UserPermissionsRemapData> migrationQueue;
    while (query.next())
    {
        int oldPermissions = query.value("rights").toInt();
        int updated = migrateFunc(oldPermissions);
        if (oldPermissions == updated)
            continue;

        migrationQueue.emplace_back(query.value("user_id").toInt(), updated);
    }

    for (const auto& remapData: migrationQueue)
    {
        if (!doRemap(database, remapData))
            return false;
    }

    return true;
}

int migrateFromV26(int oldPermissions)
{
    using namespace nx::common::compatibility;
    const auto v26permissions =
        static_cast<user_permissions::GlobalPermissionsV26>(oldPermissions);
    const auto result = user_permissions::migrateFromV26(v26permissions);

    NX_DEBUG(typeid(QSqlDatabase), lm("Migrating User Permissions: %1 -> %2").args(
        QnLexical::serialized(v26permissions), QnLexical::serialized(result)));

    return result;
}

int fixCustomFlag(int oldPermissionsValue)
{
    GlobalPermissions oldPermissions(oldPermissionsValue);
    using boost::algorithm::any_of;
    GlobalPermissions result =
        oldPermissions & ~GlobalPermissions(GlobalPermission::customUser);
    if (result.testFlag(GlobalPermission::admin))
        return int(GlobalPermission::admin);

    const bool isPredefined = any_of(QnUserRolesManager::getPredefinedRoles(),
        [result](const nx::vms::api::PredefinedRoleData& role)
        {
            return role.permissions == result;
        });
    if (!isPredefined)
        result |= GlobalPermission::customUser;

    QString logMessage = lit("Fix User Permissions Custom Flag: %1 -> %2")
        .arg(QnLexical::serialized(oldPermissions))
        .arg(result);
    NX_INFO(typeid(QSqlDatabase), logMessage);

    return result;
}

}

bool migrateV25UserPermissions(const QSqlDatabase& database)
{
    return detail::doMigration(database, detail::migrateFromV26);
}

bool fixCustomPermissionFlag(const QSqlDatabase& database)
{
    return detail::doMigration(database, detail::fixCustomFlag);
}

} // namespace db
} // namespace ec2
