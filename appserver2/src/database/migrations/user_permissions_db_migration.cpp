#include "user_permissions_db_migration.h"

#include <QtCore/QVariant>

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

#include <common/common_globals.h>

#include <core/resource_management/user_roles_manager.h>

#include <nx/fusion/model_functions.h>

#include <nx/utils/log/log.h>

#include <utils/db/db_helper.h>

namespace ec2 {
namespace db {
namespace detail {

/** Permissions from v2.5 */
enum GlobalPermissionV25
{
    V25OwnerPermission          = 0x0001,   /**< Root, can edit admins. */
    V25AdminPermission          = 0x0002,   /**< Admin, can edit other non-admins. */
    V25EditLayoutsPermission    = 0x0004,   /**< Can create and edit layouts. */
    V25EditServersPermissions   = 0x0020,   /**< Can edit server settings. */
    V25ViewLivePermission       = 0x0080,   /**< Can view live stream of available cameras. */
    V25ViewArchivePermission    = 0x0100,   /**< Can view archives of available cameras. */
    V25ExportPermission         = 0x0200,   /**< Can export archives of available cameras. */
    V25EditCamerasPermission    = 0x0400,   /**< Can edit camera settings. */
    V25PtzControlPermission     = 0x0800,   /**< Can change camera's PTZ state. */
    V25EditVideoWallPermission  = 0x2000,   /**< Can create and edit videowalls */

    /* These permissions were deprecated in 2.5 but kept for compatibility reasons. */
    V20EditUsersPermission      = 0x0008,
    V20EditCamerasPermission    = 0x0010,
    V20ViewExportArchivePermission = 0x0040,
    V20PanicPermission          = 0x1000,
};
Q_DECLARE_FLAGS(GlobalPermissionsV25, GlobalPermissionV25)
Q_DECLARE_OPERATORS_FOR_FLAGS(GlobalPermissionsV25)

struct UserPermissionsRemapData
{
    UserPermissionsRemapData(): id(0), permissions(0) {}
    UserPermissionsRemapData(int id, int permissions):
        id(id), permissions(permissions)
    {
    }

