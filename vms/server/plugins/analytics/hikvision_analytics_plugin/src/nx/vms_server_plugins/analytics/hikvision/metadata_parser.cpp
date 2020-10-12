#include "metadata_parser.h"

#include <utility>
#include <string_view>

#include <nx/utils/log/log_main.h>

namespace nx::vms_server_plugins::analytics::hikvision {

using namespace std::literals;
using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace {

constexpr double kCoordinateDomain = 1000;
constexpr auto kCacheEntryInactivityLifetime = 3s;
constexpr auto kCacheEntryMaximumLifetime = 30s;

const QString kThermalObjectTypeId = "nx.hikvision.ThermalObject";
const QString kThermalObjectPreAlarmTypeId = "nx.hikvision.ThermalObjectPreAlarm";
const QString kThermalObjectAlarmTypeId = "nx.hikvision.ThermalObjectAlarm";

} // namespace

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

    m_timestampUs = timestampUs;

    while (m_xml.readNextStartElement())
    {
        if (m_xml.name() == "Metadata")
            parseMetadataElement();
        else
            m_xml.skipCurrentElement();
    }
    if (m_xml.hasError())
    {
        NX_DEBUG(NX_SCOPE_TAG, "Failed to parse metadata XML at %1:%2: %3 [[[%4]]]",
            m_xml.lineNumber(), m_xml.columnNumber(), m_xml.errorString(), bytes);
    }
}

void MetadataParser::processEvent(HikvisionEvent* event, std::int64_t timestampUs)
{
    evictStaleCacheEntries();

    m_timestampUs = timestampUs;

    if (!event->region)
        return;

    auto& entry = m_cache[-event->region->id];
    if (entry.state == State::initial)
        return;

    if (event->typeId.startsWith("nx.hikvision.Alarm1Thermal"))
    {
        if (event->isActive)
        {
            if (entry.state == State::thermalInitial)
                entry.state = State::thermalShouldStartPreAlarm;
        }
        else
        {
            if (entry.state == State::thermalAlarmActive)
            {
                entry.state = State::thermalShouldStopAlarm;
                processEntry(&entry, event);
            }

            if (entry.state == State::thermalPreAlarmActive)
                entry.state = State::thermalShouldStopPreAlarm;
        }
    }
    else if (event->typeId.startsWith("nx.hikvision.Alarm2Thermal"))
    {
        if (event->isActive)
        {
            if (entry.state == State::thermalInitial)
            {
                entry.state = State::thermalShouldStartPreAlarm;
                processEntry(&entry, event);
            }

            if (entry.state == State::thermalPreAlarmActive)
                entry.state = State::thermalShouldStartAlarm;
        }
        else
        {
            if (entry.state == State::thermalAlarmActive)
                entry.state = State::thermalShouldStopAlarm;
        }
    }
    
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

    if (entry.state == State::initial)
    {
        entry.typeId = *typeId;

        entry.lastUpdate.restart();
        entry.lifetime.restart();
    }

    if (entry.typeId.startsWith(kThermalObjectTypeId))
    {
        if (entry.state == State::initial)
            entry.state = State::thermalInitial;
    }
    else
    {
        if (entry.state == State::initial)
            entry.state = State::normalInitial;

        for (auto it = attributes.begin(); it != attributes.end(); ++it)
        {
            if ((*it)->name() == "triggerEvent"sv && (*it)->value() == "true"sv)
            {
                attributes.erase(it);

                if (entry.state == State::normalInitial)
                    entry.state = State::normalShouldGenerateBestShot;

                break;
            }
        }
    }

    if (boundingBox || !attributes.empty())
        entry.lastUpdate.restart();

    // Thermal updates for non-point rules sometimes contain points with min/max
    // temperature. Don't collapse boundig box when that happens.
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
    packet->setTimestampUs(m_timestampUs);
    packet->setDurationUs(
        std::chrono::duration_cast<std::chrono::microseconds>(
            kCacheEntryInactivityLifetime).count());

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
            closingMetadataPacket->setDurationUs(0);
            m_handler->handleMetadata(closingMetadataPacket.get());

            entry->trackId = nx::sdk::UuidHelper::randomUuid();
            entry->lifetime.restart();
        };

    bool shouldGenerateBestShot = false;

    switch (entry->state)
    {
        case State::normalShouldGenerateBestShot:
        {
            shouldGenerateBestShot = true;
            entry->state = State::normalBestShotGenerated;
            break;
        }
        case State::thermalShouldStartPreAlarm:
        {
            changeTrackId();
            shouldGenerateBestShot = true;
            entry->typeId = kThermalObjectPreAlarmTypeId;
            entry->state = State::thermalPreAlarmActive;
            break;
        }
        case State::thermalShouldStopPreAlarm:
        {
            changeTrackId();
            entry->typeId = kThermalObjectTypeId;
            entry->state = State::thermalInitial;
            break;
        }
        case State::thermalShouldStartAlarm:
        {
            changeTrackId();
            shouldGenerateBestShot = true;
            entry->typeId = kThermalObjectAlarmTypeId;
            entry->state = State::thermalAlarmActive;
            break;
        }
        case State::thermalShouldStopAlarm:
        {
            changeTrackId();
            entry->typeId = kThermalObjectPreAlarmTypeId;
            entry->state = State::thermalPreAlarmActive;
            break;
        }
        default:
            break;
    }

    if (entry->lifetime.hasExpired(kCacheEntryMaximumLifetime))
        changeTrackId();

    const auto metadataPacket = makeMetadataPacket(*entry);
    m_handler->handleMetadata(metadataPacket.get());
    
    if (shouldGenerateBestShot)
    {
        const auto bestShotPacket = makePtr<ObjectTrackBestShotPacket>();
        bestShotPacket->setTimestampUs(m_timestampUs);
        bestShotPacket->setTrackId(entry->trackId);
        bestShotPacket->setBoundingBox(*entry->boundingBox);
        m_handler->handleMetadata(bestShotPacket.get());
    }
    
    if (event)
        event->trackId = entry->trackId;
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
