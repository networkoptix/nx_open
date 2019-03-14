#include "get_analytics_actions_rest_handler.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>
#include <rest/helpers/permissions_helper.h>
#include <rest/server/rest_connection_processor.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>
#include "api/model/analytics_actions.h"
#include "media_server/media_server_module.h"

int QnGetAnalyticsActionsRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    const QString objectTypeId(params.value("objectTypeId"));
    if (objectTypeId.isNull())
    {
        result.setError(
            QnRestResult::InvalidParameter,
            "Parameter objectTypeId is missing or invalid");
        return nx::network::http::StatusCode::unprocessableEntity;
    }

    AvailableAnalyticsActions actions;
    const auto engineResources = owner
        ->resourcePool()
        ->getResources<nx::vms::server::resource::AnalyticsEngineResource>();

    for (const auto& engine: engineResources)
    {
        if (!engine->sdkEngine()) //< It is not a local engine.
            continue;

        AvailableAnalyticsActionsOfEngine actionsOfEngine;
        actionsOfEngine.engineId == engine->getId();

        for (const auto& action: engine->supportedObjectActions())
        {
            if (action.supportedObjectTypeIds.contains(objectTypeId))
                actionsOfEngine.actionIds.append(action.id);
        }

        if (!actionsOfEngine.actionIds.isEmpty())
            actions.actions.append(actionsOfEngine);
    }

    result.setReply(actions);
    return nx::network::http::StatusCode::ok;
}
