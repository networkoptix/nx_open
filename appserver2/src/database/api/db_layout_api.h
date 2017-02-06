#pragma once

#include <QtSql/QSqlDatabase>

#include <nx_ec/data/api_fwd.h>

namespace ec2 {
namespace database {
namespace api {

/**
 * @brief Get list of layouts from the database.
 * @param in database Target database
 * @param in id Filter id. If not null, only layout with this id will be returned.
 * @param out layouts Returns list of layouts.
 * @returns True if operation was successful, false otherwise.
 */
bool fetchLayouts(const QSqlDatabase& database, const QnUuid& id, ApiLayoutDataList& layouts);

/**
* @brief Add or update layout.
* @param in database Target database.
* @param in layout Layout api data.
* @returns True if operation was successful, false otherwise.
*/
bool saveLayout(const QSqlDatabase& database, const ApiLayoutData& layout);

// Will be moved to internal api as soon as possible.
bool removeLayoutItems(const QSqlDatabase& database, qint32 internalId);

} // namespace api
} // namespace database
} // namespace ec2
