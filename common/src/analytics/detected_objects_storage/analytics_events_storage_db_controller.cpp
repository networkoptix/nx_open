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
    // Raising SELECT query priority so that those queries are not blocked by numerous INSERT.
    queryExecutor().setQueryPriority(
        utils::db::QueryType::lookup,
        utils::db::AsyncSqlQueryExecutor::kDefaultQueryPriority + 1);

    dbStructureUpdater().addUpdateScript(kCreateAnalyticsEventsSchema);
    dbStructureUpdater().addUpdateScript(kAnalyticsDbMoreIndexes);
    dbStructureUpdater().addUpdateScript(kAnalyticsDbEvenMoreIndexes);
}

} // namespace storage
} // namespace analytics
} // namespace nx
