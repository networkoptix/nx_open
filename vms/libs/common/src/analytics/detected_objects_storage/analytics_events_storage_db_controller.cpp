#include "analytics_events_storage_db_controller.h"

#include <nx/utils/log/log_message.h>

#include "analytics_events_storage_db_schema.h"
#include "analytics_events_storage_types.h"

namespace nx::analytics::storage {

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
    dbStructureUpdater().addUpdateScript(kMoveAttributesTextToASeparateTable);
    dbStructureUpdater().addUpdateScript(kMoveObjectTypeAndDeviceToSeparateTables);
    dbStructureUpdater().addUpdateScript(kConvertTimestampToMillis);
    dbStructureUpdater().addUpdateScript(lm(kPackCoordinates).args(kCoordinatesPrecision).toUtf8());
    dbStructureUpdater().addUpdateScript(kAddFullTimePeriods);
    if (kUseTrackAggregation)
        dbStructureUpdater().addUpdateScript(kSplitDataToObjectAndSearch);
}

} // namespace nx::analytics::storage
