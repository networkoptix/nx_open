#include "metadata_parser.h"

#include <utility>

#include <nx/utils/log/log_main.h>

namespace nx::vms_server_plugins::analytics::hikvision {

using namespace std::literals;
using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace {

constexpr double kCoordinateDomain = 1000;
constexpr auto kCacheEntryLifetime = 1s;

} // namespace

Ptr<ObjectMetadataPacket> MetadataParser::parsePacket(QByteArray bytes)
{
    evictStaleCacheEntries();

    // XXX: Strip garbage off both ends. Its presence is possibly a server bug.
    int begin = std::max(0, bytes.indexOf('<'));
    int end = bytes.lastIndexOf('>') + 1;
    bytes = bytes.mid(begin, std::max(0, end - begin));

    m_xml.clear();
    m_xml.addData(bytes);

    //std::cerr << "-----\n" << xml.toString(2).toStdString() << "-----\n" << std::endl;
    
    Ptr<ObjectMetadataPacket> packet;

    if (m_xml.readNextStartElement())
    {
        if (m_xml.name() == "Metadata")
        {
            packet = parseMetadataElement();
        }
        else
        {
            NX_DEBUG(NX_SCOPE_TAG, "Metadata XML root element is not 'Metadata'");
            m_xml.skipCurrentElement();
        }
    }
    if (m_xml.hasError())
    {
        NX_DEBUG(NX_SCOPE_TAG, "Failed to parse metadata XML at %1:%2: %3 [[[%4]]]",
            m_xml.lineNumber(), m_xml.columnNumber(), m_xml.errorString(), bytes);
    }

    return packet;
}

std::optional<QString> MetadataParser::parseStringElement()
{
    QString string = m_xml.readElementText();
    if (m_xml.hasError())
        return std::nullopt;

    return string;
}

std::optional<int> MetadataParser::parseIntElement()
{
    const auto string = parseStringElement();
    if (!string)
        return std::nullopt;

    bool ok = false;
    int value = string->toInt(&ok);
    if (!ok)
    {
        NX_WARNING(NX_SCOPE_TAG, "Failed to parse int: %1", *string);
        return std::nullopt;
    }

    return value;
}

std::optional<Uuid> MetadataParser::parseTargetIdElement()
{
    const auto value = parseIntElement();
    if (!value)
        return std::nullopt;

    Uuid uuid;
    std::memcpy(uuid.data(), &*value, std::min((int) sizeof(*value), uuid.size()));
    return uuid;
}

std::optional<std::string> MetadataParser::parseRecognitionElement()
{
    const auto value = parseStringElement();
    if (!value)
        return std::nullopt;

    return NX_FMT("nx.hikvision.%1", *value).toStdString();
}

std::optional<float> MetadataParser::parseCoordinateElement()
{
    const auto value = parseIntElement();
    if (!value)
        return std::nullopt;

    return *value / kCoordinateDomain;
}

std::optional<Point> MetadataParser::parsePointElement()
{
    std::optional<float> x;
    std::optional<float> y;

    bool seenX = false;
    bool seenY = false;

    while (m_xml.readNextStartElement())
    {
        if (m_xml.name() == "x")
        {
            seenX = true;
            x = parseCoordinateElement();
        }
        else if (m_xml.name() == "y")
        {
            seenY = true;
            y = parseCoordinateElement();
        }
        else
        {
            m_xml.skipCurrentElement();
        }
    }
    if (m_xml.hasError())
        return std::nullopt;

    if (!seenX)
        NX_DEBUG(NX_SCOPE_TAG, "No 'x' element in 'Point' element");
    if (!seenY)
        NX_DEBUG(NX_SCOPE_TAG, "No 'y' element in 'Point' element");

    if (!x || !y)
        return std::nullopt;

    return Point{*x, *y};
}

std::optional<MetadataParser::MinMaxRect> MetadataParser::parseRegionElement()
{
    Point min = {1, 1};
    Point max = {0, 0};

    bool seenPoint = false;

    while (m_xml.readNextStartElement())
    {
        if (m_xml.name() == "Point")
        {
            seenPoint = true;
            if (const auto point = parsePointElement())
            {
                min.x = std::min(min.x, point->x);
                min.y = std::min(min.y, point->y);

                max.x = std::max(max.x, point->x);
                max.y = std::max(max.y, point->y);
            }
        }
        else
        {
            m_xml.skipCurrentElement();
        }
    }
    if (m_xml.hasError())
        return std::nullopt;

    if (!seenPoint)
        NX_DEBUG(NX_SCOPE_TAG, "No 'Point' elements in 'Region' element");
    if (min.x > max.x || min.y > max.y)
        return std::nullopt;

    return MinMaxRect{min, max};
}

std::optional<Rect> MetadataParser::parseRegionListElement()
{
    Point min = {1, 1};
    Point max = {0, 0};

    bool seenRegion = false;

    while (m_xml.readNextStartElement())
    {
        if (m_xml.name() == "Region")
        {
            seenRegion = true;
            if (const auto rect = parseRegionElement())
            {
                min.x = std::min(min.x, rect->min.x);
                min.y = std::min(min.y, rect->min.y);

                max.x = std::max(max.x, rect->max.x);
                max.y = std::max(max.y, rect->max.y);
            }
        }
        else
        {
            m_xml.skipCurrentElement();
        }
    }
    if (m_xml.hasError())
        return std::nullopt;

    if (!seenRegion)
        NX_DEBUG(NX_SCOPE_TAG, "No 'Region' elements in 'RegionList' element");
    if (min.x > max.x || min.y > max.y)
        return std::nullopt;

    Rect rect;
    rect.x = min.x;
    rect.y = min.y;
    rect.width = max.x - min.x;
    rect.height = max.y - min.y;
    return rect;
}

Ptr<Attribute> MetadataParser::parsePropertyElement()
{
    std::optional<QString> description;
    std::optional<QString> value;

    while (m_xml.readNextStartElement())
    {
        if (m_xml.name() == "description")
            description = parseStringElement();
        else if (m_xml.name() == "value")
            value = parseStringElement();
        else
            m_xml.skipCurrentElement();
    }
    if (m_xml.hasError())
        return nullptr;

    if (!description || !value)
        return nullptr;

    return makePtr<Attribute>(
        IAttribute::Type::string,
        description->toStdString(),
        value->toStdString());
}

std::vector<Ptr<Attribute>> MetadataParser::parsePropertyListElement()
{
    std::vector<Ptr<Attribute>> attributes;

    while (m_xml.readNextStartElement())
    {
        if (m_xml.name() == "Property")
        {
            if (auto attribute = parsePropertyElement())
                attributes.push_back(std::move(attribute));
        }
        else
        {
            m_xml.skipCurrentElement();
        }
    }

    return attributes;
}

Ptr<ObjectMetadata> MetadataParser::parseTargetElement()
{
    std::optional<Uuid> trackId;
    std::optional<std::string> typeId;
    std::optional<Rect> boundingBox;
    std::vector<Ptr<Attribute>> attributes;

    bool seenTargetId = false;
    bool seenRecognition = false;
    bool seenRegionList = false;
    bool seenPropertyList = false;

    while (m_xml.readNextStartElement())
    {
        if (m_xml.name() == "targetID" || m_xml.name() == "ruleID")
        {
            // Treat rule activations as objects.
            if (m_xml.name() == "ruleID" && seenTargetId)
                continue;

            seenTargetId = true;
            trackId = parseTargetIdElement();
        }
        else if (m_xml.name() == "recognition")
        {
            seenRecognition = true;
            typeId = parseRecognitionElement();
        }
        else if (m_xml.name() == "RegionList")
        {
            seenRegionList = true;
            boundingBox = parseRegionListElement();
        }
        else if (m_xml.name() == "PropertyList")
        {
            seenPropertyList = true;
            attributes = parsePropertyListElement();
        }
        else
        {
            m_xml.skipCurrentElement();
        }
    }
    if (m_xml.hasError())
        return nullptr;

    if (!seenTargetId)
        NX_DEBUG(NX_SCOPE_TAG, "No 'targetID' or 'ruleID' element in 'Target' element");
    if (!trackId)
        return nullptr;

    if (!seenRecognition)
        typeId = "nx.hikvision.event"; // Treat rule activations as objects.

    if (boundingBox)
    {
        auto& entry = m_cache[*trackId];
        entry.lastUpdate = std::chrono::steady_clock::now();
        entry.boundingBox = *boundingBox;
    }
    else if (const auto* entry = findInCache(*trackId); entry && entry->boundingBox)
    {
        boundingBox = *entry->boundingBox;
    }
    else if (!seenRegionList)
    {
        NX_DEBUG(NX_SCOPE_TAG, "No 'RegionList' element in 'Target' element");
    }
    if (!boundingBox)
        return nullptr;

    if (seenPropertyList)
    {
        auto& entry = m_cache[*trackId];
        entry.lastUpdate = std::chrono::steady_clock::now();
        entry.attributes = attributes;
    }
    else if (const auto* entry = findInCache(*trackId))
    {
        attributes = entry->attributes;
    }

    auto metadata = makePtr<ObjectMetadata>();

    metadata->setTrackId(*trackId);

    if (typeId)
        metadata->setTypeId(*typeId);

    metadata->setBoundingBox(*boundingBox);
    metadata->addAttributes(std::move(attributes));

    return metadata;
}

std::vector<Ptr<ObjectMetadata>> MetadataParser::parseTargetListElement()
{
    std::vector<Ptr<ObjectMetadata>> metadatas;

    bool seenTarget = false;

    while (m_xml.readNextStartElement())
    {
        if (m_xml.name() == "Target")
        {
            seenTarget = true;
            if (auto metadata = parseTargetElement())
                metadatas.push_back(std::move(metadata));
        }
        else
        {
            m_xml.skipCurrentElement();
        }
    }
    if (m_xml.hasError())
        return metadatas;

    if (!seenTarget)
        NX_DEBUG(NX_SCOPE_TAG, "No 'Target' elements in 'TargetList' element");

    return metadatas;
}

std::vector<Ptr<ObjectMetadata>> MetadataParser::parseTargetDetectionElement()
{
    std::vector<Ptr<ObjectMetadata>> metadatas;

    bool seenTargetList = false;

    while (m_xml.readNextStartElement())
    {
        if (m_xml.name() == "TargetList")
        {
            seenTargetList = true;
            metadatas = parseTargetListElement();
        }
        else
        {
            m_xml.skipCurrentElement();
        }
    }
    if (m_xml.hasError())
        return metadatas;

    if (!seenTargetList)
        NX_DEBUG(NX_SCOPE_TAG, "No 'TargetList' element in 'TargetDetection' element");

    return metadatas;
}

Ptr<ObjectMetadataPacket> MetadataParser::parseMetadataElement()
{
    bool seenType = false;
    bool seenTargetDetection = false;

    std::optional<QString> type;
    std::vector<Ptr<ObjectMetadata>> metadatas;

    while (m_xml.readNextStartElement())
    {
        if (m_xml.name() == "type")
        {
            seenType = true;
            type = parseStringElement();
        }
        else if (m_xml.name() == "TargetDetection")
        {
            seenTargetDetection = true;
            metadatas = parseTargetDetectionElement();
        }
        else
        {
            m_xml.skipCurrentElement();
        }
    }
    if (m_xml.hasError())
        return nullptr;

    if (!seenType)
        NX_DEBUG(NX_SCOPE_TAG, "No 'type' element in 'Metadata' element");
    if (!seenTargetDetection)
        NX_DEBUG(NX_SCOPE_TAG, "No 'TargetDetection' element in 'Metadata' element");

    if (!type || metadatas.empty())
        return nullptr;

    if (*type != "activityTarget")
        return nullptr;

    auto packet = makePtr<ObjectMetadataPacket>();

    packet->setDurationUs(
        std::chrono::duration_cast<std::chrono::microseconds>(kCacheEntryLifetime).count());

    for (auto metadata: metadatas)
        packet->addItem(metadata.get());

    return packet;
}

MetadataParser::CacheEntry* MetadataParser::findInCache(nx::sdk::Uuid trackId)
{
    const auto it = m_cache.find(trackId);
    if (it != m_cache.end())
        return nullptr;

    return &it->second;
}

void MetadataParser::evictStaleCacheEntries()
{
    const auto now = std::chrono::steady_clock::now();
    for (auto it = m_cache.begin(); it != m_cache.end(); )
    {
        auto& entry = it->second;
        if (now - entry.lastUpdate > kCacheEntryLifetime)
            it = m_cache.erase(it);
        else
            ++it;
    }
}

} // namespace nx::vms_server_plugins::analytics::hikvision
