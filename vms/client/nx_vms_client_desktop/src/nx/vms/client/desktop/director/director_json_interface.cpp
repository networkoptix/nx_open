#include "director_json_interface.h"

#include <utility>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>

#include <nx/vms/client/desktop/director/director.h>

using std::pair;

namespace {
using namespace nx::vmx::client::desktop;

// Error strings returned by JSON server.
const QMap<DirectorJsonInterface::ErrorCodes, QString> errorStrings = {
{ DirectorJsonInterface::Ok, ""},
{ DirectorJsonInterface::BadCommand, "Bad request or unknown command"},
{ DirectorJsonInterface::BadParameters, "Bad command parameters"},
};

QJsonObject formResponseBase(DirectorJsonInterface::ErrorCodes code, const QString& errorString = {})
{
    QJsonObject response;
    response.insert("error", int(code));
    response.insert("errorString", errorString.isEmpty() ? errorStrings[code] : errorString);

    return response;
}

// JSON value converters. JSON that comes from HTTP parameter values may have
// incorrect types, so we would like to have fuzzy converters.
// Return pair<value, conversion_success>.

pair<bool, bool> treatAsBool(const QJsonValue& value, bool defaultValue)
{
    if (value.isBool())
        return {value.toBool(), true};

    if (value.isDouble())
        return {value.toDouble() != 0, true};

    if (value.isString())
    {
        const auto str = value.toString();
        if (str.toLower() == "true")
            return {true, true};

        if (str.toLower() == "false")
            return {false, true};
        bool isNumber;
        const int number = str.toInt(&isNumber);
        if (isNumber)
            return {number != 0, true};
    }

    return {defaultValue, false};
}

pair<qint64, bool> treatAsInt(const QJsonValue& value, qint64 defaultValue)
{
    if (value.isDouble())
        return {value.toInt(), true};

    if (value.isString())
    {
        const auto str = value.toString();
        bool isNumber;
        const int number = str.toInt(&isNumber);
        if (isNumber)
            return {number, true};
    }

    return {defaultValue, false};
}

// Command parsers & executors.
// Quit command
QJsonDocument executeQuit(QJsonObject request)
{
    auto [force, isOK] = treatAsBool(request.value("force"), true);
    if (request.contains("force") && !isOK)
        return QJsonDocument(formResponseBase(DirectorJsonInterface::BadParameters,
            "Incorrect \"force\" parameter value."));

    Director::instance()->quit(force);
    return QJsonDocument(formResponseBase(DirectorJsonInterface::Ok));
}

// Return frame stats command
QJsonDocument executeGetFrameTimePoints(QJsonObject request)
{
    auto [from, isOK] = treatAsInt(request.value("from"), 0);
    if (request.contains("from") && !isOK)
        return QJsonDocument(formResponseBase(DirectorJsonInterface::BadParameters,
            "Incorrect \"from\" parameter value."));

    const auto framePoints = Director::instance()->getFrameTimePoints();
    QJsonArray jsonFramePoints;
    for (auto timePoint: framePoints)
    {
        if (timePoint >= from)
            jsonFramePoints.push_back(timePoint);
    }

    auto response = formResponseBase(DirectorJsonInterface::Ok);
    response.insert("framePoints", jsonFramePoints);
    return QJsonDocument(response);
}

} // namespace

namespace nx::vmx::client::desktop {

DirectorJsonInterface::DirectorJsonInterface(QObject* parent)
{
}

QJsonDocument DirectorJsonInterface::process(const QJsonDocument& jsonRequest)
{
    NX_ASSERT(Director::instance(), "Director's life cycle must be greater than the current class");

    const auto& jsonObject = jsonRequest.object();
    const auto command = jsonObject.value("command").toString();
    if (command.isEmpty())
        return QJsonDocument(formResponseBase(BadCommand));

    // Main command dispatcher.
    if (command == "quit")
        return executeQuit(jsonObject);
    if (command == "getFrameTimePoints")
        return executeGetFrameTimePoints(jsonObject);

    return QJsonDocument(formResponseBase(BadCommand));
}

} // namespace nx::vmx::client::desktop