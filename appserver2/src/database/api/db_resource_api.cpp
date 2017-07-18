#include "db_resource_api.h"

#include <QtSql/QSqlQuery>

#include <nx_ec/data/api_resource_data.h>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/assert.h>

#include <utils/db/db_helper.h>

namespace ec2 {
namespace database {
namespace api {

void QueryCache::Pool::reset()
{
    for (const auto& c: m_caches)
        c->m_query.reset();
}

QueryCache::QueryCache(Pool* pool):
    m_pool(pool)
{
    m_pool->m_caches.insert(this);
}

QueryCache::~QueryCache()
{
    m_pool->m_caches.erase(this);
}

static void finish(QSqlQuery* q)
{
    q->finish();
}

QueryCache::Guard::Guard(QSqlQuery* q):
    std::unique_ptr<QSqlQuery, void(*)(QSqlQuery*)>(q, &finish)
{
}

QueryCache::Guard QueryCache::get(const QSqlDatabase& db, const char* query)
{
    return get(db, [&](QSqlQuery* q) { q->prepare(query); return true; });
}

QueryContext::QueryContext(const QSqlDatabase& database, QueryCache::Pool* cachePool):
    m_database(database),
    m_getId(cachePool),
    m_insert(cachePool),
    m_update(cachePool)
{
}

const QSqlDatabase& QueryContext::database() const
{
    return m_database;
}

qint32 getResourceInternalId(
    QueryContext* context,
    const QnUuid& guid)
{
    const auto query = context->getId(
        [&](QSqlQuery* q)
        {
            const QString s = R"sql(SELECT id from vms_resource where guid = ?)sql";
            q->setForwardOnly(true);
            return nx::utils::db::SqlQueryExecutionHelper::prepareSQLQuery(q, s, Q_FUNC_INFO);
        });

    if (!query)
        return 0;

    query->addBindValue(guid.toRfc4122());
    if (!query->exec() || !query->next())
        return 0;

    return query->value(0).toInt();
}

bool insertOrReplaceResource(
    QueryContext* context,
    const ApiResourceData& data,
    qint32* internalId)
{
    NX_ASSERT(!data.id.isNull(), "Resource id must not be null");
    if (data.id.isNull())
        return false;

    *internalId = getResourceInternalId(context, data.id);
    QueryCache::Guard query;
    if (*internalId)
    {
        query = context->update(
            [&](QSqlQuery* q)
            {
                const QString s = R"sql(
                    UPDATE vms_resource
                    SET xtype_guid = :typeId, parent_guid = :parentId, name = :name, url = :url
                    WHERE id = :internalId
                )sql";

                return nx::utils::db::SqlQueryExecutionHelper::prepareSQLQuery(q, s, Q_FUNC_INFO);
            });

        if (query)
            query->bindValue(":internalId", *internalId);
    }
    else
    {
        query = context->insert(
            [&](QSqlQuery* q)
            {
                const QString s = R"sql(
                    INSERT INTO vms_resource (guid, xtype_guid, parent_guid, name, url)
                    VALUES (:id, :typeId, :parentId, :name, :url)
                )sql";

                return nx::utils::db::SqlQueryExecutionHelper::prepareSQLQuery(q, s, Q_FUNC_INFO);
            });
    }

    if (!query)
        return false;

    QnSql::bind(data, query.get());
    if (!nx::utils::db::SqlQueryExecutionHelper::execSQLQuery(query.get(), Q_FUNC_INFO))
        return false;

    if (*internalId == 0)
        *internalId = query->lastInsertId().toInt();

    return true;
}

bool deleteResourceInternal(QueryContext* context, int internalId)
{
    const QString queryStr(R"sql(DELETE FROM vms_resource where id = ?)sql");
    QSqlQuery query(context->database());
    if (!nx::utils::db::SqlQueryExecutionHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    query.addBindValue(internalId);
    return nx::utils::db::SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO);
}

} // namespace api
} // namespace database
} // namespace ec2
