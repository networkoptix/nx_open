#include "object_detection_metadata.h"

#include <tuple>

#include <nx/fusion/model_functions.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/utils/log/log_message.h>

namespace nx {
namespace common {
namespace metadata {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    QN_OBJECT_DETECTION_TYPES,
    (json)(ubjson),
    _Fields,
    (brief, true))

bool operator<(const Attribute& f, const Attribute& s)
{
    return std::tie(f.name, f.value) < std::tie(s.name, s.value);
}

bool operator==(const Attribute& left, const Attribute& right)
{
    return left.name == right.name
        && left.value == right.value;
}

QString toString(const Attribute& attribute)
{
    return lm("%1: %2").args(attribute.name, attribute.value);
}

//-------------------------------------------------------------------------------------------------

bool operator==(const DetectedObject& left, const DetectedObject& right)
{
    return left.objectTypeId == right.objectTypeId
        && left.objectId == right.objectId
        && left.boundingBox == right.boundingBox
        && left.labels == right.labels;
}

//-------------------------------------------------------------------------------------------------

bool operator<(const DetectionMetadataPacket& first, const DetectionMetadataPacket& other)
{
    return first.timestampUsec < other.timestampUsec;
}

bool operator<(std::chrono::microseconds first, const DetectionMetadataPacket& second)
{
    return first.count() < second.timestampUsec;
}

QString toString(const DetectionMetadataPacket& packet)
{
    return lm("deviceId = %1, timestampUsec = %2, durationUsec = %3")
        .args(packet.deviceId, packet.timestampUsec, packet.durationUsec);
}

QnCompressedMetadataPtr toMetadataPacket(
    const DetectionMetadataPacket& detectionPacket)
{
    auto metadataPacket = std::make_shared<QnCompressedMetadata>(
        MetadataType::ObjectDetection);
    metadataPacket->setTimestampUsec(detectionPacket.timestampUsec);
    metadataPacket->setDurationUsec(detectionPacket.durationUsec);
    metadataPacket->setData(QnUbjson::serialized(detectionPacket));
    return metadataPacket;
}

bool operator==(const DetectionMetadataPacket& left, const DetectionMetadataPacket& right)
{
    return left.deviceId == right.deviceId
        && left.timestampUsec == right.timestampUsec
        && left.durationUsec == right.durationUsec
        && left.objects == right.objects;
}

bool operator<(const DetectionMetadataPacket& first, std::chrono::microseconds second)
{
    return first.timestampUsec < second.count();
}

} // namespace metadata
} // namespace common
} // namespace nx
