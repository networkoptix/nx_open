#include "user_permissions_db_migration.h"

#include <QtCore/QVariant>

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

#include <common/common_globals.h>

#include <utils/db/db_helper.h>

namespace ec2
{
    namespace db
    {
        /** Permissions from v2.5 */
        enum GlobalPermissionV25
        {
            V25OwnerPermission                  = 0x00000001,   /**< Root, can edit admins. */
            V25AdminPermission                  = 0x00000002,   /**< Admin, can edit other non-admins. */
            V25EditLayoutsPermission            = 0x00000004,   /**< Can create and edit layouts. */
            V25EditServersPermissions           = 0x00000020,   /**< Can edit server settings. */
            V25ViewLivePermission               = 0x00000080,   /**< Can view live stream of available cameras. */
            V25ViewArchivePermission            = 0x00000100,   /**< Can view archives of available cameras. */
            V25ExportPermission                 = 0x00000200,   /**< Can export archives of available cameras. */
            V25EditCamerasPermission            = 0x00000400,   /**< Can edit camera settings. */
            V25PtzControlPermission             = 0x00000800,   /**< Can change camera's PTZ state. */
            V25EditVideoWallPermission          = 0x00002000,   /**< Can create and edit videowalls */

            /* These permissions were deprecated in 2.5 but kept for compatibility reasons. */
            V20EditUsersPermission              = 0x00000008,
            V20EditCamerasPermission            = 0x00000010,
            V20ViewExportArchivePermission      = 0x00000040,
            V20PanicPermission                  = 0x00001000,
        };
        Q_DECLARE_FLAGS(GlobalPermissionsV25, GlobalPermissionV25)
        Q_DECLARE_OPERATORS_FOR_FLAGS(GlobalPermissionsV25)


        struct UserPermissionsRemapData
        {
            UserPermissionsRemapData() : id(0), permissions(0) {}

            int id;
            int permissions;
        };

        bool doRemap(const QSqlDatabase& database, const UserPermissionsRemapData& data)
        {
            QSqlQuery query(database);
            query.setForwardOnly(true);
            QString sqlText = QString("UPDATE vms_userprofile set rights = :permissions where id = :id");
            if (!QnDbHelper::prepareSQLQuery(&query, sqlText, Q_FUNC_INFO))
                return false;
            query.bindValue(":id", data.id);
            query.bindValue(":permissions", data.permissions);
            return QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO);
        }

        int migratedPermissions(int oldPermissions)
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

            return result;
        }

        bool migrateUserPermissions(const QSqlDatabase& database)
        {

            QSqlQuery query(database);
            query.setForwardOnly(true);
            QString sqlText = "SELECT id, rights from vms_userprofile";
            if (!QnDbHelper::prepareSQLQuery(&query, sqlText, Q_FUNC_INFO))
                return false;
            if (!QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO))
                return false;

            QVector<UserPermissionsRemapData> migrationQueue;
            while (query.next())
            {
                int oldPermissions = query.value("rights").toInt();

                UserPermissionsRemapData data;
                data.id = query.value("id").toInt();
                data.permissions = migratedPermissions(oldPermissions);
                migrationQueue.push_back(data);
            }

            for (const auto& remapData : migrationQueue)
            {
                if (!doRemap(database, remapData))
                    return false;
            }

            return true;
        }

    }
}