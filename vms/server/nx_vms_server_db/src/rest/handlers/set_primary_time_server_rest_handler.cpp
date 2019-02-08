#include "set_primary_time_server_rest_handler.h"

#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>
#include <nx_ec/ec_api.h>
#include <nx/vms/time_sync/time_sync_manager.h>
#include <core/resource_management/resource_pool.h>
#include <api/global_settings.h>
#include <core/resource/media_server_resource.h>

namespace rest {
namespace handlers {

int SetPrimaryTimeServerRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    const QByteArray& body,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    auto timeSyncManager = owner->commonModule()->ec2Connection()->timeSyncManager();
    result = execute(
        timeSyncManager,
        owner->commonModule(),
        QJson::deserialized<nx::vms::api::IdData>(body).id);
    return nx::network::http::StatusCode::ok;
}

QnJsonRestResult SetPrimaryTimeServerRestHandler::execute(
    nx::vms::time_sync::AbstractTimeSyncManager* timeSyncManager,
    QnCommonModule* commonModule,
    const QnUuid& id)
{
    auto server = commonModule->resourcePool()->getResourceById<QnMediaServerResource>(id);
    QnJsonRestResult result;
    if (!server && !id.isNull())
    {
        result.setError(QnRestResult::InvalidParameter,
            lit("Mediaserver with Id '%1' is not found.").arg(id.toString()));
        return result;
    }

    auto settings = commonModule->globalSettings();
    settings->setPrimaryTimeServer(id);
    settings->synchronizeNow();
    return result;
}

} // namespace handlers
} // namespace rest
