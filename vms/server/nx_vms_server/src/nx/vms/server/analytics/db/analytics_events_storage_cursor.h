#pragma once

#include <memory>

#include <nx/sql/async_sql_query_executor.h>
#include <nx/sql/sql_cursor.h>

#include <analytics/common/object_detection_metadata.h>
#include <analytics/db/abstract_cursor.h>
#include <analytics/db/analytics_events_storage_types.h>

namespace nx::analytics::db::deprecated {

class Cursor:
    public AbstractCursor
{
public:
    Cursor(std::unique_ptr<nx::sql::Cursor<DetectedObject>> dbCursor);

    virtual common::metadata::ConstDetectionMetadataPacketPtr next() override;

    virtual void close() override;

private:
    std::unique_ptr<nx::sql::Cursor<DetectedObject>> m_dbCursor;
    common::metadata::DetectionMetadataPacketPtr m_nextPacket;
    bool m_closed { false };
    std::mutex m_mutex;

    common::metadata::DetectionMetadataPacketPtr toDetectionMetadataPacket(
        DetectedObject detectedObject);

    void addToPacket(
        DetectedObject detectedObject,
        common::metadata::DetectionMetadataPacket* packet);

    nx::common::metadata::DetectedObject toMetadataObject(DetectedObject detectedObject);
};

} // namespace nx::analytics::db::deprecated
