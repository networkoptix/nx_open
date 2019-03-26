#include "director_json_interface.h"

#include <utility>

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

QJsonDocument formResponse(DirectorJsonInterface::ErrorCodes code)
{
    QJsonObject response;
    response.insert("error", int(code));
    response.insert("errorString", errorStrings[code]);

    return QJsonDocument(response);
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

// Command parsers & executors.
QJsonDocument executeQuit(QJsonObject request)
{
    auto force = treatAsBool(request.value("force"), true).first;
    Director::instance()->quit(force);
    return formResponse(DirectorJsonInterface::Ok);
}
} // namespace

namespace nx::vmx::client::desktop {

DirectorJsonInterface::DirectorJsonInterface(QObject* parent)
{
}

QJsonDocument DirectorJsonInterface::process(const QJsonDocument& jsonRequest)
{
    const auto& jsonObject = jsonRequest.object();
    const auto command = jsonObject.value("command").toString();
    if (command.isEmpty())
        return formResponse(BadCommand);

    // Main command dispatcher.
    if (command == "quit")
        return executeQuit(jsonObject);

    return formResponse(BadCommand);
}

} // namespace nx::vmx::client::desktop