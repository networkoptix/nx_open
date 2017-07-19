#pragma once

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

#include <nx_ec/data/api_fwd.h>

#include <memory>
#include <set>

namespace ec2 {
namespace database {
namespace api {

// TODO: Move out somewhere.
class QueryCache
{
public:
    class Pool
    {
    public:
        void reset();

    private:
        friend class QueryCache;
        std::set<QueryCache*> m_caches;
    };

    QueryCache(Pool* pool);
    ~QueryCache();

    class Guard: public std::unique_ptr<QSqlQuery, void(*)(QSqlQuery*)>
    {
    public:
        Guard(QSqlQuery* q = nullptr);
    };

    template<typename Init>
    Guard get(const QSqlDatabase& db, const Init& init)
    {
        if (!m_query)
        {
            m_query.reset(new QSqlQuery(db));
            if (!init(m_query.get()))
                m_query.reset();
        }

        return Guard(m_query.get());
    }

    Guard get(const QSqlDatabase& db, const char* query);

private:
    Pool* m_pool;
    std::unique_ptr<QSqlQuery> m_query;
};

class QueryContext
{
public:
    QueryContext(const QSqlDatabase& database, QueryCache::Pool* cachePool);
    const QSqlDatabase& database() const;

    template<typename Init>
    QueryCache::Guard getId(const Init& init) { return m_getId.get(m_database, init); }

    template<typename Init>
    QueryCache::Guard insert(const Init& init) { return m_insert.get(m_database, init); }

    template<typename Init>
    QueryCache::Guard update(const Init& init) { return m_update.get(m_database, init); }

private:
    const QSqlDatabase& m_database;
    QueryCache m_getId;
    QueryCache m_insert;
    QueryCache m_update;
};

// TODO: Those functions should probably be united in a class.

/**
 * Get internal entry id from vms_resource table.
 * @param in database Target database.
 * @param in guid Unique id of the resource.
 * @returns 0 if resource is not found, valid internal id otherwise.
 */
qint32 getResourceInternalId(
    QueryContext* context,
    const QnUuid& guid);

/**
 * Add or update resource.
 * @param in database Target database.
 * @param in data Resource api data.
 * @param out internalId Returns id of the inserted/updated entry.
 * @returns True if operation was successful, false otherwise.
 */
bool insertOrReplaceResource(
    QueryContext* context,
    const ApiResourceData& data,
    qint32* internalId);

/**
* Delete resource from internal table.
* @param in database Target database.
* @param in internalId Resource internal id.
* @returns True if operation was successful, false otherwise.
*/
bool deleteResourceInternal(QueryContext* context, int internalId);

} // namespace api
} // namespace database
} // namespace ec2
