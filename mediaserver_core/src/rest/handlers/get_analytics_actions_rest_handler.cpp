#include "get_analytics_actions_rest_handler.h"

#include <nx/mediaserver/metadata/manager_pool.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>
#include <rest/helpers/permissions_helper.h>
#include <rest/server/rest_connection_processor.h>
#include "api/model/analytics_actions.h"
#include "media_server/media_server_module.h"

using nx::sdk::metadata::Plugin;

QnGetAnalyticsActionsRestHandler::~QnGetAnalyticsActionsRestHandler()
{
}

int QnGetAnalyticsActionsRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    const QString objectTypeId(params.value("objectTypeId"));
    if (objectTypeId.isNull())
    {
        result.setError(QnRestResult::InvalidParameter, "Parameter objectTypeId is missing or invalid");
        return nx::network::http::StatusCode::invalidParameter;
    }

    AvailableAnalyticsActions actions;

    const auto& manifests = owner->commonModule()->currentServer()->analyticsDrivers();
    for (const auto& manifest: manifests)
    {
        AvailableAnalyticsActionsOfPlugin actionsOfPlugin;
        actionsOfPlugin.pluginId = manifest.pluginId;

        for (const auto& action: manifest.objectActions)
        {
            if (action.supportedObjectTypeIds.contains(objectTypeId))
                actionsOfPlugin.actionIds.append(action.id);
        }

        if (!actionsOfPlugin.actionIds.isEmpty())
            actions.actions.append(actionsOfPlugin);
    }

    result.setReply(actions);
    return nx::network::http::StatusCode::ok;
}
