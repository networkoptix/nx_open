#include "attributes_parser.h"

#include <array>

#include <nx/utils/log/log_main.h>

#include "string_helper.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace hikvision {

namespace {

QDateTime parseDateTime(QString stringDate)
{
    static const int kTimeOffsetLength = 5;

    int timeOffset = stringDate.right(kTimeOffsetLength).toInt();
    int timeOffsetMinutes = (timeOffset / 100) * 60 + timeOffset % 100;
    stringDate = stringDate.left(stringDate.size() - kTimeOffsetLength);
    auto dateTime = QDateTime::fromString(stringDate, "yyyyMMddThhmmss");
    dateTime.setOffsetFromUtc(timeOffsetMinutes * 60);
    return dateTime;
}

QString normalizeInternalName(const QString& rawInternalName)
{
    static const std::array<QString, 2> kIgnoredPostfixes = { "TriggerCap", "Cap" };
    for (const auto& postfix : kIgnoredPostfixes)
    {
        if (rawInternalName.endsWith(postfix))
            return rawInternalName.left(rawInternalName.length() - postfix.length());
    }
    return rawInternalName;
}

} // namespace

/*static*/ std::optional<std::vector<QString>> AttributesParser::parseSupportedEventsXml(
    const QByteArray& content)
{
    std::vector<QString> result;
    QXmlStreamReader reader(content);

    if (!reader.readNextStartElement())
        return std::optional<std::vector<QString>>(); //< Read root element.
    while (reader.readNextStartElement())
    {
        auto eventTypeName = normalizeInternalName(reader.name().toString());
        if (!eventTypeName.isEmpty())
            result.push_back(eventTypeName);
        reader.skipCurrentElement();
    }

    if (reader.error() != QXmlStreamReader::NoError)
        return std::optional<std::vector<QString>>();
    return result;
}

/*static*/ std::optional<EventWithRegions> AttributesParser::parseEventXml(
    const QByteArray& content,
    const Hikvision::EngineManifest& manifest)
{
    QString description;
    EventWithRegions result;
    QXmlStreamReader reader(content);

    if (!reader.readNextStartElement())
        return {};
    while (reader.readNextStartElement())
    {
        const auto name = reader.name().toString().toLower().trimmed();
        if (name == "channelid")
        {
            result.event.channel = reader.readElementText().toInt() - 1; //< Convert to range [0..x].
        }
        else if (name == "datetime")
        {
            result.event.dateTime = QDateTime::fromString(reader.readElementText(), Qt::ISODate);
        }
        else if (name == "eventstate")
        {
            result.event.isActive = reader.readElementText() == "active";
        }
        else if (name == "eventtype")
        {
            const auto internalName = normalizeInternalName(reader.readElementText());
            result.event.typeId = manifest.eventTypeIdByInternalName(internalName);
            if (result.event.typeId.isEmpty())
            {
                NX_WARNING(typeid(AttributesParser),
                    lm("Unknown analytics event name %1").arg(internalName));
            }
        }
        else if (name == "eventdescription")
        {
            description = reader.readElementText();
        }
        else if (name == "detectionregionlist")
        {
            result.regions = parseRegionList(reader);
        }
        else
        {
            reader.skipCurrentElement();
        }
    }

    if (reader.error() != QXmlStreamReader::NoError)
    {
        NX_VERBOSE(typeid(AttributesParser), lm("XML parse error: %1").arg(reader.errorString()));
        return {};
    }
    return result;
}

/*static*/ std::vector<Region> AttributesParser::parseRegionList(QXmlStreamReader& reader)
{
    std::vector<Region> result;
    constexpr int kExpectedRegionCount = 3;
    result.reserve(kExpectedRegionCount);
    while (reader.readNextStartElement())
    {
        const auto name = reader.name().toString().toLower().trimmed();
        if (name == "detectionregionentry")
        {
            result.push_back(parseRegionEntry(reader));
        }
        else
        {
            reader.skipCurrentElement();
        }
    }
    return result;
}

