#include "setup_cloud_rest_handler.h"

#include <api/app_server_connection.h>
#include <api/global_settings.h>
#include <api/model/password_data.h>
#include <api/resource_property_adaptor.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx/network/http/http_types.h>
#include <nx/network/rest/nx_network_rest_ini.h>
#include <nx/vms/cloud_integration/cloud_connection_manager.h>
#include <nx/vms/cloud_integration/vms_cloud_connection_processor.h>
#include <nx/vms/utils/vms_utils.h>
#include <rest/helpers/permissions_helper.h>
#include <rest/server/rest_connection_processor.h>

#include "save_cloud_system_credentials.h"
#include "system_settings_handler.h"

namespace nx::vms::server {

SetupCloudSystemRestHandler::SetupCloudSystemRestHandler(
    QnMediaServerModule* serverModule,
    cloud_integration::CloudManagerGroup* cloudManagerGroup)
    :
    ServerModuleAware(serverModule),
    m_cloudManagerGroup(cloudManagerGroup)
{
}

nx::network::rest::Response SetupCloudSystemRestHandler::executeGet(
    const nx::network::rest::Request& request)
{
    if (!nx::network::rest::ini().allowModificationsViaGetMethod)
        return nx::network::rest::Response::error(nx::network::rest::Result::Forbidden);

    return executePost(request);
}

nx::network::rest::Response SetupCloudSystemRestHandler::executePost(
    const nx::network::rest::Request& request)
{
    auto data = request.parseContentOrThrow<SetupCloudSystemData>();
    nx::network::rest::JsonResult result;
    const auto status = execute(data, request.owner, result);
    auto response = nx::network::rest::Response::result(result);
    response.statusCode = status;
    return response;
}

nx::network::http::StatusCode::Value SetupCloudSystemRestHandler::execute(
    SetupCloudSystemData data,
    const QnRestConnectionProcessor* owner,
    nx::network::rest::JsonResult& result)
{
    if (QnPermissionsHelper::isSafeMode(serverModule()))
        return (nx::network::http::StatusCode::Value) QnPermissionsHelper::safeModeError(result);
    if (!QnPermissionsHelper::hasOwnerPermissions(owner->resourcePool(), owner->accessRights()))
        return (nx::network::http::StatusCode::Value) QnPermissionsHelper::notOwnerError(result);

    nx::vms::cloud_integration::VmsCloudConnectionProcessor vmsCloudConnectionProcessor(
        owner->commonModule(),
        m_cloudManagerGroup);

    SystemSettingsProcessor systemSettingsProcessor(
        owner->commonModule(),
        owner->authSession());
    vmsCloudConnectionProcessor.setSystemSettingsProcessor(&systemSettingsProcessor);

    return vmsCloudConnectionProcessor.setupCloudSystem(
        owner->authSession(),
        data,
        &result);
}

} // namespace nx::vms::server
