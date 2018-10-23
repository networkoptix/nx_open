#include "device_analytics_settings_handler.h"
#include "parameter_names.h"
#include "utils.h"

#include <core/resource/camera_resource.h>

#include <api/helpers/camera_id_helper.h>
#include <media_server/media_server_module.h>
#include <core/resource_management/resource_pool.h>

#include <nx/mediaserver/sdk_support/utils.h>
#include <nx/mediaserver/analytics/manager.h>
#include <nx/mediaserver/interactive_settings/json_engine.h>

namespace nx::mediaserver::rest {

using namespace nx::network;

DeviceAnalyticsSettingsHandler::DeviceAnalyticsSettingsHandler(QnMediaServerModule* serverModule):
    nx::mediaserver::ServerModuleAware(serverModule)
{
    // TODO: #dmishin implement
}

JsonRestResponse DeviceAnalyticsSettingsHandler::executeGet(const JsonRestRequest& request)
{
    if (!request.params.contains(kDeviceIdParameter))
    {
        NX_WARNING(this, "Missing parameter %1", kDeviceIdParameter);
        return makeResponse(QnRestResult::Error::MissingParameter, QStringList{kDeviceIdParameter});
    }

    if (!request.params.contains(kAnalyticsEngineIdParameter))
    {
        NX_WARNING(this, "Missing parameter %1", kAnalyticsEngineIdParameter);
        return makeResponse(QnRestResult::Error::MissingParameter, QStringList{kDeviceIdParameter});
    }

    const auto deviceId = request.params[kDeviceIdParameter];
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
            lm("Wrong server. Device belongs to the server %1, current server: %2")
                .args(device->getParentId(), moduleGUID());

        NX_WARNING(this, message);
        return makeResponse(QnRestResult::Error::CantProcessRequest, message);
    }

    const auto engineId = request.params[kAnalyticsEngineIdParameter];
    const auto engine = sdk_support::find<resource::AnalyticsEngineResource>(
        serverModule(), engineId);

    if (!engine)
    {
        const auto message = lm("Unable to find analytics engine by id %1").args(engineId);
        NX_WARNING(this, message);
        return makeResponse(QnRestResult::Error::CantProcessRequest, message);
    }

    const auto analyticsManager = serverModule()->analyticsManager();
    if (!analyticsManager)
    {
        const QString message("Unable to access analytics manager");
        NX_ERROR(this, message);
        return makeResponse(QnRestResult::InternalServerError, message);
    }

    const auto settings = analyticsManager->getSettings(deviceId, engineId);
    JsonRestResponse result(http::StatusCode::ok);
    result.json.setReply(QJsonObject::fromVariantMap(settings));

    return result;
}

JsonRestResponse DeviceAnalyticsSettingsHandler::executePost(
    const JsonRestRequest& request,
    const QByteArray& body)
{
    // TODO: #dmishin implement
    auto analyticsManager = serverModule()->analyticsManager();

    const auto deviceId = request.params[kDeviceIdParameter];
    const auto engineId = request.params[kAnalyticsEngineIdParameter];
    const auto settings = request.params[kSettingsParameter];

    auto document = QJsonDocument::fromBinaryData(settings.toUtf8());
    auto settingsMap = document.object().toVariantMap();

    analyticsManager->setSettings(deviceId, engineId, settingsMap);

    return JsonRestResponse(http::StatusCode::ok);
}

} // namespace nx::mediaserver::rest
