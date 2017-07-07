#pragma once

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

#include <nx_ec/data/api_fwd.h>

#include <memory>

namespace ec2 {
namespace database {
namespace api {

class QueryCache
{
public:
    class Guard: public std::unique_ptr<QSqlQuery, void(*)(QSqlQuery*)>
    {
        static void finish(QSqlQuery* q) { q->finish(); }

    public:
        Guard(QSqlQuery* q = nullptr):
            std::unique_ptr<QSqlQuery, void(*)(QSqlQuery*)>(q, &finish) {}
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

    Guard get(const QSqlDatabase& db, const char* query)
    {
        return get(db, [&](QSqlQuery* q) { q->prepare(query); return true; });
    }

    void reset() { m_query.reset(); }

private:
    std::unique_ptr<QSqlQuery> m_query;
};

class QueryContext
{
public:
    QueryContext(const QSqlDatabase& database): m_database(database) {}
    const QSqlDatabase& database() { return m_database; }

    template<typename Init>
    QueryCache::Guard getId(const Init& init) { return m_getId.get(m_database, init); }

    template<typename Init>
    QueryCache::Guard insert(const Init& init) { return m_insert.get(m_database, init); }

    template<typename Init>
    QueryCache::Guard update(const Init& init) { return m_update.get(m_database, init); }

    void reset()
    {
        m_getId.reset();
        m_insert.reset();
        m_update.reset();
    }

private:
    QueryCache m_getId;
    QueryCache m_insert;
    QueryCache m_update;
    const QSqlDatabase& m_database;
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
