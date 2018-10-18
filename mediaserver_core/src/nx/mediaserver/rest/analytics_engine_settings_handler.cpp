#include "analytics_engine_settings_handler.h"
#include "parameter_names.h"

#include <api/helpers/camera_id_helper.h>
#include <media_server/media_server_module.h>
#include <core/resource_management/resource_pool.h>

#include <nx/mediaserver/resource/analytics_plugin_resource.h>
#include <nx/mediaserver/resource/analytics_engine_resource.h>

#include <nx/mediaserver/sdk_support/utils.h>

namespace nx::mediaserver::rest {

using namespace nx::network;

namespace {

QVariantMap jsonToSettings(const QString& jsonString)
{
    return QJsonDocument::fromBinaryData(jsonString.toUtf8()).object().toVariantMap();
}

} // namespace


AnalyticsEngineSettingsHandler::AnalyticsEngineSettingsHandler(QnMediaServerModule* serverModule):
    nx::mediaserver::ServerModuleAware(serverModule)
{
}

JsonRestResponse AnalyticsEngineSettingsHandler::executeGet(const JsonRestRequest& request)
{
    // TODO: #dmishin requireParameter?
    const auto engineId = request.params[kAnalyticsEngineIdParameter];
    auto engine = sdk_support::find<resource::AnalyticsEngineResource>(serverModule(), engineId);
    if (!engine)
        return JsonRestResponse(http::StatusCode::badRequest);

    JsonRestResponse response(http::StatusCode::ok);
    response.json.setReply(QJsonObject::fromVariantMap(engine->settingsValues()));
    return response;
}

JsonRestResponse AnalyticsEngineSettingsHandler::executePost(
    const JsonRestRequest& request,
    const QByteArray& body)
{
    // TODO: #dmishin requireParameter?
    const auto engineId = request.params[kAnalyticsEngineIdParameter];
    auto engine = sdk_support::find<resource::AnalyticsEngineResource>(serverModule(), engineId);
    if (!engine)
    {
        NX_WARNING(this, "Can't find engine with id %1", engineId);
        return JsonRestResponse(http::StatusCode::badRequest);
    }

    auto settings = jsonToSettings(request.params[kSettingsParameter]);
    engine->setSettingsValues(settings);

    return JsonRestResponse(http::StatusCode::ok);
}

} // namespace nx::mediaserver::rest
