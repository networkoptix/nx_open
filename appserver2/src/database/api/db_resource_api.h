#pragma once

#include <QtSql/QSqlDatabase>

#include <nx_ec/data/api_fwd.h>

namespace ec2 {
namespace database {
namespace api {

/**
 * Get internal entry id from vms_resource table.
 * @param in database Target database.
 * @param in guid Unique id of the resource.
 * @returns 0 if resource is not found, valid internal id otherwise.
 */
qint32 getResourceInternalId(
    const QSqlDatabase& database,
    const QnUuid& guid);

/**
 * Add or update resource.
 * @param in database Target database.
 * @param in data Resource api data.
 * @param out internalId Returns id of the inserted/updated entry.
 * @returns True if operation was successful, false otherwise.
 */
bool insertOrReplaceResource(
    const QSqlDatabase& database,
    const ApiResourceData& data,
    qint32* internalId);

/**
* Delete resource from internal table.
* @param in database Target database.
* @param in internalId Resource internal id.
* @returns True if operation was successful, false otherwise.
*/
bool deleteResourceInternal(const QSqlDatabase& database, int internalId);

} // namespace api
} // namespace database
} // namespace ec2
