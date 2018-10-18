#include "device_analytics_settings_handler.h"
#include "parameter_names.h"

#include <core/resource/camera_resource.h>

#include <api/helpers/camera_id_helper.h>
#include <media_server/media_server_module.h>
#include <core/resource_management/resource_pool.h>

#include <nx/mediaserver/sdk_support/utils.h>
#include <nx/mediaserver/analytics/manager.h>
#include <nx/mediaserver/interactive_settings/json_engine.h>

namespace nx::mediaserver::rest {

using namespace nx::network;

namespace {

} // namespace

DeviceAnalyticsSettingsHandler::DeviceAnalyticsSettingsHandler(QnMediaServerModule* serverModule):
    nx::mediaserver::ServerModuleAware(serverModule)
{
    // TODO: #dmishin implement
}

JsonRestResponse DeviceAnalyticsSettingsHandler::executeGet(const JsonRestRequest& request)
{
    const auto deviceId = request.params[kDeviceIdParameter];
    auto device = nx::camera_id_helper::findCameraByFlexibleId(
        serverModule()->resourcePool(),
        deviceId);

    if (!device)
    {
        NX_WARNING(this, "Can't find device by id %1", deviceId);
        // TODO: #dmishin consider not found.
        return JsonRestResponse(http::StatusCode::badRequest);
    }

    if (device->flags().testFlag(Qn::foreigner))
    {
        NX_WARNING(this, "Device belongs to the server %1, got request on the server %2",
            device->getParentId(), moduleGUID());

        return JsonRestResponse(http::StatusCode::badRequest);
    }

    const auto engineId = request.params[kAnalyticsEngineIdParameter];
    auto engine = sdk_support::find<resource::AnalyticsEngineResource>(serverModule(), engineId);
    if (!engine)
    {
        NX_WARNING(this, "Can't find device by id %1", engineId);
        return JsonRestResponse(http::StatusCode::badRequest);
    }

    auto analyticsManager = serverModule()->analyticsManager();
    if (!analyticsManager)
    {
        NX_ERROR(this, "Can't access analytics manager");
        return JsonRestResponse(http::StatusCode::internalServerError);
    }

    auto settings = analyticsManager->getSettings(deviceId, engineId);
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
