#include "object_detection_metadata.h"

#include <nx/kit/debug.h>

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
    QString s = lit("PTS ") + QString::number(packet.timestampUsec)
        + lit(", durationUs ") + QString::number(packet.durationUsec)
        + lit(", deviceId ") + packet.deviceId.toString()
        + lit(", objects:\n");

    for (const auto& object: packet.objects)
    {
        s += lit("    x ") + QString::number(object.boundingBox.x())
            + lit(", y ") + QString::number(object.boundingBox.y())
            + lit(", w ") + QString::number(object.boundingBox.width())
            + lit(", h ") + QString::number(object.boundingBox.height())
            + lit(", id ") + object.objectId.toString()
            + lit(", typeId ") + object.objectTypeId.toString()
            + lit(", labels [");

        bool isLabelFirst = true;
        for (const auto& label: object.labels)
        {
            if (isLabelFirst)
            {
                s += lit(", ");
                isLabelFirst = false;
            }
            s += label.name + lit("=\"")
                + QString::fromStdString(
                    //< Properly escape the value string.
                    nx::kit::debug::toString(label.value.toUtf8().constData()))
                + lit("\"");
        }

        s += lit("]\n");
    }

    return s;
}

::std::ostream& operator<<(::std::ostream& os, const DetectionMetadataPacket& packet)
{
    return os << toString(packet).toStdString();
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

DetectionMetadataPacketPtr fromMetadataPacket(const QnCompressedMetadataPtr& compressedMetadata)
{
    if (!compressedMetadata)
        return DetectionMetadataPacketPtr();

    DetectionMetadataPacketPtr metadata(new DetectionMetadataPacket);

    *metadata = QnUbjson::deserialized<DetectionMetadataPacket>(
        QByteArray::fromRawData(compressedMetadata->data(), int(compressedMetadata->dataSize())));

    return metadata;
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
