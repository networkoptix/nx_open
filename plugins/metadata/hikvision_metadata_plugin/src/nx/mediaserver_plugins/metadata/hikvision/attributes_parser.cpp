#include "attributes_parser.h"

#include <nx/utils/literal.h>
#include <array>
#include "string_helper.h"

namespace nx {
namespace mediaserver {
namespace plugins {
namespace hikvision {

namespace {

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
        auto eventTypeName = normalizeInternalName(reader.name().toString());
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
            auto internalName = normalizeInternalName(reader.readElementText());
            result.typeId = manifest.eventTypeByInternalName(internalName);
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

    result.caption = HikvisionStringHelper::buildCaption(manifest, result);
    result.description = HikvisionStringHelper::buildDescription(manifest, result);

    if (outSuccess)
        *outSuccess = true;
    return result;
}

} // namespace hikvision
} // namespace plugins
} // namespace mediaserver
} // namespace nx
