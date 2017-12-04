#include "analytics_events_storage_cursor.h"

namespace nx {
namespace analytics {
namespace storage {

Cursor::Cursor(std::unique_ptr<nx::utils::db::Cursor<DetectedObject>> dbCursor):
    m_dbCursor(std::move(dbCursor))
{
}

common::metadata::ConstDetectionMetadataPacketPtr Cursor::next()
{
    auto result = m_dbCursor->next();
    if (!result)
        return nullptr;
    return toDetectionMetadataPacket(std::move(*result));
}

common::metadata::DetectionMetadataPacketPtr Cursor::toDetectionMetadataPacket(
    DetectedObject detectedObject)
{
    // TODO: #ak

    auto packet = std::make_shared<common::metadata::DetectionMetadataPacket>();

    packet->deviceId = detectedObject.track.front().deviceId;
    packet->timestampUsec = detectedObject.track.front().timestampUsec;
    packet->durationUsec = detectedObject.track.front().durationUsec;
    packet->objects.push_back(common::metadata::DetectedObject());

    packet->objects.back().objectTypeId = detectedObject.objectTypeId;
    packet->objects.back().objectId = detectedObject.objectId;
    packet->objects.back().boundingBox = detectedObject.track.front().boundingBox;
    packet->objects.back().labels = detectedObject.attributes;

    return packet;
}

} // namespace storage
} // namespace analytics
} // namespace nx
