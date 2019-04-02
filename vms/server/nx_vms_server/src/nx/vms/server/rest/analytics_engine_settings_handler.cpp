#include "analytics_engine_settings_handler.h"
#include "parameter_names.h"

#include <api/helpers/camera_id_helper.h>
#include <media_server/media_server_module.h>
#include <core/resource_management/resource_pool.h>

#include <nx/utils/log/log.h>

#include <nx/vms/server/resource/analytics_plugin_resource.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>

#include <nx/vms/server/sdk_support/utils.h>

#include <nx/vms/api/analytics/settings_response.h>

namespace nx::vms::server::rest {

using namespace nx::network;

namespace {

nx::network::rest::Response makeSettingsResponse(
    const nx::vms::server::resource::AnalyticsEngineResourcePtr& engine)
{
    nx::vms::api::analytics::SettingsResponse response;
    response.values = QJsonObject::fromVariantMap(engine->settingsValues());

    const auto parentPlugin = engine->plugin();
    if (!NX_ASSERT(parentPlugin))
    {
        return nx::network::rest::Response::error(
            QnRestResult::Error::InternalServerError,
            lm("Unable to access the parent Plugin of the Engine, %1").args(engine));
    }

    response.model = parentPlugin->manifest().engineSettingsModel;
    return nx::network::rest::Response::reply(response);
}

} // namespace

AnalyticsEngineSettingsHandler::AnalyticsEngineSettingsHandler(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

nx::network::rest::Response AnalyticsEngineSettingsHandler::executeGet(
    const nx::network::rest::Request& request)
{
    const auto engineId = request.paramOrThrow(kAnalyticsEngineIdParameter);
    auto engine = sdk_support::find<resource::AnalyticsEngineResource>(serverModule(), engineId);
    if (!engine)
    {
        NX_DEBUG(this, "Can't find engine with id %1", engineId);
        return nx::network::rest::Response::error(
            QnRestResult::Error::CantProcessRequest,
            lm("Unable to find analytics engine with id %1").args(engineId));
    }

    return makeSettingsResponse(engine);
}

nx::network::rest::Response AnalyticsEngineSettingsHandler::executePost(
    const nx::network::rest::Request& request)
{
    const auto settings = request.parseContentOrThrow<QJsonObject>();
    const auto engineId = request.paramOrThrow(kAnalyticsEngineIdParameter);
    auto engine = sdk_support::find<resource::AnalyticsEngineResource>(serverModule(), engineId);
    if (!engine)
    {
        NX_DEBUG(this, "Can't find engine with id %1", engineId);
        return nx::network::rest::Response::error(
            QnRestResult::Error::CantProcessRequest,
            lm("Unable to find analytics engine with id %1").args(engineId));
    }

    engine->setSettingsValues(settings.toVariantMap());
    engine->saveProperties();

    return makeSettingsResponse(engine);
}

} // namespace nx::vms::server::rest
