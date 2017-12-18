#include "analytics_events_storage_db_controller.h"

#include "analytics_events_storage_db_schema.h"

namespace nx {
namespace analytics {
namespace storage {

DbController::DbController(
    const nx::utils::db::ConnectionOptions& connectionOptions)
    :
    base_type(connectionOptions)
{
    dbStructureUpdater().addUpdateScript(kCreateAnalyticsEventsSchema);
    dbStructureUpdater().addUpdateScript(kAnalyticsDbMoreIndexes);
}

} // namespace storage
} // namespace analytics
} // namespace nx
