#include "device_analytics_settings_handler.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <api/helpers/camera_id_helper.h>
#include <media_server/media_server_module.h>

#include <nx/utils/log/log.h>
#include <nx/vms/server/rest/parameter_names.h>
#include <nx/vms/server/sdk_support/utils.h>
#include <nx/vms/server/analytics/manager.h>
#include <nx/vms/server/interactive_settings/json_engine.h>

#include <nx/vms/api/analytics/settings_response.h>

namespace nx::vms::server::rest {

using namespace nx::network;

DeviceAnalyticsSettingsHandler::DeviceAnalyticsSettingsHandler(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

RestResponse DeviceAnalyticsSettingsHandler::executeGet(const RestRequest& request)
{
    if (const auto& error = checkCommonInputParameters(request.params()))
        return *error;

    const auto analyticsManager = serverModule()->analyticsManager();
    if (!analyticsManager)
    {
        const QString message("Unable to access analytics manager");
        NX_ERROR(this, message);
        return RestResponse::error(QnRestResult::InternalServerError, message);
    }

    const QString& deviceId = request.paramOr(kDeviceIdParameter);
    const QString& engineId = request.paramOr(kAnalyticsEngineIdParameter);

    return makeSettingsResponse(analyticsManager, engineId, deviceId);
}

RestResponse DeviceAnalyticsSettingsHandler::executePost(const RestRequest& request)
{
    if (const auto& error = checkCommonInputParameters(request.params()))
        return *error;

    const auto& settings = request.parseContent<QJsonObject>();
    if (!settings)
    {
        const QString message("Unable to deserialize request");
        NX_WARNING(this, message);
        return RestResponse::error(QnRestResult::Error::BadRequest, message);
    }

    auto analyticsManager = serverModule()->analyticsManager();
    if (!analyticsManager)
    {
        const QString message("Unable to access analytics manager");
        NX_ERROR(this, message);
        return RestResponse::error(QnRestResult::InternalServerError, message);
    }

    const QString& deviceId = request.paramOr(kDeviceIdParameter);
    const QString& engineId = request.paramOr(kAnalyticsEngineIdParameter);

    analyticsManager->setSettings(deviceId, engineId, settings->toVariantMap());
    return makeSettingsResponse(analyticsManager, engineId, deviceId);
}

std::optional<RestResponse> DeviceAnalyticsSettingsHandler::checkCommonInputParameters(
    const QnRequestParams& parameters) const
{
    if (!parameters.contains(kDeviceIdParameter))
    {
        NX_WARNING(this, "Missing parameter %1", kDeviceIdParameter);
        return RestResponse::error(
            QnRestResult::Error::MissingParameter, kDeviceIdParameter);
    }

    if (!parameters.contains(kAnalyticsEngineIdParameter))
    {
        NX_WARNING(this, "Missing parameter %1", kAnalyticsEngineIdParameter);
        return RestResponse::error(
            QnRestResult::Error::MissingParameter, kAnalyticsEngineIdParameter);
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
        return RestResponse::error(QnRestResult::Error::CantProcessRequest, message);
    }

    if (device->flags().testFlag(Qn::foreigner))
    {
        const auto message =
            lm("Wrong server. Device belongs to the server %1, current server: %2").args(
                device->getParentId(), moduleGUID());

        NX_WARNING(this, message);
        return RestResponse::error(QnRestResult::Error::CantProcessRequest, message);
    }

    const auto engine = sdk_support::find<resource::AnalyticsEngineResource>(
        serverModule(), engineId);

    if (!engine)
    {
        const auto message = lm("Unable to find analytics engine by id %1").args(engineId);
        NX_WARNING(this, message);
        return RestResponse::error(QnRestResult::Error::CantProcessRequest, message);
    }

    return std::nullopt;
}

std::optional<QJsonObject> DeviceAnalyticsSettingsHandler::deviceAgentSettingsModel(
    const QString& engineId) const
{
    const auto engine = sdk_support::find<resource::AnalyticsEngineResource>(
        serverModule(), engineId);

    if (!engine)
    {
        NX_WARNING(this, "Unable to find analytics engine by id %1", engineId);
        return std::nullopt;
    }

    return engine->manifest().deviceAgentSettingsModel;
}

RestResponse DeviceAnalyticsSettingsHandler::makeSettingsResponse(
    const nx::vms::server::analytics::Manager* analyticsManager,
    const QString& engineId,
    const QString& deviceId) const
{
    NX_ASSERT(analyticsManager);
    if (!analyticsManager)
    {
        return RestResponse::error(
            QnRestResult::InternalServerError,
            "Unable to access an analytics manager");
    }

    nx::vms::api::analytics::SettingsResponse response;

    const QVariantMap& settings = analyticsManager->getSettings(deviceId, engineId);
    response.values = QJsonObject::fromVariantMap(settings);

    const auto settingsModel = deviceAgentSettingsModel(engineId);
    if (!settingsModel)
    {
        const auto message =
            lm("Unable to find DeviceAgent settings model for the Engine with id %1")
            .args(engineId);

        return RestResponse::error(QnRestResult::Error::CantProcessRequest, message);
    }

    response.model = *settingsModel;
    return RestResponse::reply(response);
}

} // namespace nx::vms::server::rest
