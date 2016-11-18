#pragma once

#include <QtSql/QSqlDatabase>

#include <nx_ec/data/api_fwd.h>

namespace ec2 {
namespace database {
namespace api {

/** Get list of layouts from the database. If id is provided, only layout with this id will be
 *  returned.
 */
bool fetchLayouts(const QSqlDatabase& database, const QnUuid& id, ApiLayoutDataList& layouts);

bool saveLayout(const QSqlDatabase& database, const ApiLayoutData& layout);

bool removeLayoutItems(const QSqlDatabase& database, qint32 internalId);

} // namespace api
} // namespace database
} // namespace ec2
