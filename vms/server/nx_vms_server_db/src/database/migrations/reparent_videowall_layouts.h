#pragma once

#include <QtSql/QSqlDatabase>

namespace ec2 {
namespace database {
namespace api {
class QueryContext;
}

namespace migrations {

/**
 * In 3.0 normal layouts cannot be placed on the videowall. Special layouts with parent id set to
 * videowall id are used. Therefore we must re-parent existing layouts to videowall.
 * Unused auto-generated layouts will be deleted from the client side automatically.
 * Cloning and deleting here is forbidden due to synchronization issues.
 */
bool reparentVideoWallLayouts(ec2::database::api::QueryContext* context);

} // namespace migrations
} // namespace database
} // namespace ec2
