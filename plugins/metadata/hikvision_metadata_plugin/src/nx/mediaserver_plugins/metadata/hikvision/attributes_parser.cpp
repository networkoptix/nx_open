#include "attributes_parser.h"
#include "string_helper.h"

#include <array>

#include <nx/utils/literal.h>
#include <nx/utils/log/log_main.h>

namespace nx {
namespace mediaserver {
namespace plugins {
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

static QString normalizeInternalName(const QString& rawInternalName)
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

boost::optional<std::vector<QString>> AttributesParser::parseSupportedEventsXml(
    const QByteArray& content)
{
    std::vector<QString> result;
    QXmlStreamReader reader(content);

    if (!reader.readNextStartElement())
        return boost::optional<std::vector<QString>>(); //< Read root element.
    while (reader.readNextStartElement())
    {
        auto eventTypeName = normalizeInternalName(reader.name().toString());
        if (!eventTypeName.isEmpty())
            result.push_back(eventTypeName);
        reader.skipCurrentElement();
    }

    if (reader.error() != QXmlStreamReader::NoError)
        return boost::optional<std::vector<QString>>();
    return result;
}

boost::optional<HikvisionEvent> AttributesParser::parseEventXml(
    const QByteArray& content,
    const Hikvision::DriverManifest& manifest)
{
    using namespace nx::sdk::metadata;


    QString description;
    HikvisionEvent result;
    QXmlStreamReader reader(content);

    if (!reader.readNextStartElement())
        return boost::optional<HikvisionEvent>(); //< Read root element.
    while (reader.readNextStartElement())
    {
        const auto name = reader.name().toString().toLower().trimmed();
        if (name == "channelid")
        {
            result.channel = reader.readElementText().toInt() - 1; //< Convert to range [0..x].
        }
        else if (name == "datetime")
        {
            result.dateTime = QDateTime::fromString(reader.readElementText(), Qt::ISODate);
        }
        else if (name == "eventstate")
        {
            result.isActive = reader.readElementText() == "active";
        }
        else if (name == "eventtype")
        {
            const auto internalName = normalizeInternalName(reader.readElementText());
            result.typeId = manifest.eventTypeByInternalName(internalName);
            if (result.typeId.isNull())
                NX_WARNING(typeid(AttributesParser), lm("Unknown analytics event name %1").arg(internalName));
        }
        else if (name == "eventdescription")
        {
            description = reader.readElementText();
        }
        else if (name == "detectionregionlist")
        {
            const auto regionList = reader.readElementText().trimmed();
            // TODO: Region number is empty. I haven't seen cameras with non empty region list so far.
        }
        else
        {
            reader.skipCurrentElement();
        }
    }

    auto eventDescriptor = manifest.eventDescriptorById(result.typeId);
    result.caption = buildCaption(manifest, result);
    result.description = buildDescription(manifest, result);

    if (reader.error() != QXmlStreamReader::NoError)
        return boost::optional<HikvisionEvent>();
    return result;
}

std::vector<HikvisionEvent> AttributesParser::parseLprXml(
    const QByteArray& content,
    const Hikvision::DriverManifest& manifest)
{
    std::vector<HikvisionEvent> result;
    QXmlStreamReader reader(content);

    while (!reader.atEnd() && reader.readNextStartElement())
    {
        if (reader.name().toString().toLower() == "plate")
        {
            auto hikvisionEvent = parsePlateData(reader, manifest);
            if (!hikvisionEvent.typeId.isNull())
            {
                result.push_back(hikvisionEvent);
                HikvisionEvent inactiveEvent(hikvisionEvent);
                inactiveEvent.isActive = false;
                result.push_back(inactiveEvent);
            }
        }
    }
    return result;
}

HikvisionEvent AttributesParser::parsePlateData(
    QXmlStreamReader& reader,
    const Hikvision::DriverManifest& manifest)
{
    QString plateNumber;
    QString country;
    QString eventType;
    int laneNumber = 0;
    QString direction;
    QnUuid typeId;
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
            typeId = manifest.eventTypeByInternalName(internalName);
            if (typeId.isNull())
                NX_WARNING(typeid(AttributesParser), lm("Unknown analytics event name %1").arg(internalName));
        }
        else
        {
            reader.skipCurrentElement();
        }
    }

    using namespace nx::sdk::metadata;
    HikvisionEvent hikvisionEvent;
    if (!typeId.isNull())
    {
        hikvisionEvent.caption = lm("Plate No. %1 (%2)").args(plateNumber, country);
        hikvisionEvent.description = lm("%1, Lane No. %2, Direction: %3")
            .args(hikvisionEvent.caption, laneNumber, direction);
        hikvisionEvent.typeId = typeId;
        hikvisionEvent.dateTime = dateTime;
        hikvisionEvent.picName = picName;
        hikvisionEvent.isActive = true;

    }
    return hikvisionEvent;
}

} // namespace hikvision
} // namespace plugins
} // namespace mediaserver
} // namespace nx
