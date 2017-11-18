#include "analytics_events_storage_db_controller.h"

#include "analytics_events_storage_db_schema.h"

namespace nx {
namespace mediaserver {
namespace analytics {
namespace storage {

DbController::DbController(
    const nx::utils::db::ConnectionOptions& connectionOptions)
    :
    base_type(connectionOptions)
{
    dbStructureUpdater().addUpdateScript(kCreateAnalyticsEventsSchema);
}

} // namespace storage
} // namespace analytics
} // namespace mediaserver
} // namespace nx
