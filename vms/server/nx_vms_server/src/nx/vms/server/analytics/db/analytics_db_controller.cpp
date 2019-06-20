#include "analytics_db_controller.h"

#include <nx/utils/log/log_message.h>

#include <analytics/db/analytics_db_types.h>
#include <analytics/db/config.h>

#include "analytics_db_db_schema.h"

namespace nx::analytics::db {

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
    dbStructureUpdater().addUpdateScript(kConvertDurationToMillis);
    if (kUseTrackAggregation)
    {
        dbStructureUpdater().addUpdateScript(kSplitDataToObjectAndSearch);
        dbStructureUpdater().addUpdateScript(kObjectBestShot);
        dbStructureUpdater().addUpdateScript(kCombinedAttributes);
    }
}

} // namespace nx::analytics::db
