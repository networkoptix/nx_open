#include "metadata_parser.h"

#include <utility>
#include <string_view>

#include <nx/utils/log/log_main.h>

#include <QtCore/QDateTime>

namespace nx::vms_server_plugins::analytics::hikvision {

using namespace std::literals;
using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace {

constexpr double kCoordinateDomain = 1000;
constexpr auto kBoundingBoxLifetime = 1s;
constexpr auto kCacheEntryInactivityLifetime = 3s;
constexpr auto kCacheEntryMaximumLifetime = 30s;

} // namespace

const QString kThermalObjectTypeId = "nx.hikvision.ThermalObject";
const QString kThermalObjectPreAlarmTypeId = "nx.hikvision.ThermalObjectPreAlarm";
const QString kThermalObjectAlarmTypeId = "nx.hikvision.ThermalObjectAlarm";

void MetadataParser::setHandler(Ptr<IDeviceAgent::IHandler> handler)
{
    m_handler = handler;
}

void MetadataParser::parsePacket(QByteArray bytes, std::int64_t timestampUs)
{
    evictStaleCacheEntries();

    // Strip garbage off both ends. Its presence is possibly a server bug.
    // TODO: Remove if fixed.
    int begin = std::max(0, bytes.indexOf('<'));
    int end = bytes.lastIndexOf('>') + 1;
    bytes = bytes.mid(begin, std::max(0, end - begin));

    m_xml.clear();
    m_xml.addData(bytes);

    if (timestampUs > m_currentTimestampUs)
        m_currentTimestampUs = timestampUs;

    while (m_xml.readNextStartElement())
    {
        if (m_xml.name() == "Metadata")
        {
            m_lastMetadataTimestampUs = m_currentTimestampUs;
            m_sinceLastMetadata.restart();

            parseMetadataElement();
        }
        else
        {
            m_xml.skipCurrentElement();
        }
    }
    if (m_xml.hasError())
    {
        NX_DEBUG(NX_SCOPE_TAG, "Failed to parse metadata XML at %1:%2: %3 [[[%4]]]",
            m_xml.lineNumber(), m_xml.columnNumber(), m_xml.errorString(), bytes);
    }
}

void MetadataParser::processEvent(HikvisionEvent* event)
{
    evictStaleCacheEntries();

    if (!event->region)
        return;

    auto& entry = m_cache[-event->region->id];

    if (event->typeId.startsWith("nx.hikvision.Alarm1Thermal"))
        entry.thermalPreAlarmShouldBeActive = event->isActive;
    else if (event->typeId.startsWith("nx.hikvision.Alarm2Thermal"))
        entry.thermalAlarmShouldBeActive = event->isActive;
    else
        return;

    if (entry.trackId.isNull())
        return;

    const auto timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::microseconds(m_lastMetadataTimestampUs)
        + m_sinceLastMetadata.elapsed()).count();

    if (timestampUs > m_currentTimestampUs)
        m_currentTimestampUs = timestampUs;

    event->dateTime = QDateTime::fromMSecsSinceEpoch(m_currentTimestampUs / 1000);
    
