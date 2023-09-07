// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_metadata.h"

#include <algorithm>
#include <unordered_map>

#include <QtCore/QRegularExpression>

#include <nx/analytics/analytics_attributes.h>
#include <nx/fusion/model_functions.h>
#include <nx/kit/debug.h>
#include <nx/media/media_data_packet.h>
#include <nx/utils/log/format.h>
#include <nx/utils/math/math.h>

namespace nx {
namespace common {
namespace metadata {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ObjectMetadata, (json)(ubjson), ObjectMetadata_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ObjectMetadataPacket, (json)(ubjson), ObjectMetadataPacket_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    Attribute, (json)(ubjson)(xml)(csv_record), Attribute_Fields, (brief, true))

bool operator<(const Attribute& f, const Attribute& s)
{
    return std::tie(f.name, f.value) < std::tie(s.name, s.value);
}

bool operator==(const Attribute& left, const Attribute& right)
{
    return left.name == right.name
        && left.value == right.value;
}

bool operator==(const AttributeGroup& left, const AttributeGroup& right)
{
    return left.name == right.name
        && left.values == right.values;
}

QString toString(const Attribute& attribute)
{
    return nx::format("%1: %2").args(attribute.name, attribute.value);
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
    {
        if (!objectMetadata.imageUrl.isNull()) 
            s += ", imageUrl " + nx::kit::utils::toString(objectMetadata.imageUrl);
        if (!objectMetadata.imageDataFormat.isNull())
            s += ", imageDataFormat " + nx::kit::utils::toString(objectMetadata.imageDataFormat);
        if (objectMetadata.isImageDataPresent)
            s += ", imageData present";
        if (objectMetadata.isImageDataPresent || objectMetadata.imageDataSize != 0)
            s += ", imageDataSize " + std::to_string(objectMetadata.imageDataSize);
        return s;
    }

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
    QString s = nx::format("PTS: %1, durationUs: %2, deviceId: %3, streamIndex: %4, objects: %5\n")
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

GroupedAttributes groupAttributes(
    const Attributes& attributes, bool humanReadableNames, bool filterOutHidden)
{
    std::unordered_map<QString, GroupedAttributes::size_type> nameToIndex;
    GroupedAttributes result;

    nameToIndex.reserve(attributes.size());

    const auto preprocessValue =
        [](const QString& source)
        {
            static const QStringList kStandardValues = {"True", "False"};
            for (const auto& standardValue: kStandardValues)
            {
                if (source.compare(standardValue, Qt::CaseInsensitive) == 0)
                    return standardValue;
            }

            return source;
        };

    for (const auto& attribute: attributes)
    {
        if (filterOutHidden && nx::analytics::isAnalyticsAttributeHidden(attribute.name))
            continue;

        const auto [it, inserted] = nameToIndex.try_emplace(attribute.name, result.size());
        if (inserted)
            result.push_back(AttributeGroup{attribute.name});

        const auto value = humanReadableNames
            ? preprocessValue(attribute.value)
            : attribute.value;

        auto& group = result[it->second];
        group.values.push_back(value);
    }

    return result;
}

bool AttributeEx::isNumberOrRange(const QString& attributeName, const QString& attributeValue)
{
    const static std::string number(R"((?:-?(?:(?:\d+\.?\d*)|(?:\d*\.?\d+))))");
    const static std::string numberOrInf(R"((?:)" + number + R"(|inf))");
    const static std::string numberOrMinusInf(R"((?:)" + number + R"(|-inf))");
    const static std::regex pattern(R"(^(?:)" + number + R"(|(?:[\(\[]?)" + numberOrMinusInf
        + R"([ ]*\.\.\.[ ]*)" + numberOrInf + R"([\]\)]?))$)");
    bool result = std::regex_match(attributeValue.toStdString(), pattern);
    return result;
}

void AttributeEx::addRange(const NumericRange& range)
{
    if (auto currentRange = std::get_if<NumericRange>(&value))
    {
        if (currentRange->from && range.from)
        {
            currentRange->from->value = std::min(currentRange->from->value, range.from->value);
            currentRange->from->inclusive = currentRange->from->inclusive || range.from->inclusive;
        }
        else if (range.from)
        {
            currentRange->from = range.from;
        }

        if (currentRange->to && range.to)
        {
            currentRange->to->value = std::max(currentRange->to->value, range.to->value);
            currentRange->to->inclusive = currentRange->to->inclusive || range.to->inclusive;
        }
        else if (range.to)
        {
            currentRange->to = range.to;
        }
    }
    else
    {
        value = range;
    }
}

QString AttributeEx::stringValue() const
{
    if (auto strValue = std::get_if<QString>(&value))
        return *strValue;
    if (auto numericValue = std::get_if<NumericRange>(&value))
        return numericValue->stringValue();
    return QString();
}

AttributeEx::AttributeEx(const Attribute& attribute)
{
    name = attribute.name;

    const auto range = parseRangeFromValue(attribute.value);
    if (!range.isNull())
        value = range;
    else
        value = attribute.value;
}

const static QString kRangeDelimiter("...");

NumericRange AttributeEx::parseRangeFromValue(const QString& value)
{

    bool ok = false;
    float floatValue = value.toFloat(&ok);
    if (ok)
        return NumericRange(floatValue);

    bool isValidRange = value.contains(kRangeDelimiter);
    if (!isValidRange)
        return NumericRange();

    std::optional<RangePoint> left, right;

    auto unquotedValue = value;
    if (unquotedValue.startsWith('(') || unquotedValue.startsWith('['))
        unquotedValue = unquotedValue.mid(1);
    if (unquotedValue.endsWith(')') || unquotedValue.endsWith(']'))
        unquotedValue.chop(1);

    const auto params = QStringView(unquotedValue).split(kRangeDelimiter);
    if (params.size() != 2)
        return NumericRange(); //< Invalid value
    if (params[0] != QLatin1String("-inf"))
    {
        left = RangePoint();
        left->value = params[0].toFloat(&ok);
        left->inclusive = !value.startsWith('(');
        if (!ok)
            return NumericRange(); //< Invalid value
    }
    if (params[1] != QLatin1String("inf"))
    {
        right = RangePoint();
        right->value = params[1].toFloat(&ok);
        right->inclusive = !value.endsWith(')');
        if (!ok)
            return NumericRange(); //< Invalid value
    }
    return NumericRange(left, right);
}

bool NumericRange::intersects(const NumericRange& range) const
{
    RangePoint ownLeft = from ? *from : RangePoint{std::numeric_limits<float>::min()};
    RangePoint ownRight = to ? *to: RangePoint{std::numeric_limits<float>::max()};

    RangePoint rangeLeft = range.from ? *range.from : RangePoint{std::numeric_limits<float>::min()};
    RangePoint rangeRight = range.to ? *range.to : RangePoint{std::numeric_limits<float>::max()};

    RangePoint maxLeft = (ownLeft.value > rangeLeft.value) ? ownLeft : rangeLeft;
    RangePoint minRight = (ownRight.value < rangeRight.value) ? ownRight : rangeRight;

    if (maxLeft.inclusive && minRight.inclusive)
        return maxLeft.value <= minRight.value;
    return maxLeft.value < minRight.value;
}

bool NumericRange::hasRange(const NumericRange& range) const
{
    RangePoint ownLeft = from ? *from : RangePoint{ std::numeric_limits<float>::min() };
    RangePoint ownRight = to ? *to : RangePoint{ std::numeric_limits<float>::max() };

    RangePoint rangeLeft = range.from ? *range.from : RangePoint{ std::numeric_limits<float>::min() };
    RangePoint rangeRight = range.to ? *range.to : RangePoint{ std::numeric_limits<float>::max() };

    if (ownLeft.inclusive || !rangeLeft.inclusive)
    {
        if (ownLeft.value > rangeLeft.value)
            return false;
    }
    else
    {
        if (ownLeft.value >= rangeLeft.value)
            return false;
    }

    if (ownRight.inclusive || !rangeRight.inclusive)
    {
        if (ownRight.value < rangeRight.value)
            return false;
    }
    else
    {
        if (ownRight.value <= rangeRight.value)
            return false;
    }
    return true;
}

QString NumericRange::stringValue() const
{
    if (from && to && from->value == to->value)
        return QString::number(from->value);
    else if (from && from->inclusive && to && to->inclusive)
        return NX_FMT("%1 ... %2", from->value, to->value);

    return NX_FMT("%1%2 ... %3%4",
        from && from->inclusive ? '[' : '(',
        from ? from->value : -std::numeric_limits<float>::infinity(),
        to ? to->value : std::numeric_limits<float>::infinity(),
        to && to->inclusive ? ']' : ')');

}

QnAbstractMediaData* QnCompressedObjectMetadataPacket::clone() const
{
    auto cloned = new QnCompressedObjectMetadataPacket(metadataType);
    cloned->assign(this);
    return cloned;
}

void QnCompressedObjectMetadataPacket::assign(const QnCompressedObjectMetadataPacket* other)
{
    base_type::assign(other);
    packet = other->packet;
}

void addAttributeIfNotExists(Attributes* result, const Attribute& a)
{
    Attributes::iterator insPos = result->end();
    for (auto it = result->begin(); it != result->end(); ++it)
    {
        const auto& attribute = *it;
        if (attribute.name == a.name)
        {
            if (attribute.value == a.value)
                return;
            insPos = it;
        }
    }
    if (insPos == result->end())
        result->push_back(a);
    else
        result->insert(std::next(insPos), a);
}

Attributes::iterator findFirstAttributeByName(Attributes* a, const QString& name)
{
    return std::find_if(a->begin(), a->end(),
        [&name](const auto& attribute) { return attribute.name == name; });
}

Attributes::const_iterator findFirstAttributeByName(const Attributes* a, const QString& name)
{
    return std::find_if(a->cbegin(), a->cend(),
        [&name](const auto& attribute) { return attribute.name == name; });
}

} // namespace metadata
} // namespace common
} // namespace nx
