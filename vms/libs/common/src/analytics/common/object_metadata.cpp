#include "object_metadata.h"

#include <QtCore/QRegularExpression>

#include <nx/kit/debug.h>

#include <nx/fusion/model_functions.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/utils/log/log_message.h>

#include <utils/math/math.h>

namespace nx {
namespace common {
namespace metadata {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ObjectMetadata)(ObjectMetadataPacket),
    (json)(ubjson),
    _Fields,
    (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Attribute),
    (json)(ubjson)(xml)(csv_record),
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

bool operator==(const ObjectMetadata& left, const ObjectMetadata& right)
{
    return left.typeId == right.typeId
        && left.trackId == right.trackId
        && equalWithPrecision(left.boundingBox, right.boundingBox, kCoordinateDecimalDigits)
        && left.attributes == right.attributes;
}

QString toString(const ObjectMetadata& objectMetadata)
{
    QString s =
        "x " + QString::number(objectMetadata.boundingBox.x())
        + ", y " + QString::number(objectMetadata.boundingBox.y())
        + ", width " + QString::number(objectMetadata.boundingBox.width())
        + ", height " + QString::number(objectMetadata.boundingBox.height())
        + ", trackId " + objectMetadata.trackId.toString();

    if (objectMetadata.isBestShot())
        return s;

    s += ", typeId " + objectMetadata.typeId + ", attributes {";

    const QRegularExpression kExpectedCharsOnlyRegex("\\A[A-Za-z_0-9.]+\\z");

    bool isFirstAttribute = true;
    for (const auto& attribute: objectMetadata.attributes)
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
    return s;
}

//-------------------------------------------------------------------------------------------------

bool operator<(const ObjectMetadataPacket& first, const ObjectMetadataPacket& other)
{
    return first.timestampUs < other.timestampUs;
}

bool operator<(std::chrono::microseconds first, const ObjectMetadataPacket& second)
{
    return first.count() < second.timestampUs;
}

QString toString(const ObjectMetadataPacket& packet)
{
    QString s = lm("PTS: %1, durationUs: %2, deviceId: %3, streamIndex: %4, objects: %5\n")
        .args(
            packet.timestampUs,
            packet.durationUs,
            packet.deviceId,
            packet.streamIndex,
            packet.objectMetadataList.size());

    for (const auto& object: packet.objectMetadataList)
        s += "    " + toString(object) + "\n";

    return s;
}

::std::ostream& operator<<(::std::ostream& os, const ObjectMetadataPacket& packet)
{
    return os << toString(packet).toStdString();
}

QnCompressedMetadataPtr toCompressedMetadataPacket(
    const ObjectMetadataPacket& packet)
{
    auto metadataPacket = std::make_shared<QnCompressedMetadata>(
        MetadataType::ObjectDetection);
    metadataPacket->setTimestampUsec(packet.timestampUs);
    metadataPacket->setDurationUsec(packet.durationUs);
    metadataPacket->setData(QnUbjson::serialized(packet));
    return metadataPacket;
}

ObjectMetadataPacketPtr fromCompressedMetadataPacket(
    const QnConstCompressedMetadataPtr& compressedMetadata)
{
    if (!compressedMetadata)
        return ObjectMetadataPacketPtr();

    ObjectMetadataPacketPtr metadata(new ObjectMetadataPacket);

    *metadata = QnUbjson::deserialized<ObjectMetadataPacket>(
        QByteArray::fromRawData(compressedMetadata->data(), int(compressedMetadata->dataSize())));

    return metadata;
}

bool operator==(const ObjectMetadataPacket& left, const ObjectMetadataPacket& right)
{
    return left.deviceId == right.deviceId
        && left.timestampUs == right.timestampUs
        && left.durationUs == right.durationUs
        && left.objectMetadataList == right.objectMetadataList
        && left.streamIndex == right.streamIndex;
}

bool operator<(const ObjectMetadataPacket& first, std::chrono::microseconds second)
{
    return first.timestampUs < second.count();
}

std::vector<AttributeGroup> groupAttributes(const Attributes& attributes, int maxValuesInGroup)
{
    std::map<QString, int> tmp;
    std::vector<AttributeGroup> result;
    for (const auto& attribute: attributes)
    {
        auto itr = tmp.find(attribute.name);
        if (itr == tmp.end())
        {
            itr = tmp.emplace(attribute.name, result.size()).first;
            result.push_back(AttributeGroup());
        }
        auto& group = result[itr->second];
        group.name = attribute.name;
        if (group.values.size() < maxValuesInGroup)
            group.values.push_back(attribute.value);
        group.totalValues++;
    }

    return result;
}

} // namespace metadata
} // namespace common
} // namespace nx
