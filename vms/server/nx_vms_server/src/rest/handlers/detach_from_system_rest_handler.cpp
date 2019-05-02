#include "detach_from_system_rest_handler.h"

#include <api/global_settings.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <media_server/serverutil.h>
#include <nx/cloud/db/api/connection.h>
#include <nx/network/http/http_types.h>
#include <nx/network/rest/nx_network_rest_ini.h>
#include <nx/utils/log/log.h>
#include <nx/vms/cloud_integration/cloud_connection_manager.h>
#include <nx/vms/utils/detach_server_processor.h>
#include <nx/vms/utils/vms_utils.h>
#include <rest/helpers/permissions_helper.h>
#include <rest/server/rest_connection_processor.h>
#include <utils/common/app_info.h>

#include "private/multiserver_request_helper.h"

namespace nx::vms::server {

DetachFromSystemRestHandler::DetachFromSystemRestHandler(
    QnMediaServerModule* serverModule,
    nx::vms::cloud_integration::CloudConnectionManager* const cloudConnectionManager,
    ec2::AbstractTransactionMessageBus* messageBus)
    :
    ServerModuleAware(serverModule),
    m_cloudConnectionManager(cloudConnectionManager),
    m_messageBus(messageBus)
{
}

nx::network::rest::Response DetachFromSystemRestHandler::executeGet(
    const nx::network::rest::Request& request)
{
    if (!nx::network::rest::ini().allowModificationsViaGetMethod)
        return nx::network::rest::Response::error(nx::network::rest::Result::Forbidden);

    return executePost(request);
}

nx::network::rest::Response DetachFromSystemRestHandler::executePost(
    const nx::network::rest::Request& request)
{
    auto data = request.parseContentOrThrow<CurrentPasswordData>();
    nx::network::rest::JsonResult result;
    const auto status = execute(data, request.owner, result);
    auto response = nx::network::rest::Response::result(result);
    response.statusCode = status;
    return response;
}

nx::network::http::StatusCode::Value DetachFromSystemRestHandler::execute(
    CurrentPasswordData data,
    const QnRestConnectionProcessor* owner,
    nx::network::rest::JsonResult& result)
{
    if (!detail::verifyPasswordOrSetError(owner, data.currentPassword, &result))
        return nx::network::http::StatusCode::ok;

    using namespace nx::cloud::db;
    const Qn::UserAccessData& accessRights = owner->accessRights();
    NX_DEBUG(this, lm("Detaching server from system started."));

    if (QnPermissionsHelper::isSafeMode(serverModule()))
    {
        NX_WARNING(this, lm("Cannot detach server from system while in safe mode."));
        return (nx::network::http::StatusCode::Value) QnPermissionsHelper::safeModeError(result);
    }
    if (!QnPermissionsHelper::hasOwnerPermissions(owner->resourcePool(), accessRights))
    {
        NX_WARNING(this, lm("Cannot detach from system. Owner permissions are required."));
        return (nx::network::http::StatusCode::Value) QnPermissionsHelper::notOwnerError(result);
    }

    nx::vms::server::Utils::dropConnectionsToRemotePeers(m_messageBus);
    nx::vms::utils::DetachServerProcessor detachServerProcessor(
        owner->commonModule(),
        m_cloudConnectionManager);

    const auto resultCode = detachServerProcessor.detachServer(&result);
    nx::vms::server::Utils::resumeConnectionsToRemotePeers(m_messageBus);
    return resultCode;
}

} // namespace nx::vms::server
