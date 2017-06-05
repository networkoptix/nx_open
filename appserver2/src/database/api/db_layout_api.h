#pragma once

#include <QtSql/QSqlDatabase>

#include <nx_ec/data/api_fwd.h>
#include <database/api/db_resource_api.h>

namespace ec2 {
namespace database {
namespace api {

/**
 * Get list of layouts from the database.
 * @param in database Target database
 * @param in id Filter id. If not null, only layout with this id will be returned.
 * @param out layouts Returns list of layouts.
 * @returns True if operation was successful, false otherwise.
 */
bool fetchLayouts(const QSqlDatabase& database, const QnUuid& id, ApiLayoutDataList& layouts);

/**
* Add or update layout.
* @param in database Target database.
* @param in layout Layout api data.
* @returns True if operation was successful, false otherwise.
*/
bool saveLayout(
    ec2::database::api::Context* resourceContext,
    const ApiLayoutData& layout);

/**
* Remove layout.
* @param in database Target database.
* @param in guid Layout id.
* @returns True if operation was successful, false otherwise.
*/
bool removeLayout(
    ec2::database::api::Context* resourceContext,
    const QnUuid& id);

} // namespace api
} // namespace database
} // namespace ec2
