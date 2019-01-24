#include <array>

#include <QtCore/QByteArray>

#include <nx/utils/literal.h>
#include <nx/utils/log/log_main.h>

#include "string_helper.h"
#include "parser.h"

namespace nx::vms_server_plugins::analytics::dahua {

namespace {

static const QString kParseErrorMessage("Dahua event message parse error: ");
static const QString kParseOkMessage("Dahua event message parsing: ");

static const QString kParseDetailsErrorMessage = kParseErrorMessage + "event details = \"%1\"";
static const QString kParseDetailsOkMessage = kParseOkMessage + "event details = \"%1\"";

static const QString kHeartbeat("Heartbeat");
static const Event kHeartbeatEvent{ kHeartbeat };

} // namespace

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
    using std::nullopt;

    QString contentAsString(content);
    contentAsString.replace("\r\n", "\n");
    const QStringList lines = contentAsString.split('\n');

    NX_VERBOSE(typeid(Parser),
        kParseOkMessage + "content string = " + contentAsString);

    if (lines.isEmpty())
    {
        NX_DEBUG(typeid(Parser),
            kParseErrorMessage + "content string has no data");
        return nullopt;
    }

    contentAsString = lines[0];
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

    Event result;
    const QString internalName = code[1];
    result.typeId = engineManifest.eventTypeByInternalName(internalName);
    if (result.typeId.isEmpty())
        return nullopt;

    result.isActive = (action[1] == "Start" || action[1] == "Pulse");
    result.description = buildDescription(engineManifest, result);
    return result;
}

bool Parser::isHeartbeatEvent(const Event& event)
{
    return event.typeId == kHeartbeat;
}

} // namespace nx::vms_server_plugins::analytics::dahua
