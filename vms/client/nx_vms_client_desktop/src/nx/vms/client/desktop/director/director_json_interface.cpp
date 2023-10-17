// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "director_json_interface.h"

#include <utility>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <nx/metrics/application_metrics_storage.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/director/director.h>
#include <nx/vms/client/desktop/testkit/testkit.h>
#include <nx/vms/common/application_context.h>

using std::pair;

namespace {
using namespace nx::vms::client::desktop;

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
QJsonDocument executeQuit(Director* director, QJsonObject request)
{
    auto [force, isOK] = treatAsBool(request.value("force"), true);
    if (request.contains("force") && !isOK)
        return QJsonDocument(formResponseBase(DirectorJsonInterface::BadParameters,
            "Incorrect \"force\" parameter value."));

    director->quit(force);
    return QJsonDocument(formResponseBase(DirectorJsonInterface::Ok));
}

// Return frame stats command
QJsonDocument executeGetFrameTimePoints(Director* director, QJsonObject request)
{
    auto [from, isOK] = treatAsInt(request.value("from"), 0);
    if (request.contains("from") && !isOK)
        return QJsonDocument(formResponseBase(DirectorJsonInterface::BadParameters,
            "Incorrect \"from\" parameter value."));

    const auto framePoints = director->getFrameTimePoints();
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

QJsonDocument getServerRequestStatistics(QJsonObject request)
{
    auto metrics = nx::vms::common::appContext()->metrics();
    auto response = formResponseBase(DirectorJsonInterface::Ok);
    response.insert(
        metrics->totalServerRequests.name(), static_cast<qint64>(metrics->totalServerRequests()));
    return QJsonDocument(response);
}

QJsonDocument execute(QJsonObject request)
{
    if (!request.contains("source"))
    {
        return QJsonDocument(formResponseBase(DirectorJsonInterface::BadParameters,
            "Missing \"source\" parameter value."));
    }

    NX_DEBUG(NX_SCOPE_TAG, "Execute script: %1", request.value("source").toString());

    if (auto testkit = testkit::TestKit::instance())
        return QJsonDocument(testkit->execute(request.value("source").toString()));

    return QJsonDocument(formResponseBase(
        DirectorJsonInterface::BadParameters,
        "Testing is not supported."));
}

} // namespace

namespace nx::vms::client::desktop {

DirectorJsonInterface::DirectorJsonInterface(Director* director, QObject* parent):
    QObject(parent),
    m_director(director)
{
}

QJsonDocument DirectorJsonInterface::process(const QJsonDocument& jsonRequest)
{
    const auto& jsonObject = jsonRequest.object();
    const auto command = jsonObject.value("command").toString();
    if (command.isEmpty())
        return QJsonDocument(formResponseBase(BadCommand));

    // Main command dispatcher.
    if (command == "quit")
        return executeQuit(m_director, jsonObject);
    if (command == "getFrameTimePoints")
        return executeGetFrameTimePoints(m_director, jsonObject);
    if (command == "execute")
        return execute(jsonObject);
    if (command == "statistics/requests")
        return getServerRequestStatistics(jsonObject);

    return QJsonDocument(formResponseBase(BadCommand));
}

} // namespace nx::vms::client::desktop
