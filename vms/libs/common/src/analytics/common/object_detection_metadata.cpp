#include "object_detection_metadata.h"

#include <nx/kit/debug.h>

#include <nx/fusion/model_functions.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/utils/log/log_message.h>

#include <utils/math/math.h>

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
        && equalWithPrecision(left.boundingBox, right.boundingBox, kCoordinateDecimalDigits)
        && left.labels == right.labels;
}

QString toString(const DetectedObject& object)
{
    QString s =
        "x " + QString::number(object.boundingBox.x())
        + ", y " + QString::number(object.boundingBox.y())
        + ", width " + QString::number(object.boundingBox.width())
        + ", height " + QString::number(object.boundingBox.height())
        + ", id " + object.objectId.toString()
        + ", typeId " + object.objectTypeId
        + ", attributes {";

    const QRegularExpression kExpectedCharsOnlyRegex("\\A[A-Za-z_0-9.]+\\z");

    bool isFirstAttribute = true;
    for (const auto& attribute: object.labels)
    {
        if (isFirstAttribute)
            isFirstAttribute = false;
        else
            s += ", ";

        // If attribute.name is empty or contains chars other than letters, underscores, digits, or
        // dots, then properly enquote it with escaping.
        if (kExpectedCharsOnlyRegex.match(attribute.name).hasMatch())
            s += attribute.name;
        else
            s += nx::kit::utils::toString(attribute.name).c_str();

        s += ": ";
        // Properly enquote the value string with escaping.
        s += nx::kit::utils::toString(attribute.value.toUtf8().constData()).c_str();
    }

    s += "}";
    s += QString(", isBestShot ") + (object.bestShot ? "true" : "false");
    return s;
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
        + lit(", objects: ") + QString::number(packet.objects.size()) + lit("\n");

    for (const auto& object: packet.objects)
        s += "    " + toString(object) + "\n";

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

DetectionMetadataPacketPtr fromMetadataPacket(const QnConstCompressedMetadataPtr& compressedMetadata)
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
