#include <array>

#include <QtCore/QByteArray>

#include <nx/utils/literal.h>
#include <nx/utils/log/log_main.h>

#include "string_helper.h"
#include "parser.h"
#include <QtCore/QJsonDocument>

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

static QMap<QByteArray, QByteArray> splitMessage(const QByteArray& content)
{
    QMap<QByteArray, QByteArray> result;
    for (const auto& param: content.split(';'))
    {
        if (int pos = param.indexOf('='); pos > 0)
            result.insert(param.left(pos).trimmed(), param.mid(pos + 1).trimmed());
    }
    return result;
}

std::optional<Event> Parser::parseEventMessage(const QByteArray& content,
    const EngineManifest& engineManifest)
{
    #if 0
    // Content example.
    QByteArray content =
        R"json(
            Code=StayDetection;action=start;index=0;
            data=
            {
                "Object": { "BoundingBox": [2992,1136,4960,5192] },
                "Objects": [ 
                { "BoundingBox": [2992,1136,4960,5192] },
                { "BoundingBox": [4392,4136,6960,6512] }
                ],
                "DetectRegion": [ [1292,3469], [6535,3373]],
                "AreaID" : 2
            }
        )json";
    #endif

    if (content == kHeartbeat)
    {
        NX_VERBOSE(typeid(Parser), kParseOkMessage + "heartbeat message received");
        return kHeartbeatEvent;
    }
    else if (content.isEmpty())
    {
        NX_DEBUG(typeid(Parser), kParseErrorMessage + "content string has no data");
        return std::nullopt;
    }
    NX_VERBOSE(typeid(Parser), kParseOkMessage + "content string = " + content);

    const QMap<QByteArray, QByteArray> parameters = splitMessage(content);
    const QByteArray internalName = parameters["Code"];
    const QByteArray actionType = parameters["action"].toLower();
    const QByteArray jsonData = parameters["data"];

    if (internalName.isEmpty() || actionType.isEmpty())
    {
        NX_DEBUG(typeid(Parser), kParseDetailsErrorMessage, content);
        return std::nullopt;
    }

    NX_VERBOSE(typeid(Parser), kParseOkMessage + "code=%1, action=%2", internalName, actionType);

    Event result;
    result.typeId = engineManifest.eventTypeByInternalName(internalName);
    if (result.typeId.isEmpty())
    {
        NX_DEBUG(typeid(Parser), kParseErrorMessage + "Skip unknown event %1", internalName);
        return std::nullopt;
    }
    
    if (!jsonData.isEmpty())
    {
        QJsonParseError error;
        const auto json = QJsonDocument::fromJson(jsonData, &error);
        if (error.error == QJsonParseError::NoError)
        {
            const QJsonObject jsonObject = json.object();
            const auto& areaIdValue = jsonObject["AreaID"];
            if (!areaIdValue.isNull())
                result.region = areaIdValue.toInt();
        }
        else
        {
            NX_DEBUG(typeid(Parser), 
                kParseErrorMessage + "can't parse json. Error: %2", error.errorString());
        }
    }

    result.isActive = (actionType == "start" || actionType == "pulse");
    result.description = buildDescription(engineManifest, result);
    return result;
}

bool Parser::isHeartbeatEvent(const Event& event)
{
    return event.typeId == kHeartbeat;
}

} // namespace nx::vms_server_plugins::analytics::dahua
