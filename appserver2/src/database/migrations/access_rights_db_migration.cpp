#include "access_rights_db_migration.h"

#include <QtCore/QVariant>

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

#include <nx/utils/db/sql_query_execution_helper.h>
#include <nx_ec/data/api_access_rights_data.h>
#include <database/db_manager.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log_main.h>
#include <common/common_module.h>

using namespace nx::utils::db;

namespace ec2 {
namespace db {

bool cleanupOldData(const QSqlDatabase& database)
{
    // migrate transaction log
    QSqlQuery query(database);
    query.setForwardOnly(true);
    query.prepare(QString("SELECT tran_guid, tran_data from transaction_log"));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    QSqlQuery delQuery(database);
    delQuery.prepare(QString("DELETE FROM transaction_log WHERE tran_guid = ?"));

    while (query.next()) 
    {
        QnUuid tranGuid = QnSql::deserialized_field<QnUuid>(query.value(0));
        QnAbstractTransaction abstractTran;
        QByteArray srcData = query.value(1).toByteArray();
        QnUbjsonReader<QByteArray> stream(&srcData);
        if (!QnUbjson::deserialize(&stream, &abstractTran)) 
        {
            NX_WARNING("migrateAccessRightsToUbjsonFormat", "Can' deserialize transaction from transaction log");
            return false;
        }
        if (abstractTran.command == ApiCommand::removeAccessRights
            || abstractTran.command == ApiCommand::setAccessRights)
        {
            delQuery.addBindValue(QnSql::serialized_field(tranGuid));
            if (!delQuery.exec())
            {
                NX_WARNING("migrateAccessRightsToUbjsonFormat", query.lastError().text());
                return false;
            }
        }
    }

    return true;
}

bool addNewRemoveUserAccessRights(const QSqlDatabase& database, detail::QnDbManager* db)
{
    // migrate transaction log
    QSqlQuery query(database);
    query.setForwardOnly(true);
    query.prepare(QString("SELECT tran_guid, tran_data from transaction_log"));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    while (query.next())
    {
        QnUuid tranGuid = QnSql::deserialized_field<QnUuid>(query.value(0));
        QnAbstractTransaction abstractTran;
        QByteArray srcData = query.value(1).toByteArray();
        QnUbjsonReader<QByteArray> stream(&srcData);
        if (!QnUbjson::deserialize(&stream, &abstractTran))
        {
            NX_WARNING("migrateAccessRightsToUbjsonFormat", "Can't deserialize transaction from transaction log");
            return false;
        }
        if (abstractTran.command != ApiCommand::removeUser)
            continue;
        ApiIdData data;
        if (!QnUbjson::deserialize(&stream, &data))
        {
            NX_WARNING("migrateAccessRightsToUbjsonFormat", "Can't deserialize transaction from transaction log");
            return false;
        }

        QnTransaction<ApiIdData> transaction(
            ApiCommand::removeAccessRights,
            db->commonModule()->moduleGUID(),
            data);
        db->transactionLog()->fillPersistentInfo(transaction);
        if (db->transactionLog()->saveTransaction(transaction) != ErrorCode::ok)
            return false;

    }

    return true;
}

bool loadOldAccessRightList(const QSqlDatabase& database, ApiAccessRightsDataList& accessRightsList)
{
    QSqlQuery query(database);
    query.setForwardOnly(true);
    const QString queryStr = R"(
        SELECT rights.guid as userId, resource.guid as resourceId
        FROM vms_access_rights_tmp rights
        JOIN vms_resource resource on resource.id = rights.resource_ptr_id
        ORDER BY rights.guid
    )";
    
    if (!SqlQueryExecutionHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    if (!SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    ApiAccessRightsData current;
    while (query.next())
    {
        QnUuid userId = QnUuid::fromRfc4122(query.value(0).toByteArray());
        if (userId != current.userId)
        {
            if (!current.userId.isNull())
                accessRightsList.push_back(current);

            current.userId = userId;
            current.resourceIds.clear();
        }

        QnUuid resourceId = QnUuid::fromRfc4122(query.value(1).toByteArray());
        current.resourceIds.push_back(resourceId);
    }
    if (!current.userId.isNull())
        accessRightsList.push_back(current);
    
    return true;
}

bool migrateAccessRightsToUbjsonFormat(QSqlDatabase& database, detail::QnDbManager* db)
{
    if (!cleanupOldData(database))
        return false;
    if (!addNewRemoveUserAccessRights(database, db))
        return false;

    ApiAccessRightsDataList accessRightsList;
    if (!loadOldAccessRightList(database, accessRightsList))
        return false;

    for (const auto& data: accessRightsList)
    {
        QnTransaction<ApiAccessRightsData> tran(
            ApiCommand::setAccessRights,
            db->commonModule()->moduleGUID(),
            data);
        db->transactionLog()->fillPersistentInfo(tran);
        if (db->executeTransactionNoLock(tran, QnUbjson::serialized(tran)) != ErrorCode::ok)
            return false;
    }

    return SqlQueryExecutionHelper::execSQLScript(
        "DROP TABLE vms_access_rights_tmp",
        database);
}

} // namespace db
} // namespace ec2
