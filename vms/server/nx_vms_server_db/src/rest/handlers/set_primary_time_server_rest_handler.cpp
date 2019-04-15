#include "set_primary_time_server_rest_handler.h"

#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>
#include <nx_ec/ec_api.h>
#include <nx/vms/time_sync/time_sync_manager.h>
#include <core/resource_management/resource_pool.h>
#include <api/global_settings.h>
#include <core/resource/media_server_resource.h>

namespace rest::handlers {

nx::network::rest::Response SetPrimaryTimeServerRestHandler::executePost(
    const nx::network::rest::Request& request)
{
    const auto id = request.parseContentOrThrow<nx::vms::api::IdData>().id;
    const auto common = request.owner->commonModule();
    auto server = common->resourcePool()->getResourceById<QnMediaServerResource>(id);
    if (!server && !id.isNull())
    {
        return nx::network::rest::Response::error(
            nx::network::http::StatusCode::ok, QnRestResult::CantProcessRequest,
            lm("Server with Id '%1' is not found.").arg(id));
    }

    auto settings = common->globalSettings();
    settings->setPrimaryTimeServer(id);
    settings->synchronizeNow();
    return nx::network::rest::Response::result(QnJsonRestResult());
}

} // namespace rest::handlers