    processEntry(&entry, event);
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

std::optional<QString> MetadataParser::parseRecognitionElement()
{
    const auto value = parseStringElement();
    if (!value)
        return std::nullopt;

    return NX_FMT("nx.hikvision.%1", *value);
}

std::optional<float> MetadataParser::parseCoordinateElement()
{
    const auto value = parseIntElement();
    if (!value)
        return std::nullopt;

    return *value / kCoordinateDomain;
}

std::optional<nx::sdk::analytics::Point> MetadataParser::parsePointElement()
{
    std::optional<float> x;
    std::optional<float> y;

    while (m_xml.readNextStartElement())
    {
        if (m_xml.name() == "x")
            x = parseCoordinateElement();
        else if (m_xml.name() == "y")
            y = parseCoordinateElement();
        else
            m_xml.skipCurrentElement();
    }
    if (m_xml.hasError())
        return std::nullopt;

    if (!x || !y)
        return std::nullopt;

    return nx::sdk::analytics::Point{*x, *y};
}

std::optional<MetadataParser::MinMaxRect> MetadataParser::parseRegionElement()
{
    nx::sdk::analytics::Point min = {1, 1};
    nx::sdk::analytics::Point max = {0, 0};

    while (m_xml.readNextStartElement())
    {
        if (m_xml.name() == "Point")
        {
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

    if (min.x > max.x || min.y > max.y)
        return std::nullopt;

    return MinMaxRect{min, max};
}

std::optional<Rect> MetadataParser::parseRegionListElement()
{
    nx::sdk::analytics::Point min = {1, 1};
    nx::sdk::analytics::Point max = {0, 0};

    while (m_xml.readNextStartElement())
    {
        if (m_xml.name() == "Region")
        {
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

void MetadataParser::parseTargetElement()
{
    std::optional<int> trackId;
    std::optional<QString> typeId;
    std::optional<Rect> boundingBox;
    std::vector<Ptr<Attribute>> attributes;

    while (m_xml.readNextStartElement())
    {
        if (m_xml.name() == "targetID")
        {
            trackId = parseTargetIdElement();
            if (!trackId)
                return;
        }
        else if (m_xml.name() == "recognition")
        {
            typeId = parseRecognitionElement();
            if (!typeId)
                return;
        }
        else if (m_xml.name() == "ruleID")
        {
            // Treat all rule activity as thermal objects.
            if (!trackId)
            {
                trackId = parseTargetIdElement();
                if (!trackId)
                    return;

                *trackId = -*trackId;
                typeId = kThermalObjectTypeId;
            }
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
        return;

    if (!trackId || !typeId)
        return;

    auto& entry = m_cache[*trackId];
    if (entry.originalTrackId.isNull())
    {
        entry.originalTrackId = nx::sdk::UuidHelper::randomUuid();
        entry.trackId = entry.originalTrackId;
        entry.typeId = *typeId;

        entry.lastUpdate.restart();
        entry.lifetime.restart();
    }

    if (!entry.typeId.startsWith(kThermalObjectTypeId))
    {
        for (auto it = attributes.begin(); it != attributes.end(); ++it)
        {
            if ((*it)->name() == "triggerEvent"sv && (*it)->value() == "true"sv)
            {
                attributes.erase(it);

                entry.regularBestShotShouldBeGenerated = true;

                break;
            }
        }
    }

    if (boundingBox || !attributes.empty())
        entry.lastUpdate.restart();

    // Thermal updates for non-point rules sometimes contain points with min/max
    // temperature. Don't collapse bounding box when that happens.
    if (boundingBox && (
        !entry.typeId.startsWith(kThermalObjectTypeId)
        || !entry.boundingBox
        || (boundingBox->width > entry.boundingBox->width
            && boundingBox->height > entry.boundingBox->height)))
    {
        entry.boundingBox = *boundingBox;
    }

    if (!attributes.empty())
        entry.attributes = attributes;

    processEntry(&entry);
}

void MetadataParser::parseTargetListElement()
{
    while (m_xml.readNextStartElement())
    {
        if (m_xml.name() == "Target")
            parseTargetElement();
        else
            m_xml.skipCurrentElement();
    }
}

void MetadataParser::parseTargetDetectionElement()
{
    while (m_xml.readNextStartElement())
    {
        if (m_xml.name() == "TargetList")
            parseTargetListElement();
        else
            m_xml.skipCurrentElement();
    }
}

void MetadataParser::parseMetadataElement()
{
    while (m_xml.readNextStartElement())
    {
        if (m_xml.name() == "type")
        {
            if (auto type = parseStringElement(); type != "activityTarget")
                return;
        }
        else if (m_xml.name() == "TargetDetection")
        {
            parseTargetDetectionElement();
        }
        else
        {
            m_xml.skipCurrentElement();
        }
    }
}

Ptr<ObjectMetadataPacket> MetadataParser::makeMetadataPacket(const CacheEntry& entry)
{
    auto packet = makePtr<ObjectMetadataPacket>();
    packet->setTimestampUs(m_currentTimestampUs);
    packet->setDurationUs(
        std::chrono::duration_cast<std::chrono::microseconds>(
            kBoundingBoxLifetime).count());

    const auto metadata = makePtr<ObjectMetadata>();
    metadata->setTrackId(entry.trackId);
    metadata->setTypeId(entry.typeId.toStdString());
    metadata->setBoundingBox(*entry.boundingBox);
    metadata->addAttributes(entry.attributes);
    packet->addItem(metadata.get());

    return packet;
}

void MetadataParser::processEntry(CacheEntry* entry, HikvisionEvent* event)
{
    if (!entry->boundingBox)
        return;

    const auto changeTrackId = 
        [&]()
        {
            const auto closingMetadataPacket = makeMetadataPacket(*entry);
            closingMetadataPacket->setDurationUs(1);
            m_handler->handleMetadata(closingMetadataPacket.get());

            entry->trackId = nx::sdk::UuidHelper::randomUuid();
            entry->lifetime.restart();
        };

    bool shouldGenerateBestShot = false;

    if (entry->regularBestShotShouldBeGenerated != entry->regularBestShotIsGenerated)
    {
        entry->regularBestShotIsGenerated = entry->regularBestShotShouldBeGenerated;
        shouldGenerateBestShot = entry->regularBestShotShouldBeGenerated;
    }
    else if (entry->thermalPreAlarmIsActive != entry->thermalPreAlarmShouldBeActive)
    {
        changeTrackId();
        if (entry->thermalPreAlarmShouldBeActive)
            entry->typeId = kThermalObjectPreAlarmTypeId;
        else if (entry->thermalAlarmIsActive)
            entry->typeId = kThermalObjectAlarmTypeId;
        else
            entry->typeId = kThermalObjectTypeId;

        entry->thermalPreAlarmIsActive = entry->thermalPreAlarmShouldBeActive;
        shouldGenerateBestShot = entry->thermalPreAlarmShouldBeActive;
    }
    else if (entry->thermalAlarmIsActive != entry->thermalAlarmShouldBeActive)
    {
        changeTrackId();
        if (entry->thermalAlarmShouldBeActive)
            entry->typeId = kThermalObjectAlarmTypeId;
        else if (entry->thermalPreAlarmIsActive)
            entry->typeId = kThermalObjectPreAlarmTypeId;
        else
            entry->typeId = kThermalObjectTypeId;

        entry->thermalAlarmIsActive = entry->thermalAlarmShouldBeActive;
        shouldGenerateBestShot = entry->thermalAlarmShouldBeActive;
    }

    if (entry->lifetime.hasExpired(kCacheEntryMaximumLifetime))
        changeTrackId();

    const auto metadataPacket = makeMetadataPacket(*entry);
    m_handler->handleMetadata(metadataPacket.get());
    
    if (shouldGenerateBestShot)
    {
        const auto bestShotPacket = makePtr<ObjectTrackBestShotPacket>();
        bestShotPacket->setTimestampUs(m_currentTimestampUs);
        bestShotPacket->setTrackId(entry->trackId);
        bestShotPacket->setBoundingBox(*entry->boundingBox);
        m_handler->handleMetadata(bestShotPacket.get());
    }
    
    if (event)
        event->trackId = entry->originalTrackId;
}

void MetadataParser::evictStaleCacheEntries()
{
    for (auto it = m_cache.begin(); it != m_cache.end();)
    {
        auto& entry = it->second;
        if (entry.lastUpdate.hasExpired(kCacheEntryInactivityLifetime))
            it = m_cache.erase(it);
        else
            ++it;
    }
}

} // namespace nx::vms_server_plugins::analytics::hikvision
