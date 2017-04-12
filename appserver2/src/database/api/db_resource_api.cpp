#include "db_resource_api.h"

#include <QtSql/QSqlQuery>

#include <nx_ec/data/api_resource_data.h>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/assert.h>

#include <utils/db/db_helper.h>

namespace ec2 {
namespace database {
namespace api {

qint32 getResourceInternalId(const QSqlDatabase& database, const QnUuid& guid)
{
    const QString queryStr = R"sql(
        SELECT id from vms_resource where guid = ?
    )sql";

    QSqlQuery query(database);
    query.setForwardOnly(true);
    if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return 0;

    query.addBindValue(guid.toRfc4122());
    if (!query.exec() || !query.next())
        return 0;

    return query.value(0).toInt();
}

bool insertOrReplaceResource(
    const QSqlDatabase& database,
    const ApiResourceData& data,
    qint32* internalId)
{
    NX_ASSERT(!data.id.isNull(), "Resource id must not be null");
    if (data.id.isNull())
        return false;

    *internalId = getResourceInternalId(database, data.id);
    QSqlQuery query(database);
    if (*internalId)
    {
        const QString queryStr = R"sql(
            UPDATE vms_resource
            SET guid = :id, xtype_guid = :typeId, parent_guid = :parentId, name = :name, url = :url
            WHERE id = :internalId
        )sql";

        if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
            return false;

        query.bindValue(":internalId", *internalId);
    }
    else
    {
        const QString queryStr = R"sql(
            INSERT INTO vms_resource
            (guid, xtype_guid, parent_guid, name, url)
            VALUES
            (:id, :typeId, :parentId, :name, :url)
        )sql";

        if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
            return false;
    }
    QnSql::bind(data, &query);

    if (!QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    if (*internalId == 0)
        *internalId = query.lastInsertId().toInt();

    return true;
}

bool deleteResourceInternal(const QSqlDatabase& database, int internalId)
{
    const QString queryStr(R"sql(
        DELETE FROM vms_resource where id = ?
    )sql");

    QSqlQuery query(database);
    if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    query.addBindValue(internalId);
    return QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO);
}

} // namespace api
} // namespace database
} // namespace ec2