    int id;
    int permissions;
};


bool doRemap(const QSqlDatabase& database, const UserPermissionsRemapData& data)
{
    QSqlQuery query(database);
    query.setForwardOnly(true);
    QString sqlText("UPDATE vms_userprofile set rights = :permissions where user_id = :id");
    if (!QnDbHelper::prepareSQLQuery(&query, sqlText, Q_FUNC_INFO))
        return false;
    query.bindValue(":id", data.id);
    query.bindValue(":permissions", data.permissions);
    return QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO);
}


bool doMigration(const QSqlDatabase& database, std::function<int(int)> migrateFunc)
{
    QSqlQuery query(database);
    query.setForwardOnly(true);
    QString sqlText = "SELECT user_id, rights from vms_userprofile";
    if (!QnDbHelper::prepareSQLQuery(&query, sqlText, Q_FUNC_INFO))
        return false;

    if (!QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO))
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

int migrateFromV25(int oldPermissions)
{
    GlobalPermissionsV25 v25permissions = static_cast<GlobalPermissionsV25>(oldPermissions);

    /* Admin permission should be enough for everything, it is automatically expanded on check. */
    if (v25permissions.testFlag(V25OwnerPermission) || v25permissions.testFlag(V25AdminPermission))
        return Qn::GlobalAdminPermission;

    /* On first step we are checking 2.0 permissions */
    if (v25permissions.testFlag(V20EditCamerasPermission))
        v25permissions |= V25EditCamerasPermission | V25PtzControlPermission;

    if (v25permissions.testFlag(V20ViewExportArchivePermission))
        v25permissions |= V25ViewArchivePermission | V25ExportPermission;

    /* Second step - process v2.5 permissions. */
    Qn::GlobalPermissions result;
    if (v25permissions.testFlag(V25EditCamerasPermission))
    {
        result |= Qn::GlobalEditCamerasPermission;
        if (v25permissions.testFlag(V25ViewArchivePermission))  /*< Advanced viewers will be able to edit bookmarks. */
            result |= Qn::GlobalManageBookmarksPermission;
    }

    if (v25permissions.testFlag(V25PtzControlPermission))
        result |= Qn::GlobalUserInputPermission;

    if (v25permissions.testFlag(V25ViewArchivePermission))
        result |= Qn::GlobalViewArchivePermission | Qn::GlobalViewBookmarksPermission;

    if (v25permissions.testFlag(V25ExportPermission))
        result |= Qn::GlobalViewArchivePermission | Qn::GlobalViewBookmarksPermission | Qn::GlobalExportPermission;

    if (v25permissions.testFlag(V25EditVideoWallPermission))
        result |= Qn::GlobalControlVideoWallPermission;

    QString logMessage = lit("Migrating User Permissions: %1 -> %2")
        .arg(QnLexical::serialized(v25permissions)).arg(QnLexical::serialized(result));
    NX_LOG(logMessage, cl_logINFO);

    return result;
}

int fixCustomFlag(int oldPermissions)
{
    using boost::algorithm::any_of;
    Qn::GlobalPermissions result =
        static_cast<Qn::GlobalPermissions>(oldPermissions) & ~Qn::GlobalCustomUserPermission;
    if (result.testFlag(Qn::GlobalAdminPermission))
        return Qn::GlobalAdminPermission;

    const bool isPredefined = any_of(QnUserRolesManager::getPredefinedRoles(),
        [result](const ApiPredefinedRoleData& role)
        {
            return role.permissions == result;
        });
    if (!isPredefined)
        result |= Qn::GlobalCustomUserPermission;

    QString logMessage = lit("Fix User Permissions Custom Flag: %1 -> %2")
        .arg(QnLexical::serialized(static_cast<Qn::GlobalPermissions>(oldPermissions)))
        .arg(QnLexical::serialized(result));
    NX_LOG(logMessage, cl_logINFO);

    return result;
}

}

bool migrateV25UserPermissions(const QSqlDatabase& database)
{
    return detail::doMigration(database, detail::migrateFromV25);
}

bool fixCustomPermissionFlag(const QSqlDatabase& database)
{
    return detail::doMigration(database, detail::fixCustomFlag);
}

} // namespace db

} // namespace ec2

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(ec2::db::detail, GlobalPermissionV25,
    (ec2::db::detail::GlobalPermissionV25::V25OwnerPermission, "v25_owner")
    (ec2::db::detail::GlobalPermissionV25::V25AdminPermission, "v25_admin")
    (ec2::db::detail::GlobalPermissionV25::V25EditLayoutsPermission, "v25_editLayouts")
    (ec2::db::detail::GlobalPermissionV25::V25EditServersPermissions, "v25_editServers")
    (ec2::db::detail::GlobalPermissionV25::V25ViewLivePermission, "v25_viewLive")
    (ec2::db::detail::GlobalPermissionV25::V25ViewArchivePermission, "v25_viewArchive")
    (ec2::db::detail::GlobalPermissionV25::V25ExportPermission, "v25_export")
    (ec2::db::detail::GlobalPermissionV25::V25EditCamerasPermission, "v25_editCameras")
    (ec2::db::detail::GlobalPermissionV25::V25PtzControlPermission, "v25_ptzControl")
    (ec2::db::detail::GlobalPermissionV25::V25EditVideoWallPermission, "v25_editVideowall")
    (ec2::db::detail::GlobalPermissionV25::V20EditUsersPermission, "v20_editUsers")
    (ec2::db::detail::GlobalPermissionV25::V20EditCamerasPermission, "v20_editCameras")
    (ec2::db::detail::GlobalPermissionV25::V20ViewExportArchivePermission, "v20_viewArchive")
    (ec2::db::detail::GlobalPermissionV25::V20PanicPermission, "v20_panic")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(ec2::db::detail, GlobalPermissionsV25,
    (ec2::db::detail::GlobalPermissionV25::V25OwnerPermission, "v25_owner")
    (ec2::db::detail::GlobalPermissionV25::V25AdminPermission, "v25_admin")
    (ec2::db::detail::GlobalPermissionV25::V25EditLayoutsPermission, "v25_editLayouts")
    (ec2::db::detail::GlobalPermissionV25::V25EditServersPermissions, "v25_editServers")
    (ec2::db::detail::GlobalPermissionV25::V25ViewLivePermission, "v25_viewLive")
    (ec2::db::detail::GlobalPermissionV25::V25ViewArchivePermission, "v25_viewArchive")
    (ec2::db::detail::GlobalPermissionV25::V25ExportPermission, "v25_export")
    (ec2::db::detail::GlobalPermissionV25::V25EditCamerasPermission, "v25_editCameras")
    (ec2::db::detail::GlobalPermissionV25::V25PtzControlPermission, "v25_ptzControl")
    (ec2::db::detail::GlobalPermissionV25::V25EditVideoWallPermission, "v25_editVideowall")
    (ec2::db::detail::GlobalPermissionV25::V20EditUsersPermission, "v20_editUsers")
    (ec2::db::detail::GlobalPermissionV25::V20EditCamerasPermission, "v20_editCameras")
    (ec2::db::detail::GlobalPermissionV25::V20ViewExportArchivePermission, "v20_viewArchive")
    (ec2::db::detail::GlobalPermissionV25::V20PanicPermission, "v20_panic")
)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (ec2::db::detail::GlobalPermissionV25)(ec2::db::detail::GlobalPermissionsV25),
    (lexical))
