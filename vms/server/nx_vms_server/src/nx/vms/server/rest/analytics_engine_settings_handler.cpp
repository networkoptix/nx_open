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

RestResponse makeSettingsResponse(
    const nx::vms::server::resource::AnalyticsEngineResourcePtr& engine)
{
    nx::vms::api::analytics::SettingsResponse response;
    response.values = QJsonObject::fromVariantMap(engine->settingsValues());

    const auto parentPlugin = engine->plugin();
    if (!NX_ASSERT(parentPlugin))
    {
        return RestResponse::error(
            QnRestResult::Error::InternalServerError,
            lm("Unable to access the parent Plugin of the Engine, %1").args(engine));
    }

    response.model = parentPlugin->manifest().engineSettingsModel;
    return RestResponse::reply(response);
}

} // namespace

AnalyticsEngineSettingsHandler::AnalyticsEngineSettingsHandler(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

RestResponse AnalyticsEngineSettingsHandler::executeGet(const RestRequest& request)
{
    if (!request.param(kAnalyticsEngineIdParameter))
    {
        NX_WARNING(this, "Missing required parameter 'analyticsEngineId'");
        return RestResponse::error(
            QnRestResult::Error::MissingParameter, kAnalyticsEngineIdParameter);
    }

    const auto engineId = request.paramOr(kAnalyticsEngineIdParameter);
    auto engine = sdk_support::find<resource::AnalyticsEngineResource>(serverModule(), engineId);
    if (!engine)
    {
        NX_WARNING(this, "Can't find engine with id %1", engineId);
        return RestResponse::error(
            QnRestResult::Error::CantProcessRequest,
            lm("Unable to find analytics engine with id %1").args(engineId));
    }

    return makeSettingsResponse(engine);
}

RestResponse AnalyticsEngineSettingsHandler::executePost(const RestRequest& request)
{
    if (!request.param(kAnalyticsEngineIdParameter))
    {
        NX_WARNING(this, "Missing required parameter 'analyticsEngineId'");
        return RestResponse::error(
            QnRestResult::Error::MissingParameter, kAnalyticsEngineIdParameter);
    }

    const auto settings = request.parseContent<QJsonObject>();
    if (!settings)
    {
        const QString message("Unable to deserialize request");
        NX_WARNING(this, message);
        return RestResponse::error(QnRestResult::Error::BadRequest, message);
    }

    const auto engineId = request.paramOr(kAnalyticsEngineIdParameter);
    auto engine = sdk_support::find<resource::AnalyticsEngineResource>(serverModule(), engineId);
    if (!engine)
    {
        NX_WARNING(this, "Can't find engine with id %1", engineId);
        return RestResponse::error(QnRestResult::Error::CantProcessRequest,
            lm("Unable to find analytics engine with id %1").args(engineId));
    }

    engine->setSettingsValues(settings->toVariantMap());
    engine->saveProperties();

    return makeSettingsResponse(engine);
}

} // namespace nx::vms::server::rest
