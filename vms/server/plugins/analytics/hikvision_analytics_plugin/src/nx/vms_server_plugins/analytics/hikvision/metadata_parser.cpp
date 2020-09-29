#include "metadata_parser.h"

#include <utility>
#include <string_view>

#include <nx/utils/log/log_main.h>
#include <nx/sdk/helpers/uuid_helper.h>

namespace nx::vms_server_plugins::analytics::hikvision {

using namespace std::literals;
using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace {

constexpr double kCoordinateDomain = 1000;
constexpr auto kCacheEntryInactivityLifetime = 3s;
constexpr auto kCacheEntryMaximumLifetime = 30s;

} // namespace

MetadataParser::Result MetadataParser::parsePacket(QByteArray bytes)
{
    evictStaleCacheEntries();

    // Strip garbage off both ends. Its presence is possibly a server bug.
    // TODO: Remove if fixed.
    int begin = std::max(0, bytes.indexOf('<'));
    int end = bytes.lastIndexOf('>') + 1;
    bytes = bytes.mid(begin, std::max(0, end - begin));

    m_xml.clear();
    m_xml.addData(bytes);

    Result result;

    if (m_xml.readNextStartElement())
    {
        if (m_xml.name() == "Metadata")
        {
            result = parseMetadataElement();
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

    return result;
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

std::optional<int> MetadataParser::parseTargetIdElement()
{
    const auto value = parseIntElement();
    if (!value)
        return std::nullopt;

    return value;
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

MetadataParser::TargetResult MetadataParser::parseTargetElement()
{
    std::optional<int> trackId;
    std::optional<std::string> typeId;
    std::optional<Rect> boundingBox;
    std::vector<Ptr<Attribute>> attributes;

    while (m_xml.readNextStartElement())
    {
        if (m_xml.name() == "targetID")
        {
            trackId = parseTargetIdElement();
        }
        else if (m_xml.name() == "ruleID")
        {
            // Treat rule activations as objects.
            if (!trackId)
            {
                if (trackId = parseTargetIdElement(); trackId != 0)
                    trackId = -(*trackId);
            }
        }
        else if (m_xml.name() == "recognition")
        {
            typeId = parseRecognitionElement();
            if (!typeId)
                return {};
        }
        else if (m_xml.name() == "RegionList")
        {
            boundingBox = parseRegionListElement();
        }
        else if (m_xml.name() == "PropertyList")
        {
            attributes = parsePropertyListElement();
        }
        else
        {
            m_xml.skipCurrentElement();
        }
    }
    if (m_xml.hasError())
        return {};

    if (!trackId)
    {
        NX_DEBUG(NX_SCOPE_TAG, "No 'targetID' or 'ruleID' element in 'Target' element");
        return {};
    }


    if (!typeId)
    {
        // Treat rule activations as objects.
        if (*trackId < 0)
            typeId = "nx.hikvision.event";
        else
            return {};
    }

    bool shouldGenerateBestShot = false;
    for (auto it = attributes.begin(); it != attributes.end(); ++it)
    {
        if ((*it)->name() == "triggerEvent"sv && (*it)->value() == "true"sv)
        {
            attributes.erase(it);
            shouldGenerateBestShot = true;
            break;
        }
    }

    auto& entry = m_cache[*trackId];
    if (boundingBox || !attributes.empty() || entry.trackId.isNull())
        entry.lastUpdate.restart();
    if (entry.trackId.isNull())
        entry.trackId = nx::sdk::UuidHelper::randomUuid();

    if (boundingBox)
        entry.boundingBox = *boundingBox;
    if (!attributes.empty())
        entry.attributes = attributes;


    if (!entry.boundingBox)
    {
        NX_DEBUG(NX_SCOPE_TAG, "No 'RegionList' element in 'Target' element");
        return {};
    }

    auto metadata = makePtr<ObjectMetadata>();
    metadata->setTrackId(entry.trackId);
    metadata->setTypeId(*typeId);
    metadata->setBoundingBox(*entry.boundingBox);
    metadata->addAttributes(entry.attributes);

    Ptr<ObjectTrackBestShotPacket> bestShotPacket;
    if (shouldGenerateBestShot && !entry.bestShotGenerated)
    {
        bestShotPacket = makePtr<ObjectTrackBestShotPacket>();

        bestShotPacket->setTrackId(entry.trackId);
        bestShotPacket->setBoundingBox(*entry.boundingBox);

        entry.bestShotGenerated = true;
    }

    return {std::move(metadata), std::move(bestShotPacket)};
}

MetadataParser::TargetListResult MetadataParser::parseTargetListElement()
{
    std::vector<Ptr<ObjectMetadata>> metadatas;
    std::vector<Ptr<ObjectTrackBestShotPacket>> bestShotPackets;

    bool seenTarget = false;

    while (m_xml.readNextStartElement())
    {
        if (m_xml.name() == "Target")
        {
            seenTarget = true;

            auto result = parseTargetElement();
            if (result.metadata)
                metadatas.push_back(std::move(result.metadata));
            if (result.bestShotPacket)
                bestShotPackets.push_back(std::move(result.bestShotPacket));
        }
        else
        {
            m_xml.skipCurrentElement();
        }
    }
    if (m_xml.hasError())
        return {};

    if (!seenTarget)
        NX_DEBUG(NX_SCOPE_TAG, "No 'Target' elements in 'TargetList' element");

    return {std::move(metadatas), std::move(bestShotPackets)};
}

MetadataParser::TargetListResult MetadataParser::parseTargetDetectionElement()
{
    TargetListResult result;

    bool seenTargetList = false;

    while (m_xml.readNextStartElement())
    {
        if (m_xml.name() == "TargetList")
        {
            seenTargetList = true;
            result = parseTargetListElement();
        }
        else
        {
            m_xml.skipCurrentElement();
        }
    }
    if (m_xml.hasError())
        return {};

    if (!seenTargetList)
        NX_DEBUG(NX_SCOPE_TAG, "No 'TargetList' element in 'TargetDetection' element");

    return result;
}

MetadataParser::Result MetadataParser::parseMetadataElement()
{
    bool seenType = false;
    bool seenTargetDetection = false;

    std::optional<QString> type;
    TargetListResult result;

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
            result = parseTargetDetectionElement();
        }
        else
        {
            m_xml.skipCurrentElement();
        }
    }
    if (m_xml.hasError())
        return {};

    if (!seenType)
        NX_DEBUG(NX_SCOPE_TAG, "No 'type' element in 'Metadata' element");
    if (!seenTargetDetection)
        NX_DEBUG(NX_SCOPE_TAG, "No 'TargetDetection' element in 'Metadata' element");

    if (!type)
        return {};

    if (*type != "activityTarget")
        return {};

    Ptr<ObjectMetadataPacket> metadataPacket;
    if (!result.metadatas.empty())
    {
        metadataPacket = makePtr<ObjectMetadataPacket>();

        metadataPacket->setDurationUs(
            std::chrono::duration_cast<std::chrono::microseconds>(
                kCacheEntryInactivityLifetime).count());

        for (auto metadata: result.metadatas)
            metadataPacket->addItem(metadata.get());
    }

    return {std::move(metadataPacket), std::move(result.bestShotPackets)};
}

void MetadataParser::evictStaleCacheEntries()
{
    for (auto it = m_cache.begin(); it != m_cache.end();)
    {
        auto& entry = it->second;
        if (entry.lastUpdate.hasExpired(kCacheEntryInactivityLifetime)
            || entry.lifetime.hasExpired(kCacheEntryMaximumLifetime))
        {
            it = m_cache.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

} // namespace nx::vms_server_plugins::analytics::hikvision
