#include "analytics_engine_settings_handler.h"
#include "parameter_names.h"
#include "utils.h"

#include <api/helpers/camera_id_helper.h>
#include <media_server/media_server_module.h>
#include <core/resource_management/resource_pool.h>

#include <nx/mediaserver/resource/analytics_plugin_resource.h>
#include <nx/mediaserver/resource/analytics_engine_resource.h>

#include <nx/mediaserver/sdk_support/utils.h>

namespace nx::mediaserver::rest {

using namespace nx::network;

AnalyticsEngineSettingsHandler::AnalyticsEngineSettingsHandler(QnMediaServerModule* serverModule):
    nx::mediaserver::ServerModuleAware(serverModule)
{
}

JsonRestResponse AnalyticsEngineSettingsHandler::executeGet(const JsonRestRequest& request)
{
    // TODO: #dmishin requireParameter?
    if (!request.params.contains(kAnalyticsEngineIdParameter))
    {
        NX_WARNING(this, "Missing required parameter 'analyticsEngineId'");
        return makeResponse(
            QnRestResult::Error::MissingParameter,
            QStringList{"analyticsEngineId"});
    }

    const auto engineId = request.params[kAnalyticsEngineIdParameter];
    auto engine = sdk_support::find<resource::AnalyticsEngineResource>(serverModule(), engineId);
    if (!engine)
    {
        NX_WARNING(this, "Can't find engine with id %1", engineId);
        return makeResponse(
            QnRestResult::Error::CantProcessRequest,
            lm("Unable to find analytics engine with id %1").args(engineId));
    }

    JsonRestResponse response(http::StatusCode::ok);
    response.json.setReply(QJsonObject::fromVariantMap(engine->settingsValues()));
    return response;
}

JsonRestResponse AnalyticsEngineSettingsHandler::executePost(
    const JsonRestRequest& request,
    const QByteArray& body)
{
    // TODO: #dmishin requireParameter?
    bool success = false;
    auto requestJson = QJson::deserialized(body, QJsonObject(), &success);

    if (!success)
    {
        NX_WARNING(this, "Can't deserialize request JSON");
        return makeResponse(QnRestResult::Error::BadRequest, "Unable to deserialize reqeust");
    }

    auto parameters = requestJson.toVariantMap();
    if (!parameters.contains(kAnalyticsEngineIdParameter))
    {
        NX_WARNING(this, "Missing required parameter 'analyticsEngineId'");
        return makeResponse(
            QnRestResult::Error::MissingParameter,
            QStringList{"analyticsEngineId"});
    }

    if (!parameters.contains(kSettingsParameter))
    {
        NX_WARNING(this, "Missing required parameter 'settings'");
        return makeResponse(QnRestResult::Error::MissingParameter, QStringList{"settings"});
    }

    const auto engineId = parameters[kAnalyticsEngineIdParameter].toString();
    auto engine = sdk_support::find<resource::AnalyticsEngineResource>(serverModule(), engineId);
    if (!engine)
    {
        NX_WARNING(this, "Can't find engine with id %1", engineId);
        return makeResponse(QnRestResult::Error::CantProcessRequest,
            lm("Unable to find analytics engine with id %1").args(engineId));
    }

    auto settings = parameters[kSettingsParameter].value<QVariantMap>();
    engine->setSettingsValues(settings);
    engine->saveParams();

    JsonRestResponse response(http::StatusCode::ok);
    response.json.setReply(QJsonObject::fromVariantMap(engine->settingsValues()));

    return response;
}

} // namespace nx::mediaserver::rest
