#include "device_analytics_settings_handler.h"

#include <nx/mediaserver/analytics/manager.h>
#include <nx/mediaserver/interactive_settings/json_engine.h>

namespace nx::mediaserver::rest {

using namespace nx::network;

DeviceAnalyticsSettingsHandler::DeviceAnalyticsSettingsHandler(QnMediaServerModule* serverModule):
    nx::mediaserver::ServerModuleAware(serverModule)
{
}

JsonRestResponse DeviceAnalyticsSettingsHandler::executeGet(const JsonRestRequest& request)
{
    const auto error = checkCommonInputParameters(request.params);
    if (error)
        return *error;

    const auto deviceId = request.params[kDeviceIdParameter];
    const auto engineId = request.params[kAnalyticsEngineIdParameter];

    const auto analyticsManager = serverModule()->analyticsManager();
    if (!analyticsManager)
    {
        const QString message("Unable to access analytics manager");
        NX_ERROR(this, message);
        return makeResponse(QnRestResult::InternalServerError, message);
    }

    const auto settings = analyticsManager->getSettings(deviceId, engineId);
    JsonRestResponse result(http::StatusCode::ok);
    result.json.setReply(variantMapToStringMap(settings));

    return result;
}

JsonRestResponse DeviceAnalyticsSettingsHandler::executePost(
    const JsonRestRequest& request,
    const QByteArray& body)
{
    bool success = false;
    auto requestJson = QJson::deserialized(body, QJsonObject(), &success);
    if (!success)
    {
        NX_WARNING(this, "Can't deserialize request JSON");
        return makeResponse(QnRestResult::Error::BadRequest, "Unable to deserialize reqeust");
    }

    auto parameters = requestJson.toVariantMap();
    const auto error = checkCommonInputParameters(parameters);
    if (error)
        return *error;

    if (!parameters.contains(kSettingsParameter))
    {
        NX_WARNING(this, "Missing parameter %1", kSettingsParameter);
        return makeResponse(QnRestResult::Error::MissingParameter, QStringList{kSettingsParameter});
    }

    auto analyticsManager = serverModule()->analyticsManager();
    if (!analyticsManager)
    {
        const QString message("Unable to access analytics manager");
        NX_ERROR(this, message);
        return makeResponse(QnRestResult::InternalServerError, message);
    }

    const auto deviceId = parameters[kDeviceIdParameter].toString();
    const auto engineId = parameters[kAnalyticsEngineIdParameter].toString();
    const auto settings = parameters[kSettingsParameter];

    analyticsManager->setSettings(
        deviceId,
        engineId,
        settings.value<QVariantMap>());

    const auto updatedSettings = analyticsManager->getSettings(deviceId, engineId);
    JsonRestResponse result(http::StatusCode::ok);
    result.json.setReply(variantMapToStringMap(updatedSettings));

    return result;
}

} // namespace nx::mediaserver::rest
