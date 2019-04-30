#include "analytics_archive.h"

namespace nx::vms::server::metadata {

static const int kAggregationIntervalSeconds = 5;

AnalyticsArchive::AnalyticsArchive(const QString& dataDir, const QString& uniqueId):
    MetadataArchive(
        "analytics",
        kGridDataSize + 2 * 4, //< recordSize
        kAggregationIntervalSeconds,
        dataDir,
        uniqueId,
        0 //< Video channel is not used yet
    )
{

}

} // namespace nx::vms::server::metadata
