#pragma once

#include <memory>

#include <nx/utils/db/async_sql_query_executor.h>
#include <nx/utils/db/sql_cursor.h>

#include <analytics/common/object_detection_metadata.h>

#include "analytics_events_storage_types.h"

namespace nx {
namespace analytics {
namespace storage {

class AbstractCursor
{
public:
    virtual ~AbstractCursor() = default;

    virtual common::metadata::ConstDetectionMetadataPacketPtr next() = 0;
};

//-------------------------------------------------------------------------------------------------
class Cursor:
    public AbstractCursor
{
public:
    Cursor(std::unique_ptr<nx::utils::db::Cursor<DetectedObject>> dbCursor);

    virtual common::metadata::ConstDetectionMetadataPacketPtr next() override;

private:
    std::unique_ptr<nx::utils::db::Cursor<DetectedObject>> m_dbCursor;
    common::metadata::DetectionMetadataPacketPtr m_nextPacket;

    common::metadata::DetectionMetadataPacketPtr toDetectionMetadataPacket(
        DetectedObject detectedObject);
    void addToPacket(
        DetectedObject detectedObject,
        common::metadata::DetectionMetadataPacket* packet);
    nx::common::metadata::DetectedObject toMetadataObject(DetectedObject detectedObject);
};

} // namespace storage
} // namespace analytics
} // namespace nx
