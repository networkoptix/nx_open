#pragma once

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

#include <nx_ec/data/api_fwd.h>

#include <memory>

namespace ec2 {
namespace database {
namespace api {

    // todo: get rid of this struct. Make function insertOrReplaceResource member of some class.
    struct Context
    {
        Context(const QSqlDatabase& db): database(db) {}
        void reset()
        {
            getIdQuery.reset();
            insQuery.reset();
            updQuery.reset();
        }

        std::unique_ptr<QSqlQuery> getIdQuery;
        std::unique_ptr<QSqlQuery> insQuery;
        std::unique_ptr<QSqlQuery> updQuery;
        const QSqlDatabase& database;

    };

/**
 * Get internal entry id from vms_resource table.
 * @param in database Target database.
 * @param in guid Unique id of the resource.
 * @returns 0 if resource is not found, valid internal id otherwise.
 */
qint32 getResourceInternalId(
    Context* context,
    const QnUuid& guid);

/**
 * Add or update resource.
 * @param in database Target database.
 * @param in data Resource api data.
 * @param out internalId Returns id of the inserted/updated entry.
 * @returns True if operation was successful, false otherwise.
 */
bool insertOrReplaceResource(
    Context* context,
    const ApiResourceData& data,
    qint32* internalId);

/**
* Delete resource from internal table.
* @param in database Target database.
* @param in internalId Resource internal id.
* @returns True if operation was successful, false otherwise.
*/
bool deleteResourceInternal(Context* context, int internalId);

} // namespace api
} // namespace database
} // namespace ec2
