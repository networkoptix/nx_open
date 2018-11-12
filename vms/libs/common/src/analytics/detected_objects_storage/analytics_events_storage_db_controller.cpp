#include "analytics_events_storage_db_controller.h"

#include "analytics_events_storage_db_schema.h"

namespace nx {
namespace analytics {
namespace storage {

DbController::DbController(
    const nx::sql::ConnectionOptions& connectionOptions)
    :
    base_type(connectionOptions)
{
    // Raising SELECT query priority so that those queries are not blocked by numerous INSERT.
    queryExecutor().setQueryPriority(
        sql::QueryType::lookup,
        sql::AsyncSqlQueryExecutor::kDefaultQueryPriority + 1);

    dbStructureUpdater().addUpdateScript(kCreateAnalyticsEventsSchema);
    dbStructureUpdater().addUpdateScript(kAnalyticsDbMoreIndexes);
    dbStructureUpdater().addUpdateScript(kAnalyticsDbEvenMoreIndexes);

    // TODO: #ak Rename object_id column after switching to SQLITE 3.25.0.
    // Before that renaming requires re-creating table and copying data
    // which can be quite expensive here.
    //dbStructureUpdater().addUpdateScript(
    //    "ALTER TABLE event RENAME COLUMN object_id TO object_appearance_id");
}

} // namespace storage
} // namespace analytics
} // namespace nx
