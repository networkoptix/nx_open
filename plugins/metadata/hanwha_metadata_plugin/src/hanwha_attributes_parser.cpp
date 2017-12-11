#if defined(ENABLED_HANWHA)

#include "hanwha_attributes_parser.h"

#include <iostream>

#include <nx/utils/literal.h>

namespace nx {
namespace mediaserver {
namespace plugins {

namespace {

static const QString kChannelParameter = lit("Channel.#.EventType");
static const QString kAlarmInputParameter = lit("AlarmInput");
static const QString kAlarmOutputParameter = lit("AlarmOutput");
static const QString kSystemEventParameter = lit("SystemEvent");

} // namespace

bool HanwhaAttributesParser::parseCgi(const QString& content)
{
    QXmlStreamReader reader(content);
    
    if (!readCgi(reader))
        return false;

    if (!readSubMenu(reader))
        return false;

    if (!readMonitorAction(reader))
        return false;
    
    if (!readChannelEvents(reader))
        return false;

    return true;
}

std::vector<QString> HanwhaAttributesParser::supportedEvents() const
{
    return m_supportedEvents;
}

bool HanwhaAttributesParser::readCgi(QXmlStreamReader& reader)
{
    if (!reader.readNextStartElement())
        return false;

    if (reader.name() == "cgis" && !reader.readNextStartElement())
        return false;

    if (reader.name() != "cgi")
        return false;
    
    while (reader.readNextStartElement())
    {
        if (reader.name() == "submenu" && (reader.attributes().value("name") == "eventstatus"))
            return true;
        else
            reader.skipCurrentElement();
    }

    return false;
}

bool HanwhaAttributesParser::readSubMenu(QXmlStreamReader& reader)
{
    while (reader.readNextStartElement())
    {
        if (reader.name() == "action" && reader.attributes().value("name") == "monitor")
            return true;
        else
            reader.skipCurrentElement();
    }

    return false;
}

bool HanwhaAttributesParser::readMonitorAction(QXmlStreamReader& reader)
{
    while (reader.readNextStartElement())
    {
        if (reader.name() == "parameter" && reader.attributes().value("name") == "Channel.#.EventType")
            return true;
        else
            reader.skipCurrentElement();
    }

    return false;
}

bool HanwhaAttributesParser::readChannelEvents(QXmlStreamReader& reader)
{
    bool entryNodesHaveBeenReached = false;
    while (reader.readNextStartElement())
    {
        if (reader.name() != "entry" && entryNodesHaveBeenReached)
            return true;

        if (reader.name() == "entry")
        {
            entryNodesHaveBeenReached = true;
            m_supportedEvents.push_back(reader.attributes().value("value").toString());
            reader.skipCurrentElement();
        }
    }

    return entryNodesHaveBeenReached;
}

} // namespace plugins
} // namespace mediaserver
} // namespace nx

#endif // defined(ENABLED_HANWHA)
