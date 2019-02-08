#include "analytics_engine_settings_handler.h"
#include "parameter_names.h"
#include "utils.h"

#include <api/helpers/camera_id_helper.h>
#include <media_server/media_server_module.h>
#include <core/resource_management/resource_pool.h>

#include <nx/utils/log/log.h>

#include <nx/vms/server/resource/analytics_plugin_resource.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>

#include <nx/vms/server/sdk_support/utils.h>
#include <nx/vms/server/rest/utils.h>

namespace nx::vms::server::rest {

using namespace nx::network;

AnalyticsEngineSettingsHandler::AnalyticsEngineSettingsHandler(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

JsonRestResponse AnalyticsEngineSettingsHandler::executeGet(const JsonRestRequest& request)
{
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
    if (!request.params.contains(kAnalyticsEngineIdParameter))
    {
        NX_WARNING(this, "Missing required parameter 'analyticsEngineId'");
        return makeResponse(
            QnRestResult::Error::MissingParameter,
            QStringList{kAnalyticsEngineIdParameter});
    }

    bool success = false;
    const auto& settings = QJson::deserialized(body, QJsonObject(), &success);
    if (!success)
    {
        const QString message("Unable to deserialize request");
        NX_WARNING(this, message);
        return makeResponse(QnRestResult::Error::BadRequest, message);
    }

    const auto engineId = request.params[kAnalyticsEngineIdParameter];
    auto engine = sdk_support::find<resource::AnalyticsEngineResource>(serverModule(), engineId);
    if (!engine)
    {
        NX_WARNING(this, "Can't find engine with id %1", engineId);
        return makeResponse(QnRestResult::Error::CantProcessRequest,
            lm("Unable to find analytics engine with id %1").args(engineId));
    }

    engine->setSettingsValues(settings.toVariantMap());
    engine->saveProperties();

    JsonRestResponse response(http::StatusCode::ok);
    response.json.setReply(QJsonObject::fromVariantMap(engine->settingsValues()));

    return response;
}

} // namespace nx::vms::server::rest
