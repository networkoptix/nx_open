#include "attributes_parser.h"
#include "string_helper.h"

#include <array>
#include <iostream>
#include <fstream>

#include <nx/utils/literal.h>
#include <nx/utils/log/log_main.h>

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace dahua {

namespace {

static const QString kParseErrorMessage("Dahua event message parse error: ");
static const QString kParseOkMessage("Dahua event message parsing: ");

static const QString kParseDetailsErrorMessage = kParseErrorMessage + "event details = \"%1\"";
static const QString kParseDetailsOkMessage = kParseOkMessage + "event details = \"%1\"";

static const QString kHeartbeat("Heartbeat");
static const DahuaEvent kHeartbeatEvent{ kHeartbeat };

}

std::vector<QString> AttributesParser::parseSupportedEventsMessage(const QByteArray& content)
{
    std::vector<QString> result;
    QString contentAsString(content);
    contentAsString.replace("\r\n", "\n");

    QStringList contentAsList = contentAsString.split('\n');
    for (QString& line : contentAsList)
    {
        QStringList lineAsList = line.split('=');
        if (lineAsList.size() == 2)
            result.push_back(lineAsList[1]);
    }
    return result;
}

std::optional<DahuaEvent> AttributesParser::parseEventMessage(const QByteArray& content,
    const Dahua::EngineManifest& manifest)
{
    using namespace nx::sdk::analytics;
    using std::nullopt;
    static std::ofstream* f = new std::ofstream("D:\\events.txt");
    static int t = 0;

    QString contentAsString(content);
    contentAsString.replace("\r\n", "\n");
    const QStringList Lines = contentAsString.split('\n');

    NX_VERBOSE(typeid(AttributesParser),
        kParseOkMessage + "content string = " + contentAsString);

    if (Lines.isEmpty())
    {
        NX_DEBUG(typeid(AttributesParser),
            kParseErrorMessage + "content string has no data");
        return nullopt;
    }

    contentAsString = Lines[0];
    // contentAsString should contain something like "Code=NewFile;action=Pulse;index=0;data={"

    const QStringList eventDetails = contentAsString.split(";");
    if (eventDetails.isEmpty())
    {
        NX_DEBUG(typeid(AttributesParser), kParseDetailsErrorMessage, contentAsString);
        return nullopt;
    }

    if (eventDetails[0] == kHeartbeat)
    {
        NX_VERBOSE(typeid(AttributesParser), kParseOkMessage + "heartbeat message received");
        *f << ++t << " " << eventDetails[0].toStdString().c_str() << std::endl;
        return kHeartbeatEvent;
    }

    if (eventDetails.size() < 2)
    {
        // At least two pieces of event information needed: name and action (start/stop/pulse)
        NX_DEBUG(typeid(AttributesParser), kParseDetailsErrorMessage, contentAsString);
        return nullopt;
    }

    const QStringList code = eventDetails[0].split("=");
    const QStringList action = eventDetails[1].split("=");

    if (code.size() != 2 || code[0] != "Code" || action.size() != 2 || action[0] != "action")
    {
        NX_DEBUG(typeid(AttributesParser), kParseDetailsErrorMessage, contentAsString);
        return nullopt;
    }

    NX_VERBOSE(typeid(AttributesParser),
        kParseOkMessage + "code = %1, action = %2" , code[1], action[1]);
    *f << ++t << " " << code[1].toStdString().c_str()
              << " " << action[1].toStdString().c_str() << std::endl;

    DahuaEvent result;
    const QString internalName = code[1];
    result.typeId = manifest.eventTypeByInternalName(internalName);
    if (result.typeId.isEmpty())
        return nullopt;

    result.isActive = (action[1] == "Start" || action[1] == "Pulse");
    result.description = buildDescription(manifest, result);
    return result;
}

bool AttributesParser::isHartbeatEvent(const DahuaEvent& event)
{
    return event.typeId == kHeartbeat;
}

} // namespace dahua
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
