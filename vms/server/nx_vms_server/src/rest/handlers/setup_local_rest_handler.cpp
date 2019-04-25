#include "setup_local_rest_handler.h"

#include <api/global_settings.h>
#include <api/model/setup_local_system_data.h>
#include <api/resource_property_adaptor.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <media_server/media_server_module.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/http/http_types.h>
#include <nx/network/rest/nx_network_rest_ini.h>
#include <nx/utils/log/log.h>
#include <nx/vms/utils/setup_system_processor.h>
#include <nx/vms/utils/vms_utils.h>
#include <rest/helpers/permissions_helper.h>
#include <rest/server/rest_connection_processor.h>
#include <server/host_system_password_synchronizer.h>

#include "system_settings_handler.h"

namespace nx::vms::server {

SetupLocalSystemRestHandler::SetupLocalSystemRestHandler(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule)
{
}

nx::network::rest::Response SetupLocalSystemRestHandler::executeGet(
    const nx::network::rest::Request& request)
{
    if (!nx::network::rest::ini().allowModificationsViaGetMethod)
        return nx::network::rest::Response::error(nx::network::rest::Result::Forbidden);

    return executePost(request);
}

nx::network::rest::Response SetupLocalSystemRestHandler::executePost(
    const nx::network::rest::Request& request)
{
    const auto data = request.parseContentOrThrow<SetupLocalSystemData>();
    nx::network::rest::JsonResult result;
    const auto status = execute(data, request.owner, result);
    auto response = nx::network::rest::Response::result(result);
    response.statusCode = status;
    return response;
}

nx::network::http::StatusCode::Value SetupLocalSystemRestHandler::execute(
    SetupLocalSystemData data,
    const QnRestConnectionProcessor* owner,
    nx::network::rest::JsonResult& result)
{
    if (QnPermissionsHelper::isSafeMode(serverModule()))
        return (nx::network::http::StatusCode::Value) QnPermissionsHelper::safeModeError(result);
    if (!QnPermissionsHelper::hasOwnerPermissions(owner->resourcePool(), owner->accessRights()))
        return (nx::network::http::StatusCode::Value) QnPermissionsHelper::notOwnerError(result);

    SystemSettingsProcessor systemSettingsProcessor(
        owner->commonModule(),
        owner->authSession());

    nx::vms::utils::SetupSystemProcessor setupSystemProcessor(owner->commonModule());
    setupSystemProcessor.setSystemSettingsProcessor(&systemSettingsProcessor);
    const auto resultCode = setupSystemProcessor.setupLocalSystem(
        owner->authSession(),
        data,
        &result);
    if (resultCode != nx::network::http::StatusCode::ok)
        return resultCode;

    if (auto admin = setupSystemProcessor.getModifiedLocalAdmin())
    {
        serverModule()->hostSystemPasswordSynchronizer()->
            syncLocalHostRootPasswordWithAdminIfNeeded(admin);
    }

    return resultCode;
}

} // namespace nx::vms::server
