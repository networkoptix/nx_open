#include "device_analytics_settings_handler.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <api/helpers/camera_id_helper.h>
#include <media_server/media_server_module.h>

#include <nx/utils/log/log.h>
#include <nx/mediaserver/rest/parameter_names.h>
#include <nx/mediaserver/rest/utils.h>
#include <nx/mediaserver/sdk_support/utils.h>
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
    if (const auto& error = checkCommonInputParameters(request.params))
        return *error;

    const auto analyticsManager = serverModule()->analyticsManager();
    if (!analyticsManager)
    {
        const QString message("Unable to access analytics manager");
        NX_ERROR(this, message);
        return makeResponse(QnRestResult::InternalServerError, message);
    }

    const QString& deviceId = request.params[kDeviceIdParameter];
    const QString& engineId = request.params[kAnalyticsEngineIdParameter];

    const QVariantMap& settings = analyticsManager->getSettings(deviceId, engineId);
    JsonRestResponse result(http::StatusCode::ok);
    result.json.setReply(QJsonObject::fromVariantMap(settings));

    return result;
}

JsonRestResponse DeviceAnalyticsSettingsHandler::executePost(
    const JsonRestRequest& request,
    const QByteArray& body)
{
    if (const auto& error = checkCommonInputParameters(request.params))
        return *error;

    bool success = false;
    const auto& settings = QJson::deserialized(body, QJsonObject(), &success);
    if (!success)
    {
        const QString message("Unable to deserialize request");
        NX_WARNING(this, message);
        return makeResponse(QnRestResult::Error::BadRequest, message);
    }

    auto analyticsManager = serverModule()->analyticsManager();
    if (!analyticsManager)
    {
        const QString message("Unable to access analytics manager");
        NX_ERROR(this, message);
        return makeResponse(QnRestResult::InternalServerError, message);
    }

    const QString& deviceId = request.params[kDeviceIdParameter];
    const QString& engineId = request.params[kAnalyticsEngineIdParameter];

    analyticsManager->setSettings(deviceId, engineId, settings.toVariantMap());

    const QVariantMap& updatedSettings = analyticsManager->getSettings(deviceId, engineId);
    JsonRestResponse result(http::StatusCode::ok);
    result.json.setReply(QJsonObject::fromVariantMap(updatedSettings));

    return result;
}

std::optional<JsonRestResponse> DeviceAnalyticsSettingsHandler::checkCommonInputParameters(
    const QnRequestParams& parameters) const
{
    if (!parameters.contains(kDeviceIdParameter))
    {
        NX_WARNING(this, "Missing parameter %1", kDeviceIdParameter);
        return makeResponse(
            QnRestResult::Error::MissingParameter,
            QStringList{kDeviceIdParameter});
    }

    if (!parameters.contains(kAnalyticsEngineIdParameter))
    {
        NX_WARNING(this, "Missing parameter %1", kAnalyticsEngineIdParameter);
        return makeResponse(
            QnRestResult::Error::MissingParameter,
            QStringList{kAnalyticsEngineIdParameter});
    }

    const QString& deviceId = parameters[kDeviceIdParameter];
    const QString& engineId = parameters[kAnalyticsEngineIdParameter];

    const auto device = nx::camera_id_helper::findCameraByFlexibleId(
        serverModule()->resourcePool(),
        deviceId);

    if (!device)
    {
        const auto message = lm("Unable to find device by id %1").args(deviceId);
        NX_WARNING(this, message);
        return makeResponse(QnRestResult::Error::CantProcessRequest, message);
    }

    if (device->flags().testFlag(Qn::foreigner))
    {
        const auto message =
            lm("Wrong server. Device belongs to the server %1, current server: %2").args(
                device->getParentId(), moduleGUID());

        NX_WARNING(this, message);
        return makeResponse(QnRestResult::Error::CantProcessRequest, message);
    }

    const auto engine = sdk_support::find<resource::AnalyticsEngineResource>(
        serverModule(), engineId);

    if (!engine)
    {
        const auto message = lm("Unable to find analytics engine by id %1").args(engineId);
        NX_WARNING(this, message);
        return makeResponse(QnRestResult::Error::CantProcessRequest, message);
    }

    return std::nullopt;
}

} // namespace nx::mediaserver::rest
