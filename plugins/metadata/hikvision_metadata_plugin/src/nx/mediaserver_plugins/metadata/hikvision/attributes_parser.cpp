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
    const Hikvision::DriverManifest manifest)
{
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

    result.caption = buildCaption(manifest, result);
    result.description = buildDescription(manifest, result);

    if (reader.error() != QXmlStreamReader::NoError)
        return boost::optional<HikvisionEvent>();
    return result;
}

} // namespace hikvision
} // namespace plugins
} // namespace mediaserver
} // namespace nx
