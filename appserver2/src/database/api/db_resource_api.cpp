#include "db_resource_api.h"

#include <QtSql/QSqlQuery>

#include <nx_ec/data/api_resource_data.h>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/assert.h>

#include <utils/db/db_helper.h>

namespace ec2 {
namespace database {
namespace api {

qint32 getResourceInternalId(
    Context* context,
    const QnUuid& guid)
{
    if (!context->getIdQuery)
    {
        context->getIdQuery.reset(new QSqlQuery(context->database));
        const QString queryStr = R"sql(SELECT id from vms_resource where guid = ?)sql";

        context->getIdQuery->setForwardOnly(true);
        if (!SqlQueryExecutionHelper::prepareSQLQuery(context->getIdQuery.get(), queryStr, Q_FUNC_INFO))
            return 0;
    }

    context->getIdQuery->addBindValue(guid.toRfc4122());
    if (!context->getIdQuery->exec() || !context->getIdQuery->next())
        return 0;

    return context->getIdQuery->value(0).toInt();
}

bool insertOrReplaceResource(
    Context* context,
    const ApiResourceData& data,
    qint32* internalId)
{
    NX_ASSERT(!data.id.isNull(), "Resource id must not be null");
    if (data.id.isNull())
        return false;

    *internalId = getResourceInternalId(context, data.id);
    QSqlQuery* query = nullptr;
    if (*internalId)
    {
        if (!context->updQuery)
        {
            context->updQuery.reset(new QSqlQuery(context->database));
            const QString queryStr = R"sql(
                UPDATE vms_resource
                SET xtype_guid = :typeId, parent_guid = :parentId, name = :name, url = :url
                WHERE id = :internalId
            )sql";

            if (!SqlQueryExecutionHelper::prepareSQLQuery(context->updQuery.get(), queryStr, Q_FUNC_INFO))
                return false;
        }
        context->updQuery->bindValue(":internalId", *internalId);
        query = context->updQuery.get();
    }
    else
    {
        if (!context->insQuery)
        {
            context->insQuery.reset(new QSqlQuery(context->database));
            const QString queryStr = R"sql(
                INSERT INTO vms_resource
                (guid, xtype_guid, parent_guid, name, url)
                VALUES
                (:id, :typeId, :parentId, :name, :url)
            )sql";

            if (!SqlQueryExecutionHelper::prepareSQLQuery(context->insQuery.get(), queryStr, Q_FUNC_INFO))
                return false;
        }
        query = context->insQuery.get();
    }
    QnSql::bind(data, query);

    if (!SqlQueryExecutionHelper::execSQLQuery(query, Q_FUNC_INFO))
        return false;

    if (*internalId == 0)
        *internalId = query->lastInsertId().toInt();

    return true;
}

bool deleteResourceInternal(Context* context, int internalId)
{
    const QString queryStr(R"sql(
        DELETE FROM vms_resource where id = ?
    )sql");

    QSqlQuery query(context->database);
    if (!SqlQueryExecutionHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    query.addBindValue(internalId);
    return SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO);
}

} // namespace api
} // namespace database
} // namespace ec2
