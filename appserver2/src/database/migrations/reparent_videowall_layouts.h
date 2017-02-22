#pragma once

#include <QtSql/QSqlDatabase>

namespace ec2 {
namespace database {
namespace migrations {

/**
 * In 3.0 normal layouts cannot be placed on the videowall. Special layouts with parent id set to
 * videowall id are used. Therefore we must clone users' layouts and re-parent existing layouts
 * (if auto-generated) to videowall. Unused auto-generated layouts must be deleted.
 */
bool reparentVideoWallLayouts(const QSqlDatabase& database);

} // namespace migrations
} // namespace database
} // namespace ec2
