#pragma once

#include <QtSql/QSqlDatabase>

#include <nx_ec/data/api_fwd.h>

namespace ec2 {
namespace database {
namespace api {

/**
 * Get list of layouts from the database.
 * @param in database Target database
 * @param out tours Returns list of layout tours.
 * @returns True if operation was successful, false otherwise.
 */
bool fetchLayoutTours(const QSqlDatabase& database, ApiLayoutTourDataList& tours);

/**
* Add or update layout.
* @param in database Target database.
* @param in layout Layout api data.
* @returns True if operation was successful, false otherwise.
*/
//bool saveLayout(const QSqlDatabase& database, const ApiLayoutData& layout);

/**
* Remove layout.
* @param in database Target database.
* @param in guid Layout id.
* @returns True if operation was successful, false otherwise.
*/
//bool removeLayout(const QSqlDatabase& database, const QnUuid& id);

} // namespace api
} // namespace database
} // namespace ec2
