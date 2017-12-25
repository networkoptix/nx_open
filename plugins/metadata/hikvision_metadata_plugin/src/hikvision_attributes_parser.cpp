#include "hikvision_attributes_parser.h"

#include <nx/utils/literal.h>
#include <array>
#include "hikvision_string_helper.h"

namespace nx {
namespace mediaserver {
namespace plugins {
namespace hikvision {

namespace {

static const QString kChannelParameter = lit("Channel.#.EventType");
static const QString kAlarmInputParameter = lit("AlarmInput");
static const QString kAlarmOutputParameter = lit("AlarmOutput");
static const QString kSystemEventParameter = lit("SystemEvent");

QString parseInternalName(const QString& rawInternalName)
{
    static const std::array<QString, 2> kIgnoredPostfixes = { "TriggerCap", "Cap" };
    for (const auto& postfix : kIgnoredPostfixes)
    {
        if (rawInternalName.endsWith(postfix))
            return rawInternalName.left(rawInternalName.length() - postfix.length());
        return QString();
    }
}

} // namespace

std::vector<QString> AttributesParser::parseSupportedEventsXml(
    const QByteArray& content,
    bool* outSuccess)
{
    std::vector<QString> result;
    QXmlStreamReader reader(content);
    if (outSuccess)
        *outSuccess = false;

    if (!reader.readNextStartElement())
        return result; //< Read root element
    while (reader.readNextStartElement())
    {
        auto eventTypeName = parseInternalName(reader.name().toString());
        if (!eventTypeName.isEmpty())
            result.push_back(eventTypeName);
        reader.skipCurrentElement();
    }

    if (outSuccess)
        *outSuccess = true;
    return result;
}

HikvisionEvent AttributesParser::parseEventXml(
    const QByteArray& content,
    const Hikvision::DriverManifest manifest,
    bool* outSuccess)
{
    QString description;
    HikvisionEvent result;
    QXmlStreamReader reader(content);
    if (outSuccess)
        *outSuccess = false;

    if (!reader.readNextStartElement())
        return result; //< Read root element
    while (reader.readNextStartElement())
    {
        auto name = reader.name().toString().toLower().trimmed();
        if (name == "channelid")
        {
            result.channel = reader.readElementText().toInt();
        }
        else if (name == "datetime")
        {
            result.dateTime = QDateTime::fromString(reader.readElementText(), Qt::ISODate);
        }
        else if (name == "state")
        {
            result.isActive = reader.readElementText() == "active";
        }
        else if (name == "eventtype")
        {
            const auto internalName = parseInternalName(reader.readElementText());
            result.typeId = manifest.eventTypeByInternalName(internalName);
        }
        else if (name == "eventdescription")
        {
            description = parseInternalName(reader.readElementText());
        }
        else
        {
            reader.skipCurrentElement();
        }
    }

    result.caption = HikvisionStringHelper::buildCaption(
        manifest,
        result.typeId,
        result.channel,
        result.region,
        result.isActive);

    result.description = HikvisionStringHelper::buildDescription(
        manifest,
        result.typeId,
        result.channel,
        result.region,
        result.isActive);


    if (outSuccess)
        *outSuccess = true;
    return result;
}

} // namespace hikvision
} // namespace plugins
} // namespace mediaserver
} // namespace nx
