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
    common::metadata::DetectionMetadataPacketPtr resultPacket;
    m_nextPacket.swap(resultPacket);

    for (;;)
    {
        auto detectedObject = m_dbCursor->next();
        if (!detectedObject)
            return resultPacket;

        NX_ASSERT(detectedObject->track.size() == 1, lm("%1").args(detectedObject->track.size()));

        if (!resultPacket)
        {
            resultPacket = toDetectionMetadataPacket(std::move(*detectedObject));
            continue;
        }

        if (detectedObject->track.front().timestampUsec == resultPacket->timestampUsec &&
            detectedObject->track.front().deviceId == resultPacket->deviceId)
        {
            addToPacket(std::move(*detectedObject), resultPacket.get());
            continue;
        }

        m_nextPacket = toDetectionMetadataPacket(std::move(*detectedObject));
        return resultPacket;
    }
}

common::metadata::DetectionMetadataPacketPtr Cursor::toDetectionMetadataPacket(
    DetectedObject detectedObject)
{
    auto packet = std::make_shared<common::metadata::DetectionMetadataPacket>();

    packet->deviceId = detectedObject.track.front().deviceId;
    packet->timestampUsec = detectedObject.track.front().timestampUsec;
    packet->durationUsec = detectedObject.track.front().durationUsec;
    packet->objects.push_back(toMetadataObject(std::move(detectedObject)));
    return packet;
}

void Cursor::addToPacket(
    DetectedObject detectedObject,
    common::metadata::DetectionMetadataPacket* packet)
{
    packet->durationUsec =
        std::max(packet->durationUsec, detectedObject.track.front().durationUsec);
    packet->objects.push_back(toMetadataObject(std::move(detectedObject)));
}

nx::common::metadata::DetectedObject Cursor::toMetadataObject(
    DetectedObject detectedObject)
{
    nx::common::metadata::DetectedObject result;
    result.objectTypeId = std::move(detectedObject.objectTypeId);
    result.objectId = std::move(detectedObject.objectId);
    result.boundingBox = std::move(detectedObject.track.front().boundingBox);
    result.labels = std::move(detectedObject.attributes);
    return result;
}

} // namespace storage
} // namespace analytics
} // namespace nx