/*static*/ Region AttributesParser::parseRegionEntry(QXmlStreamReader& reader)
{
    Region result;
    while (reader.readNextStartElement())
    {
        const auto name = reader.name().toString().toLower().trimmed();
        if (name == "regionid")
            result.id = reader.readElementText().toInt(); //< id = 0 if text is not an integer
        else if (name == "regioncoordinateslist")
            result.points = parseCoordinatesList(reader);
        else
            reader.skipCurrentElement();
    }
    return result;
}

/*static*/ std::vector<Point> AttributesParser::parseCoordinatesList(QXmlStreamReader& reader)
{
    std::vector<Point> result;
    constexpr int kMaxPointCount = 10;
    result.reserve(kMaxPointCount);
    while (reader.readNextStartElement())
    {
        const auto name = reader.name().toString().toLower().trimmed();
        if (name == "regioncoordinates")
        {
            Point point;
            while (reader.readNextStartElement())
            {
                const auto subname = reader.name().toString().toLower().trimmed();
                if (subname == "positionx")
                    point.x = reader.readElementText().toInt(); //< x = 0 if text is not an integer
                else if (subname == "positiony")
                    point.y = reader.readElementText().toInt(); //< y = 0 if text is not an integer
                else
                    reader.skipCurrentElement();
            }
            result.push_back(point);
        }
        else
        {
            reader.skipCurrentElement();
        }
    }
    return result;
}

/*static*/ std::vector<HikvisionEvent> AttributesParser::parseLprXml(
    const QByteArray& content,
    const Hikvision::EngineManifest& manifest)
{
    using namespace nx::vms::api::analytics;

    std::vector<HikvisionEvent> result;
    QXmlStreamReader reader(content);

    auto addEvent =
        [&](HikvisionEvent hikvisionEvent)
        {
            hikvisionEvent.isActive = true;
            result.push_back(hikvisionEvent);
        };

    while (!reader.atEnd() && reader.readNextStartElement())
    {
        if (reader.name().toString().toLower() == "plate")
        {
            auto hikvisionEvent = parsePlateData(reader, manifest);
            if (hikvisionEvent.typeId.isEmpty())
                continue;

            addEvent(hikvisionEvent);
            const auto eventType = manifest.eventTypeById(hikvisionEvent.typeId);
        }
    }
    return result;
}

/*static*/ HikvisionEvent AttributesParser::parsePlateData(
    QXmlStreamReader& reader,
    const Hikvision::EngineManifest& manifest)
{
    QString plateNumber;
    QString country;
    QString eventType;
    int laneNumber = 0;
    QString direction;
    QString eventTypeId;
    QDateTime dateTime;
    QString picName;

    while (reader.readNextStartElement())
    {
        const auto name = reader.name().toString().toLower().trimmed();
        if (name == "capturetime")
        {
            dateTime = parseDateTime(reader.readElementText());
        }
        else if (name == "platenumber")
        {
            plateNumber = reader.readElementText();
        }
        else if (name == "country")
        {
            country = reader.readElementText();
        }
        else if (name == "laneno")
        {
            laneNumber = reader.readElementText().toInt();
        }
        else if (name == "direction")
        {
            direction = reader.readElementText();
        }
        else if (name == "picname")
        {
            picName = reader.readElementText();
        }
        else if (name == "matchingresult")
        {
            const auto internalName = normalizeInternalName(reader.readElementText());
            eventTypeId = manifest.eventTypeIdByInternalName(internalName);
            if (eventTypeId.isEmpty())
            {
                NX_WARNING(typeid(AttributesParser),
                    lm("Unknown analytics event name %1").arg(internalName));
            }
        }
        else
        {
            reader.skipCurrentElement();
        }
    }

    HikvisionEvent hikvisionEvent;
    if (!eventTypeId.isEmpty())
    {
        hikvisionEvent.caption = lm("Plate No. %1 (%2)").args(plateNumber, country);
        hikvisionEvent.description = lm("%1, Lane No. %2, Direction: %3")
            .args(hikvisionEvent.caption, laneNumber, direction);
        hikvisionEvent.typeId = eventTypeId;
        hikvisionEvent.dateTime = dateTime;
        hikvisionEvent.picName = picName;
        hikvisionEvent.isActive = true;

    }
    return hikvisionEvent;
}

} // namespace hikvision
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
