#include <array>
#include <iostream>
#include <fstream>

#include <QtCore/QByteArray>

#include <nx/utils/literal.h>
#include <nx/utils/log/log_main.h>

#include "string_helper.h"
#include "parser.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace dahua {

namespace {

static const QString kParseErrorMessage("Dahua event message parse error: ");
static const QString kParseOkMessage("Dahua event message parsing: ");

static const QString kParseDetailsErrorMessage = kParseErrorMessage + "event details = \"%1\"";
static const QString kParseDetailsOkMessage = kParseOkMessage + "event details = \"%1\"";

static const QString kHeartbeat("Heartbeat");
static const Event kHeartbeatEvent{ kHeartbeat };

}

std::vector<QString> Parser::parseSupportedEventsMessage(const QByteArray& content)
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

std::optional<Event> Parser::parseEventMessage(const QByteArray& content,
    const EngineManifest& engineManifest)
{
    using namespace nx::sdk::analytics;
    using std::nullopt;
    static std::ofstream* f = new std::ofstream("D:\\events.txt");
    static int t = 0;

    QString contentAsString(content);
    contentAsString.replace("\r\n", "\n");
    const QStringList Lines = contentAsString.split('\n');

    NX_VERBOSE(typeid(Parser),
        kParseOkMessage + "content string = " + contentAsString);

    if (Lines.isEmpty())
    {
        NX_DEBUG(typeid(Parser),
            kParseErrorMessage + "content string has no data");
        return nullopt;
    }

    contentAsString = Lines[0];
    // contentAsString should contain something like "Code=NewFile;action=Pulse;index=0;data={"

    const QStringList eventDetails = contentAsString.split(";");
    if (eventDetails.isEmpty())
    {
        NX_DEBUG(typeid(Parser), kParseDetailsErrorMessage, contentAsString);
        return nullopt;
    }

    if (eventDetails[0] == kHeartbeat)
    {
        NX_VERBOSE(typeid(Parser), kParseOkMessage + "heartbeat message received");
        *f << ++t << " " << eventDetails[0].toStdString().c_str() << std::endl;
        return kHeartbeatEvent;
    }

    if (eventDetails.size() < 2)
    {
        // At least two pieces of event information needed: name and action (start/stop/pulse)
        NX_DEBUG(typeid(Parser), kParseDetailsErrorMessage, contentAsString);
        return nullopt;
    }

    const QStringList code = eventDetails[0].split("=");
    const QStringList action = eventDetails[1].split("=");

    if (code.size() != 2 || code[0] != "Code" || action.size() != 2 || action[0] != "action")
    {
        NX_DEBUG(typeid(Parser), kParseDetailsErrorMessage, contentAsString);
        return nullopt;
    }

    NX_VERBOSE(typeid(Parser),
        kParseOkMessage + "code = %1, action = %2" , code[1], action[1]);
    *f << ++t << " " << code[1].toStdString().c_str()
              << " " << action[1].toStdString().c_str() << std::endl;

    Event result;
    const QString internalName = code[1];
    result.typeId = engineManifest.eventTypeByInternalName(internalName);
    if (result.typeId.isEmpty())
        return nullopt;

    result.isActive = (action[1] == "Start" || action[1] == "Pulse");
    result.description = buildDescription(engineManifest, result);
    return result;
}

bool Parser::isHartbeatEvent(const Event& event)
{
    return event.typeId == kHeartbeat;
}

} // namespace dahua
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
