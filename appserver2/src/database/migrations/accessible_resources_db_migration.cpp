#include "accessible_resources_db_migration.h"

#include <QtCore/QVariant>

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

#include <common/common_globals.h>
#include <core/resource/resource_type.h>

#include <nx_ec/data/api_layout_data.h>

#include <utils/db/db_helper.h>

namespace ec2
{
    namespace db
    {
        typedef QSet<QnUuid> ResourcesSet;

        /* Map of resources that are already accessible to user. */
        typedef QMap<QnUuid, ResourcesSet > AccessibleResourcesMap;

        bool getAccessibleResources(const QSqlDatabase& database, AccessibleResourcesMap& resources)
        {
            QSqlQuery query(database);
            query.setForwardOnly(true);
            const QString queryStr = R"(
                SELECT li.resource_guid as resourceId, r.parent_guid as userId
                FROM vms_layoutitem li
                JOIN vms_resource r on r.id = li.layout_id order by r.parent_guid
            )";
            if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
                return false;

            if (!QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO))
                return false;

            while (query.next())
            {
                QnUuid userId = QnUuid::fromRfc4122(query.value("userId").toByteArray());
                QnUuid resourceId = QnUuid::fromRfc4122(query.value("resourceId").toByteArray());
                QSet<QnUuid>& userResources = resources[userId];
                userResources.insert(resourceId);
            }

            return true;
        }

        bool getDesktopCameraTypeId(const QSqlDatabase& database, QnUuid& result)
        {
            QSqlQuery query(database);
            query.setForwardOnly(true);
            const QString queryStr = R"(
                SELECT guid
                FROM vms_resourcetype
                WHERE name = ?
            )";
            if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
                return false;

            query.addBindValue(QnResourceTypePool::kDesktopCameraTypeName);

            if (!QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO))
                return false;

            /* If there is no desktop camera type in DB, it is strange but not critical. */
            if (query.next())
                result = QnUuid::fromRfc4122(query.value("guid").toByteArray());

            return true;
        }

        bool getAllCameras(const QSqlDatabase& database, ResourcesSet& allCameras)
        {
            /* Desktop cameras must be ignored. */
            QnUuid desktopCameraTypeId;
            if (!getDesktopCameraTypeId(database, desktopCameraTypeId))
                return false;

            QSqlQuery query(database);
            query.setForwardOnly(true);
            const QString queryStr = R"(
                SELECT r.guid, r.xtype_guid as typeId
                FROM vms_resource r
                JOIN vms_camera c on c.resource_ptr_id = r.id ORDER BY r.guid
            )";
            if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
                return false;

            if (!QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO))
                return false;

            while (query.next())
            {
                QnUuid typeId = QnUuid::fromRfc4122(query.value("typeId").toByteArray());
                if (typeId == desktopCameraTypeId)
                    continue;

                QnUuid resourceId = QnUuid::fromRfc4122(query.value("guid").toByteArray());
                allCameras << resourceId;
            }

            return true;
        }

        int getResourceInternalId(const QSqlDatabase& database, const QnUuid& guid)
        {
            QSqlQuery query(database);
            query.setForwardOnly(true);
            if (!QnDbHelper::prepareSQLQuery(&query, "SELECT id from vms_resource where guid = ?", Q_FUNC_INFO))
                return 0;
            query.addBindValue(guid.toRfc4122());
            if (!QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO) || !query.next())
                return 0;
            return query.value(0).toInt();
        }

        int getCurrentUserPermissions(const QSqlDatabase& database, int internalUserId)
        {
            QSqlQuery query(database);
            query.setForwardOnly(true);
            if (!QnDbHelper::prepareSQLQuery(&query, "SELECT rights from vms_userprofile where user_id = ?", Q_FUNC_INFO))
                return 0;
            query.addBindValue(internalUserId);
            if (!QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO) || !query.next())
                return 0;
            return query.value(0).toInt();
        }

        /** Add default live viewer resources permissions. */
        bool addCommonPermissions(const QSqlDatabase& database, const QnUuid& userId)
        {
            int internalUserId = getResourceInternalId(database, userId);
            if (internalUserId <= 0)
                return false;

            int permissions = getCurrentUserPermissions(database, internalUserId);
            permissions |= Qn::GlobalLiveViewerPermissionSet;

            QSqlQuery query(database);
            query.setForwardOnly(true);
            QString sqlText = QString("UPDATE vms_userprofile set rights = :permissions where user_id = :id");
            if (!QnDbHelper::prepareSQLQuery(&query, sqlText, Q_FUNC_INFO))
                return false;
            query.bindValue(":id", internalUserId);
            query.bindValue(":permissions", permissions);
            return QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO);
        }

        /** Add custom available cameras */
        bool addAccessibleCamerasList(const QSqlDatabase& database, const QnUuid& userId, const ResourcesSet& cameras)
        {
            /* Adding 'replace' here to do not mess with developers' changes. */
            const QString queryStr = R"(
                INSERT OR REPLACE
                INTO vms_access_rights
                (guid, resource_ptr_id)
                VALUES
                (:guid, :resource_ptr_id)
                )";
            const QByteArray userGuid = userId.toRfc4122();

            for (const QnUuid& cameraId : cameras)
            {
                int internalId = getResourceInternalId(database, cameraId);
                if (internalId <= 0)
                    return false;

                QSqlQuery query(database);
                query.setForwardOnly(true);
                if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
                    return false;

                query.bindValue(":guid", userGuid);
                query.bindValue(":resource_ptr_id", internalId);

                if (!QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO))
                    return false;
            }

            return true;
        }

        bool migrateAccessibleResources(const QSqlDatabase& database)
        {
            AccessibleResourcesMap resourcesByUser;
            if (!getAccessibleResources(database, resourcesByUser))
                return false;

            ResourcesSet allCameras;
            if (!getAllCameras(database, allCameras))
                return false;

            for (const QnUuid& userId : resourcesByUser.keys())
            {
                QSet<QnUuid> userCameras = resourcesByUser[userId].intersect(allCameras);
                bool allCamerasAvailable = userCameras == allCameras;
                bool success = allCamerasAvailable
                    ? addCommonPermissions(database, userId)
                    : addAccessibleCamerasList(database, userId, userCameras);
                if (!success)
                    return false;
            }

            return true;
        }

    }
}